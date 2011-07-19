/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <ctype.h>
#include "cutils/misc.h"
#include "cutils/properties.h"
#include <dirent.h>
#include <getopt.h>
#include <linux/input.h>
#include <signal.h>
#include <sys/limits.h>
#include <sys/reboot.h>
#include <termios.h>
#include <time.h>

#include "bootloader.h"
#include "common.h"
#include "extra-functions.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "minzip/Zip.h"
#include "recovery_ui.h"
#include "roots.h"
#include "ddftw.h"
#include "backstore.h"
#include "settings_file.h"

//kang system() from bionic/libc/unistd and rename it __system() so we can be even more hackish :)
#undef _PATH_BSHELL
#define _PATH_BSHELL "/sbin/sh"

extern char **environ;

int
__system(const char *command)
{
  pid_t pid;
	sig_t intsave, quitsave;
	sigset_t mask, omask;
	int pstat;
	char *argp[] = {"sh", "-c", NULL, NULL};

	if (!command)		/* just checking... */
		return(1);

	argp[2] = (char *)command;

	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &mask, &omask);
	switch (pid = vfork()) {
	case -1:			/* error */
		sigprocmask(SIG_SETMASK, &omask, NULL);
		return(-1);
	case 0:				/* child */
		sigprocmask(SIG_SETMASK, &omask, NULL);
		execve(_PATH_BSHELL, argp, environ);
    _exit(127);
  }

	intsave = (sig_t)  bsd_signal(SIGINT, SIG_IGN);
	quitsave = (sig_t) bsd_signal(SIGQUIT, SIG_IGN);
	pid = waitpid(pid, (int *)&pstat, 0);
	sigprocmask(SIG_SETMASK, &omask, NULL);
	(void)bsd_signal(SIGINT, intsave);
	(void)bsd_signal(SIGQUIT, quitsave);
	return (pid == -1 ? -1 : pstat);
}

static struct pid {
	struct pid *next;
	FILE *fp;
	pid_t pid;
} *pidlist;

FILE *
__popen(const char *program, const char *type)
{
	struct pid * volatile cur;
	FILE *iop;
	int pdes[2];
	pid_t pid;

	if ((*type != 'r' && *type != 'w') || type[1] != '\0') {
		errno = EINVAL;
		return (NULL);
	}

	if ((cur = malloc(sizeof(struct pid))) == NULL)
		return (NULL);

	if (pipe(pdes) < 0) {
		free(cur);
		return (NULL);
	}

	switch (pid = vfork()) {
	case -1:			/* Error. */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		free(cur);
		return (NULL);
		/* NOTREACHED */
	case 0:				/* Child. */
	    {
		struct pid *pcur;
		/*
		 * because vfork() instead of fork(), must leak FILE *,
		 * but luckily we are terminally headed for an execl()
		 */
		for (pcur = pidlist; pcur; pcur = pcur->next)
			close(fileno(pcur->fp));

		if (*type == 'r') {
			int tpdes1 = pdes[1];

			(void) close(pdes[0]);
			/*
			 * We must NOT modify pdes, due to the
			 * semantics of vfork.
			 */
			if (tpdes1 != STDOUT_FILENO) {
				(void)dup2(tpdes1, STDOUT_FILENO);
				(void)close(tpdes1);
				tpdes1 = STDOUT_FILENO;
			}
		} else {
			(void)close(pdes[1]);
			if (pdes[0] != STDIN_FILENO) {
				(void)dup2(pdes[0], STDIN_FILENO);
				(void)close(pdes[0]);
			}
		}
		execl(_PATH_BSHELL, "sh", "-c", program, (char *)NULL);
		_exit(127);
		/* NOTREACHED */
	    }
	}

	/* Parent; assume fdopen can't fail. */
	if (*type == 'r') {
		iop = fdopen(pdes[0], type);
		(void)close(pdes[1]);
	} else {
		iop = fdopen(pdes[1], type);
		(void)close(pdes[0]);
	}

	/* Link into list of file descriptors. */
	cur->fp = iop;
	cur->pid =  pid;
	cur->next = pidlist;
	pidlist = cur;

	return (iop);
}

/*
 * pclose --
 *	Pclose returns -1 if stream is not associated with a `popened' command,
 *	if already `pclosed', or waitpid returns an error.
 */
int
__pclose(FILE *iop)
{
	struct pid *cur, *last;
	int pstat;
	pid_t pid;

	/* Find the appropriate file pointer. */
	for (last = NULL, cur = pidlist; cur; last = cur, cur = cur->next)
		if (cur->fp == iop)
			break;

	if (cur == NULL)
		return (-1);

	(void)fclose(iop);

	do {
		pid = waitpid(cur->pid, &pstat, 0);
	} while (pid == -1 && errno == EINTR);

	/* Remove the entry from the linked list. */
	if (last == NULL)
		pidlist = cur->next;
	else
		last->next = cur->next;
	free(cur);

	return (pid == -1 ? -1 : pstat);
}

void get_device_id()
{
	FILE *fp;
	fp = __popen("cat /proc/cmdline | sed \"s/.*serialno=//\" | cut -d\" \" -f1", "r");
	if (fp == NULL)
	{
		LOGI("=> device id file not found.");
	} else 
	{
		fgets(device_id, 15, fp);
		int len = strlen(device_id);
		if (device_id[len-1] == '\n') {
			device_id[len-1] = 0;
		}
		LOGI("=> DEVICE_ID: %s\n", device_id);
	}
	__pclose(fp);
}

/*partial kangbang from system/vold
TODO: Currently only one mount is supported, defaulting
/mnt/sdcard to lun0 and anything else gets no love. Fix this.
*/
#ifndef CUSTOM_LUN_FILE
#define CUSTOM_LUN_FILE "/sys/devices/platform/usb_mass_storage/lun"
#endif

void usb_storage_toggle()
{
/*maybe make this a header instead?
    static char* headers[] = {  "Mounting USB as storage device",
                                "",
                                NULL
    };
*/
    ui_print("\nMounting USB as storage device...");

    int fd;
    Volume *vol = volume_for_path("/sdcard"); 
    if ((fd = open(CUSTOM_LUN_FILE"0/file", O_WRONLY)) < 0) {
        LOGE("Unable to open ums lunfile: (%s)", strerror(errno));
        return;
    }

    if ((write(fd, vol->device, strlen(vol->device)) < 0) &&
        (!vol->device2 || (write(fd, vol->device, strlen(vol->device2)) < 0))) {
        LOGE("Unable to write to ums lunfile: (%s)", strerror(errno));
        close(fd);
        return;

    } else {
        ui_clear_key_queue();
        ui_print("\nUSB as storage device mounted!\n");
        ui_print("\nPress Power to disable,");
        ui_print("\nand return to menu\n");

        for (;;) {
        	int key = ui_wait_key();
        	if (key == KEY_POWER) {
        		ui_print("\nDisabling USB as storage device...");

        		if ((fd = open(CUSTOM_LUN_FILE"0/file", O_WRONLY)) < 0) {
        			LOGE("Unable to open ums lunfile: (%s)", strerror(errno));
        			return;
        		}

        		char ch = 0;
        		if (write(fd, &ch, 1) < 0) {
        			LOGE("Unable to write to ums lunfile: (%s)", strerror(errno));
        			close(fd);
        			return;
        		}
        		ui_print("\nUSB as storage device unmounted!\n");
        		break;
        	}
        }
    }
}

char* zip_verify()
{
	char* tmp_set = (char*)malloc(40);
	strcpy(tmp_set, "[ ] Zip Signature Verification");
	if (is_true(tw_signed_zip_val) == 1) {
		tmp_set[1] = 'x';
	}
	return tmp_set;
}

char* reboot_after_flash()
{
	char* tmp_set = (char*)malloc(40);
	strcpy(tmp_set, "[ ] Reboot After Successful Flash");
	if (is_true(tw_reboot_after_flash_option) == 1) {
		tmp_set[1] = 'x';
	}
	return tmp_set;
}

void install_zip_menu()
{
	// INSTALL ZIP MENU
	#define ITEM_CHOOSE_ZIP           0
	#define ITEM_REBOOT_AFTER_FLASH   1
	#define ITEM_TOGGLE_SIG           2
	#define ITEM_ZIP_BACK		      3
	
    ui_set_background(BACKGROUND_ICON_FLASH_ZIP);
    static char* MENU_FLASH_HEADERS[] = {   "Flash zip From SD card:",
                                            NULL };

	char* MENU_INSTALL_ZIP[] = {  "--> Choose Zip To Flash",
	                              reboot_after_flash(),
								  zip_verify(),
	                              "<-- Back To Main Menu",
	                              NULL };

	char** headers = prepend_title(MENU_FLASH_HEADERS);

    inc_menu_loc(ITEM_ZIP_BACK);
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, MENU_INSTALL_ZIP, 0, 0);
        switch (chosen_item)
        {
            case ITEM_CHOOSE_ZIP:
            	;
                int status = sdcard_directory(tw_zip_location_val);
                ui_reset_progress();  // reset status bar so it doesnt run off the screen 
                if (status != INSTALL_SUCCESS) {
                	if (notError == 1) {
                        ui_set_background(BACKGROUND_ICON_ERROR);
                        ui_print("Installation aborted due to an error.\n");  
                	}
                } else if (!ui_text_visible()) {
                    return;  // reboot if logs aren't visible
                } else {
                    ui_print("\nInstall from sdcard complete.\n");
					if (is_true(tw_reboot_after_flash_option)) {
						ui_print("\nRebooting phone.\n");
						reboot(RB_AUTOBOOT);
						return;
					}
            	    if (!go_home) {
                        ui_print("\nInstall from sdcard complete.\n");
            	    }
                }
                break;
			case ITEM_REBOOT_AFTER_FLASH:
				if (is_true(tw_reboot_after_flash_option)) {
            		strcpy(tw_reboot_after_flash_option, "0");
            	} else {
            		strcpy(tw_reboot_after_flash_option, "1");
            	}
                write_s_file();
                break;
            case ITEM_TOGGLE_SIG:
            	if (is_true(tw_signed_zip_val)) {
            		strcpy(tw_signed_zip_val, "0");
            	} else {
            		strcpy(tw_signed_zip_val, "1");
            	}
                write_s_file();
                break;
            case ITEM_ZIP_BACK:
    	        dec_menu_loc();
                ui_set_background(BACKGROUND_ICON_MAIN);
                return;
        }
	    if (go_home) { 
	        dec_menu_loc();
	        return;
	    }
        break;
    }
	ui_end_menu();
    dec_menu_loc();
	install_zip_menu();
}

void wipe_dalvik_cache()
{
        ui_set_background(BACKGROUND_ICON_WIPE);
        ensure_path_mounted("/data");
        ensure_path_mounted("/cache");
        ui_print("\n-- Wiping Dalvik Cache Directories...\n");
        __system("rm -rf /data/dalvik-cache");
        ui_print("Cleaned: /data/dalvik-cache...\n");
        __system("rm -rf /cache/dalvik-cache");
        ui_print("Cleaned: /cache/dalvik-cache...\n");
        __system("rm -rf /cache/dc");
        ui_print("Cleaned: /cache/dc\n");

        struct stat st;
        if (0 != stat("/dev/block/mmcblk0p2", &st))
        {
            ui_print("sd-ext not present, skipping\n");
        } else {
            if (ensure_path_mounted("/sd-ext") == 0) {
                __system("rm -rf /sd-ext/dalvik-cache");
        	    ui_print("Cleaned: /sd-ext/dalvik-cache...\n");
            } else {
                ui_print("/dev/block/mmcblk0p2 exists but sd-ext not present, skipping\n");
            }
        }
        ensure_path_unmounted("/data");
        ui_print("-- Dalvik Cache Directories Wipe Complete!\n");
        ui_set_background(BACKGROUND_ICON_MAIN);
        if (!ui_text_visible()) return;
}

void reboot_menu()
{
	
	#define ITEM_SYSTEM      0
	#define ITEM_RECOVERY    1
	#define ITEM_BOOTLOADER  2
	#define ITEM_POWEROFF    3
	#define ITEM_BACKK		 4
	

    static char* MENU_REBOOT_HEADERS[] = {  "Reboot Menu:",
                                            NULL };
    
	// REBOOT MENU
	char* MENU_REBOOT[] = { "Reboot To System",
	                        "Reboot To Recovery",
	                        "Reboot To Bootloader",
	                        "Power Off",
	                        "<-Back To Main Menu",
	                        NULL };

    
    char** headers = prepend_title(MENU_REBOOT_HEADERS);
    
    inc_menu_loc(ITEM_BACKK);
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, MENU_REBOOT, 0, 0);
        switch (chosen_item)
        {
            case ITEM_RECOVERY:
                __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, "recovery");
                break;

            case ITEM_BOOTLOADER:
                __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, "bootloader");
                break;

            case ITEM_POWEROFF:
                __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL);
                break;

            case ITEM_SYSTEM:
                reboot(RB_AUTOBOOT);
                break;

            case ITEM_BACKK:
            	dec_menu_loc();
                return;
        }
	    if (go_home) { 
	        dec_menu_loc();
	        return;
	    }
    }
}

// BATTERY STATS
void wipe_battery_stats()
{
    ensure_path_mounted("/data");
    struct stat st;
    if (0 != stat("/data/system/batterystats.bin", &st))
    {
        ui_print("No Battery Stats Found. No Need To Wipe.\n");
    } else {
        ui_set_background(BACKGROUND_ICON_WIPE);
        remove("/data/system/batterystats.bin");
        ui_print("Cleared: Battery Stats...\n");
        ensure_path_unmounted("/data");
    }
}

// ROTATION SETTINGS
void wipe_rotate_data()
{
    ui_set_background(BACKGROUND_ICON_WIPE);
    ensure_path_mounted("/data");
    __system("rm -r /data/misc/akmd*");
    __system("rm -r /data/misc/rild*");
    ui_print("Cleared: Rotatation Data...\n");
    ensure_path_unmounted("/data");
}   


void advanced_menu()
{
	// ADVANCED MENU
	#define ITEM_REBOOT_MENU       0
	#define ITEM_FORMAT_MENU       1
	#define ITEM_BATTERY_STATS     2
	#define ITEM_ROTATE_DATA       3
	#define ITEM_ALL_SETTINGS      4
	#define ADVANCED_MENU_BACK     5

    static char* MENU_ADVANCED_HEADERS[] = { "Advanced Options",
                                              NULL };
    
	char* MENU_ADVANCED[] = { "Reboot Menu",
	                          "Format Menu",
	                          "Wipe Battery Stats",
	                          "Wipe Rotation Data",
	                          "Change twrp Settings",
	                          "<-Back To Main Menu",
	                          NULL };
	

    char** headers = prepend_title(MENU_ADVANCED_HEADERS);
    
    inc_menu_loc(ADVANCED_MENU_BACK);
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, MENU_ADVANCED, 0, 0);
        switch (chosen_item)
        {
            case ITEM_REBOOT_MENU:
                reboot_menu();
                break;

            case ITEM_FORMAT_MENU:
                format_menu();
                break;

            case ITEM_BATTERY_STATS:
                wipe_battery_stats();
                break;

            case ITEM_ROTATE_DATA:
                wipe_rotate_data();
                break;
            case ITEM_ALL_SETTINGS:
			    all_settings_menu();
				break;
            case ADVANCED_MENU_BACK:
            	dec_menu_loc();
            	return;
        }
	    if (go_home) { 
	        dec_menu_loc();
	        return;
	    }
    }
}

// kang'd this from recovery.c cuz there wasnt a recovery.h!
int
erase_volume(const char *volume) {
    ui_show_indeterminate_progress();
    ui_print("Formatting %s...\n", volume);

    if (strcmp(volume, "/cache") == 0) {
        // Any part of the log we'd copied to cache is now gone.
        // Reset the pointer so we copy from the beginning of the temp
        // log.
        tmplog_offset = 0;
    }

    return format_volume(volume);
}

// FORMAT MENU
void
format_menu()
{
	#define ITEM_FORMAT_CACHE       0
	#define ITEM_FORMAT_DATA        1
	#define ITEM_FORMAT_SDCARD      2
	#define ITEM_FORMAT_SYSTEM      3
	#define ITEM_FORMAT_BACK        4
	
	char* part_headers[] = {    "Format Menu",
                                "Choose Partition to Format: ",
                                NULL };
	
    char* part_items[] = {  "Format Cache (/cache)",
                            "Format Data (/data)",
                            "Format Sdcard (/sdcard)",
                            "Format System (/system)",
						    "<- Back To Advanced Menu",
						    NULL };

    char** headers = prepend_title(part_headers);
    
    inc_menu_loc(ITEM_FORMAT_BACK);
	for (;;)
	{
                ui_set_background(BACKGROUND_ICON_WIPE_CHOOSE);
		int chosen_item = get_menu_selection(headers, part_items, 0, 0);
		switch (chosen_item)
		{
			case ITEM_FORMAT_CACHE:
                confirm_format("CACHE", "/cache");
                break;
			case ITEM_FORMAT_DATA:
                confirm_format("DATA", "/data");
                break;
			case ITEM_FORMAT_SDCARD:
                confirm_format("SDCARD", "/sdcard");
                break;
			case ITEM_FORMAT_SYSTEM:
                confirm_format("SYSTEM", "/system");
                break;
			case ITEM_FORMAT_BACK:
            	dec_menu_loc();
                ui_set_background(BACKGROUND_ICON_MAIN);
				return;
		}
	    if (go_home) { 
    	        dec_menu_loc();
                ui_set_background(BACKGROUND_ICON_MAIN);
	        return;
	    }
	}
}

void
confirm_format(char* volume_name, char* volume_path) {

    char* confirm_headers[] = { "Confirm Format of Partition: ",
                        volume_name,
                        "",
                        "  THIS CAN NOT BE UNDONE!",
                        NULL };

    char* items[] = {   "No",
                        "Yes -- Permanently Format",
                        NULL };
    
    char** headers = prepend_title(confirm_headers);
    
    inc_menu_loc(0);
    int chosen_item = get_menu_selection(headers, items, 1, 0);
    if (chosen_item != 1) {
        dec_menu_loc();
        return;
    }
    else {
        ui_set_background(BACKGROUND_ICON_WIPE);
        ui_print("\n-- Wiping %s Partition...\n", volume_name);
        erase_volume(volume_path);
        ui_print("-- %s Partition Wipe Complete!\n", volume_name);
        dec_menu_loc();
    }
}

char* 
print_batt_cap()  {
    char cap_s[4];
    
    FILE * cap = fopen("/sys/class/power_supply/battery/capacity","r");
    fgets(cap_s, 4, cap);
    fclose(cap);
	
    int len = strlen(cap_s);
	if (cap_s[len-1] == '\n') {
		cap_s[len-1] = 0;
	}
    
    // Get a usable time

    struct tm *current;
    time_t now;
    now = time(0);
    current = localtime(&now);
    
    // HACK: Not sure if this could be a possible memory leak
    char* full_cap_s = (char*)malloc(30);
    char full_cap_a[30];
    sprintf(full_cap_a, "Battery Level: %s%% @ %02D:%02D", cap_s, current->tm_hour, current->tm_min);

    strcpy(full_cap_s, full_cap_a);

    //ui_print("\n%s\n", full_cap_s);
    
    return full_cap_s;
}

void all_settings_menu()
{
	// ALL SETTINGS MENU (ALLS for ALL Settings)
	#define ALLS_SIG_TOGGLE           0
	#define ALLS_REBOOT_AFTER_FLASH   1
	#define ALLS_TIME_ZONE            2
	#define ALLS_DEFAULT              3
	#define ALLS_MENU_BACK            4

    static char* MENU_ALLS_HEADERS[] = { "All Settings",
                                              NULL };
    
	char* MENU_ALLS[] =     { zip_verify(),
	                          reboot_after_flash(),
	                          "Change Time Zone",
	                          "Reset Settings to Defaults",
	                          "<-- Back To Main Menu",
	                          NULL };

    char** headers = prepend_title(MENU_ALLS_HEADERS);
    
    inc_menu_loc(ALLS_MENU_BACK);
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, MENU_ALLS, 0, 0);
        switch (chosen_item)
        {
            case ALLS_SIG_TOGGLE:
            	if (is_true(tw_signed_zip_val)) {
            		strcpy(tw_signed_zip_val, "0");
            	} else {
            		strcpy(tw_signed_zip_val, "1");
            	}
                write_s_file();
                break;
			case ALLS_REBOOT_AFTER_FLASH:
                if (is_true(tw_reboot_after_flash_option)) {
            		strcpy(tw_reboot_after_flash_option, "0");
            	} else {
            		strcpy(tw_reboot_after_flash_option, "1");
            	}
                write_s_file();
                break;
			case ALLS_TIME_ZONE:
			    time_zone_menu();
				break;
			case ALLS_DEFAULT:
				tw_set_defaults();
                write_s_file();
				break;
            case ALLS_MENU_BACK:
            	dec_menu_loc();
            	return;
        }
	    if (go_home) { 
	        dec_menu_loc();
	        return;
	    }
        break;
    }
	ui_end_menu();
    dec_menu_loc();
	all_settings_menu();
}	

void time_zone_menu()
{
	// ALL SETTINGS MENU (ALLS for ALL Settings)
	#define TZ_GMT_MINUS10		0
	#define TZ_GMT_MINUS09		1
	#define TZ_GMT_MINUS08		2
	#define TZ_GMT_MINUS07		3
	#define TZ_GMT_MINUS06		4
	#define TZ_GMT_MINUS05		5
	#define TZ_GMT_MINUS04		6
	#define TZ_GMT_MENU_BACK	7

    static char* MENU_TZ_HEADERS[] = { "Time Zone",
                                              NULL };
    
	char* MENU_TZ[] =       { "GMT-10 (HST)",
							  "GMT-9 (AST)",
							  "GMT-8 (PST)",
							  "GMT-7 (MST)",
							  "GMT-6 (CST)",
							  "GMT-5 (EST)",
							  "GMT-4 (AST)",
	                          "<-Back To Main Menu",
	                          NULL };

    char** headers = prepend_title(MENU_TZ_HEADERS);
    
    inc_menu_loc(TZ_GMT_MENU_BACK);
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, MENU_TZ, 0, 3); // puts the initially selected item to GMT which should be right in the middle of the most used time zones for ease of use
        switch (chosen_item)
        {
            case TZ_GMT_MINUS10:
            	strcpy(tw_time_zone_val, "HST10HDT");
                break;
            case TZ_GMT_MINUS09:
            	strcpy(tw_time_zone_val, "AST9ADT");
                break;
            case TZ_GMT_MINUS08:
            	strcpy(tw_time_zone_val, "PST8PDT");
                break;
            case TZ_GMT_MINUS07:
            	strcpy(tw_time_zone_val, "MST7MDT");
                break;
            case TZ_GMT_MINUS06:
            	strcpy(tw_time_zone_val, "CST6CDT");
                break;
            case TZ_GMT_MINUS05:
            	strcpy(tw_time_zone_val, "EST5EDT");
                break;
            case TZ_GMT_MINUS04:
            	strcpy(tw_time_zone_val, "AST4ADT");
                break;
            case TZ_GMT_MENU_BACK:
            	dec_menu_loc();
            	return;
        }
		update_tz_environment_variables();
        write_s_file();
	    if (go_home) { 
	        dec_menu_loc();
	        return;
	    }
    }
}

void update_tz_environment_variables() {
    setenv("TZ", tw_time_zone_val, 1);
    tzset();
}

void inc_menu_loc(int bInt)
{
	menu_loc_idx++;
	menu_loc[menu_loc_idx] = bInt;
	//ui_print("=> Increased Menu Level; %d : %d\n",menu_loc_idx,menu_loc[menu_loc_idx]);
}
void dec_menu_loc()
{
	menu_loc_idx--;
	//ui_print("=> Decreased Menu Level; %d : %d\n",menu_loc_idx,menu_loc[menu_loc_idx]);
}

