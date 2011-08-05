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
#include "themes.h"

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
		//LOGI("=> DEVICE_ID: %s\n", device_id);
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

void tw_reboot()
{
    ui_print("Rebooting...\n");
    sync();
    finish_recovery("s");
    reboot(RB_AUTOBOOT);
}

void install_zip_menu(int pIdx)
{
	// INSTALL ZIP MENU
	#define ITEM_CHOOSE_ZIP           0
	#define ITEM_WIPE_CACHE_DALVIK    1
	#define ITEM_REBOOT_AFTER_FLASH   2
	#define ITEM_TOGGLE_SIG           3
	#define ITEM_ZIP_RBOOT			  4
	#define ITEM_ZIP_BACK		      5
	
    ui_set_background(BACKGROUND_ICON_FLASH_ZIP);
    static char* MENU_FLASH_HEADERS[] = { "Install Zip Menu",
    									  "Flash Zip From SD Card:",
                                          NULL };

	char* MENU_INSTALL_ZIP[] = {  "--> Choose Zip To Flash",
								  "Wipe Cache and Dalvik Cache",
			  	  	  	  	  	  reboot_after_flash(),
								  zip_verify(),
								  "--> Reboot To System",
	                              "<-- Back To Main Menu",
	                              NULL };

	char** headers = prepend_title(MENU_FLASH_HEADERS);

    inc_menu_loc(ITEM_ZIP_BACK);
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, MENU_INSTALL_ZIP, 0, pIdx);
        pIdx = chosen_item;
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
					if (is_true(tw_reboot_after_flash_option)) {
						tw_reboot();
						return;
					}
					if (go_home != 1) {
						ui_print("\nInstall from sdcard complete.\n");
					}
				}
                break;
			case ITEM_WIPE_CACHE_DALVIK:
				ui_print("\n-- Wiping Cache Partition...\n");
                erase_volume("/cache");
                ui_print("-- Cache Partition Wipe Complete!\n");
				wipe_dalvik_cache();
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
			case ITEM_ZIP_RBOOT:
				tw_reboot();
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
	install_zip_menu(pIdx);
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
        if (0 != stat(sde.blk, &st))
        {
            ui_print("/sd-ext not present, skipping\n");
        } else {
        	__system("mount /sd-ext");
    	    LOGI("Mounting /sd-ext\n");
    	    if (stat("/sd-ext/dalvik-cache",&st) == 0)
    	    {
                __system("rm -rf /sd-ext/dalvik-cache");
        	    ui_print("Cleaned: /sd-ext/dalvik-cache...\n");
    	    }
        }
        ensure_path_unmounted("/data");
        ui_print("-- Dalvik Cache Directories Wipe Complete!\n\n");
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
	

    static char* MENU_REBOOT_HEADERS[] = {  "Reboot Menu",
    										"Choose Your Destiny:",
                                            NULL };
    
	// REBOOT MENU
	char* MENU_REBOOT[] = { "Reboot To System",
	                        "Reboot To Recovery",
	                        "Reboot To Bootloader",
	                        "Power Off",
	                        "<-- Back To Advanced Menu",
	                        NULL };

    
    char** headers = prepend_title(MENU_REBOOT_HEADERS);
    
    inc_menu_loc(ITEM_BACKK);
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, MENU_REBOOT, 0, 0);
        switch (chosen_item)
        {
            case ITEM_RECOVERY:
            	ensure_path_unmounted("/sdcard");
                __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, "recovery");
                break;

            case ITEM_BOOTLOADER:
                __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, "bootloader");
                break;

            case ITEM_POWEROFF:
                __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL);
                break;

            case ITEM_SYSTEM:
            	tw_reboot();
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

void fix_perms()
{
	FILE *fp;
	char exe[255];
	char tmpOutput[100];
	char tmpProcess[10];
	sprintf(exe,"fix_permissions.sh");
	ui_show_progress(1,30);
	fp = __popen(exe, "r");
    ui_print("\n-- Fixing Permissions\n");
	while (fscanf(fp,"%s",tmpOutput) != EOF)
	{
		if(is_true(tw_show_spam_val))
		{
			ui_print("%s\n",tmpOutput);
		}
	}
	ui_print("-- Done.\n\n");
	ui_reset_progress();
	__pclose(fp);
}

void advanced_menu()
{
	// ADVANCED MENU
	#define ITEM_REBOOT_MENU       0
	#define ITEM_FORMAT_MENU       1
	#define ITEM_FIX_PERM          2
	#define ITEM_ALL_SETTINGS      3
	#define ITEM_CPY_LOG		   4
	#define ADVANCED_MENU_BACK     5

    static char* MENU_ADVANCED_HEADERS[] = { "Advanced Menu",
    										 "Reboot, Format, or twrp!",
                                              NULL };
    
	char* MENU_ADVANCED[] = { "Reboot Menu",
	                          "Format Menu",
	                          "Fix Permissions",
	                          "Change twrp Settings",
	                          "Copy recovery log to /sdcard",
	                          "<-- Back To Main Menu",
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
            case ITEM_FIX_PERM:
            	fix_perms();
                break;
            case ITEM_ALL_SETTINGS:
			    all_settings_menu(0);
				break;
            case ITEM_CPY_LOG:
                ensure_path_mounted("/sdcard");
            	__system("cp /tmp/recovery.log /sdcard");
                ui_print("Copied recovery log to /sdcard.\n");
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
	struct stat st;
	
	#define ITEM_FORMAT_SYSTEM      0
	#define ITEM_FORMAT_DATA        1
	#define ITEM_FORMAT_CACHE       2
	#define ITEM_FORMAT_SDCARD      3
	#define ITEM_FORMAT_SDEXT		4
	#define ITEM_FORMAT_BACK        5
	
	char* part_headers[] = {    "Format Menu",
                                "Choose a Partition to Format: ",
                                NULL };
	
    char* part_items[] = { 	"Format SYSTEM (/system)",
                            "Format DATA (/data)",
            				"Format CACHE (/cache)",
                            "Format SDCARD (/sdcard)",
                            "Format SD-EXT (/sd-ext)",
						    "<-- Back To Advanced Menu",
						    NULL };

    char** headers = prepend_title(part_headers);
    
    inc_menu_loc(ITEM_FORMAT_BACK);
	for (;;)
	{
                ui_set_background(BACKGROUND_ICON_WIPE_CHOOSE);
		int chosen_item = get_menu_selection(headers, part_items, 0, 0);
		switch (chosen_item)
		{
			case ITEM_FORMAT_SYSTEM:
				confirm_format("SYSTEM", "/system");
				break;
			case ITEM_FORMAT_DATA:
                confirm_format("DATA", "/data");
                break;
			case ITEM_FORMAT_CACHE:
                confirm_format("CACHE", "/cache");
                break;
			case ITEM_FORMAT_SDCARD:
                confirm_format("SDCARD", "/sdcard");
                break;
            case ITEM_FORMAT_SDEXT:
            	if (stat(sde.blk,&st) == 0)
            	{
                	__system("mount /sd-ext");
                    confirm_format("SD-EXT", "/sd-ext");
            	} else {
            		ui_print("\n/sd-ext not detected! Aborting.\n");
            	}
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
    } else {
        ui_set_background(BACKGROUND_ICON_WIPE);
        ui_print("\n-- Wiping %s Partition...\n", volume_name);
        erase_volume(volume_path);
        ui_print("-- %s Partition Wipe Complete!\n", volume_name);
        dec_menu_loc();
    }
}

char* 
print_batt_cap()  {
	char* full_cap_s = (char*)malloc(30);
    char cap_s[4];
	char full_cap_a[30];
	FILE * cap = fopen("/sys/class/power_supply/battery/capacity","r");
	fgets(cap_s, 4, cap);
	fclose(cap);
	
	int cap_i = atoi(cap_s);
    
    //int len = strlen(cap_s);
	//if (cap_s[len-1] == '\n') {
	//	cap_s[len-1] = 0;
	//}
	
	// Get a usable time
	struct tm *current;
	time_t now;
	now = time(0);
	current = localtime(&now);
	
	sprintf(full_cap_a, "Battery Level: %i%% @ %02D:%02D", cap_i, current->tm_hour, current->tm_min);
	strcpy(full_cap_s, full_cap_a);
	
	return full_cap_s;
}

void time_zone_menu()
{
	#define TZ_REBOOT           0
	#define TZ_MINUS            1
	#define TZ_PLUS             2
	#define TZ_MAIN_BACK        3

    static char* MENU_TZ_HEADERS[] = { "Time Zone",
    								   "Select Region:",
                                              NULL };
    
	char* MENU_TZ[] =       { "[REBOOT AND APPLY TIME ZONE]",
            				  "Minus (GMT 0 to -11)",
							  "Plus  (GMT +1 to +12)",
							  "<-- Back To twrp Settings",
	                          NULL };

    char** headers = prepend_title(MENU_TZ_HEADERS);
    
    inc_menu_loc(TZ_MAIN_BACK);
	
	ui_print("Currently selected time zone: %s\n", tw_time_zone_val);
	
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, MENU_TZ, 0, 1);
        switch (chosen_item)
        {
			case TZ_REBOOT:
				ensure_path_unmounted("/sdcard");
				__reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_RESTART2, "recovery");
				break;
            case TZ_MINUS:
            	time_zone_minus();
                break;
            case TZ_PLUS:
            	time_zone_plus();
                break;
            case TZ_MAIN_BACK:
            	dec_menu_loc();
            	return;
        }
	    if (go_home) { 
	        dec_menu_loc();
	        return;
	    }
    }
}

void time_zone_minus()
{
	#define TZ_GMT_MINUS11		0
	#define TZ_GMT_MINUS10		1
	#define TZ_GMT_MINUS09		2
	#define TZ_GMT_MINUS08		3
	#define TZ_GMT_MINUS07		4
	#define TZ_GMT_MINUS06		5
	#define TZ_GMT_MINUS05		6
	#define TZ_GMT_MINUS04		7
	#define TZ_GMT_MINUS03		8
	#define TZ_GMT_MINUS02		9
	#define TZ_GMT_MINUS01		10
	#define TZ_GMT      		11
	#define TZ_GMTCUT      		12
	#define TZ_MINUS_MENU_BACK  13

    static char* MENU_TZ_HEADERS_MINUS[] =    { "Time Zone",
    								         "Instant Time Machine:",
                                              NULL };
    
	char* MENU_TZ_MINUS[] =       { "GMT-11 (BST)",
									"GMT-10 (HST)",
							        "GMT -9 (AKST)",
						            "GMT -8 (PST)",
							        "GMT -7 (MST)",
							        "GMT -6 (CST)",
							        "GMT -5 (EST)",
							        "GMT -4 (AST)",
									"GMT -3 (GRNLNDST)",
									"GMT -2 (FALKST)",
									"GMT -1 (AZOREST)",
									"GMT  0 (GMT)",
									"GMT  0 (CUT)",
	                                "<-- Back To twrp Settings",
	                                NULL };

    char** headers = prepend_title(MENU_TZ_HEADERS_MINUS);
    
    inc_menu_loc(TZ_MINUS_MENU_BACK);
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, MENU_TZ_MINUS, 0, 6);
        switch (chosen_item)
        {
            case TZ_GMT_MINUS11:
            	strcpy(tw_time_zone_val, "BST11");
				strcpy(time_zone_dst_string, "BDT");
                break;
			case TZ_GMT_MINUS10:
            	strcpy(tw_time_zone_val, "HST10");
				strcpy(time_zone_dst_string, "HDT");
                break;
            case TZ_GMT_MINUS09:
            	strcpy(tw_time_zone_val, "AST9");
				strcpy(time_zone_dst_string, "ADT");
                break;
            case TZ_GMT_MINUS08:
            	strcpy(tw_time_zone_val, "PST8");
				strcpy(time_zone_dst_string, "PDT");
                break;
            case TZ_GMT_MINUS07:
            	strcpy(tw_time_zone_val, "MST7");
				strcpy(time_zone_dst_string, "MDT");
                break;
            case TZ_GMT_MINUS06:
            	strcpy(tw_time_zone_val, "CST6");
				strcpy(time_zone_dst_string, "CDT");
                break;
            case TZ_GMT_MINUS05:
            	strcpy(tw_time_zone_val, "EST5");
				strcpy(time_zone_dst_string, "EDT");
                break;
            case TZ_GMT_MINUS04:
            	strcpy(tw_time_zone_val, "AST4");
				strcpy(time_zone_dst_string, "ADT");
                break;
			case TZ_GMT_MINUS03:
            	strcpy(tw_time_zone_val, "GRNLNDST3");
				strcpy(time_zone_dst_string, "GRNLNDDT");
                break;
			case TZ_GMT_MINUS02:
            	strcpy(tw_time_zone_val, "FALKST2");
				strcpy(time_zone_dst_string, "FALKDT");
                break;
			case TZ_GMT_MINUS01:
            	strcpy(tw_time_zone_val, "AZOREST1");
				strcpy(time_zone_dst_string, "AZOREDT");
                break;
			case TZ_GMT:
            	strcpy(tw_time_zone_val, "GMT0");
				strcpy(time_zone_dst_string, "BST");
                break;
			case TZ_GMTCUT:
            	strcpy(tw_time_zone_val, "CUT0");
				strcpy(time_zone_dst_string, "GDT");
                break;
            case TZ_MINUS_MENU_BACK:
            	dec_menu_loc();
            	return;
        }
	    if (go_home) { 
	        dec_menu_loc();
	        return;
	    } else {
			ui_print("New time zone: %s", tw_time_zone_val);
			time_zone_offset();
			time_zone_dst();
			update_tz_environment_variables();
			write_s_file();
			ui_print("\nTime zone change requires reboot.\n");
			dec_menu_loc();
			return;
		}
    }
}

void time_zone_plus()
{
	#define TZ_GMT_PLUS01		0
	#define TZ_GMT_PLUS02USAST  1
	#define TZ_GMT_PLUS02WET	2
	#define TZ_GMT_PLUS03SAUST  3
	#define TZ_GMT_PLUS03MEST	4
	#define TZ_GMT_PLUS04   	5
	#define TZ_GMT_PLUS05   	6
	#define TZ_GMT_PLUS06   	7
	#define TZ_GMT_PLUS07   	8
	#define TZ_GMT_PLUS08TAIST	9
	#define TZ_GMT_PLUS08WAUST 10
	#define TZ_GMT_PLUS09KORST 11
	#define TZ_GMT_PLUS09JST   12
	#define TZ_GMT_PLUS10      13
	#define TZ_GMT_PLUS11      14
	#define TZ_GMT_PLUS12      15
	#define TZ_PLUS_MENU_BACK  16

    static char* MENU_TZ_HEADERS_PLUS[] =    { "Time Zone",
    								         "Instant Time Machine:",
                                              NULL };
    
	char* MENU_TZ_PLUS[] =       { "GMT +1 (NFT)",
							           "GMT +2 (USAST)",
						               "GMT +2 (WET)",
							           "GMT +3 (SAUST)",
						               "GMT +3 (MEST)",
							           "GMT +4 (WST)",
							           "GMT +5 (PAKST)",
									   "GMT +6 (TASHST)",
									   "GMT +7 (THAIST)",
									   "GMT +8 (TAIST)",
									   "GMT +8 (WAUST)",
									   "GMT +9 (KORST)",
									   "GMT +9 (JST)",
									   "GMT+10 (EET)",
									   "GMT+11 (MET)",
									   "GMT+12 (NZST)",
	                                   "<-- Back To twrp Settings",
	                                   NULL };

    char** headers = prepend_title(MENU_TZ_HEADERS_PLUS);
    
    inc_menu_loc(TZ_PLUS_MENU_BACK);
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, MENU_TZ_PLUS, 0, 3); // puts the initially selected item to MST/MDT which should be right in the middle of the most used time zones for ease of use
        switch (chosen_item)
        {
            case TZ_GMT_PLUS01:
            	strcpy(tw_time_zone_val, "NFT-1");
				strcpy(time_zone_dst_string, "DFT");
                break;
            case TZ_GMT_PLUS02USAST:
            	strcpy(tw_time_zone_val, "USAST-2");
				strcpy(time_zone_dst_string, "USADT");
                break;
            case TZ_GMT_PLUS02WET:
            	strcpy(tw_time_zone_val, "WET-2");
				strcpy(time_zone_dst_string, "WET");
                break;
            case TZ_GMT_PLUS03SAUST:
            	strcpy(tw_time_zone_val, "SAUST-3");
				strcpy(time_zone_dst_string, "SAUDT");
                break;
            case TZ_GMT_PLUS03MEST:
            	strcpy(tw_time_zone_val, "MEST-3");
				strcpy(time_zone_dst_string, "MEDT");
                break;
            case TZ_GMT_PLUS04:
            	strcpy(tw_time_zone_val, "WST-4");
				strcpy(time_zone_dst_string, "WDT");
                break;
            case TZ_GMT_PLUS05:
            	strcpy(tw_time_zone_val, "PAKST-5");
				strcpy(time_zone_dst_string, "PAKDT");
                break;
			case TZ_GMT_PLUS06:
            	strcpy(tw_time_zone_val, "TASHST-6");
				strcpy(time_zone_dst_string, "TASHDT");
                break;
			case TZ_GMT_PLUS07:
            	strcpy(tw_time_zone_val, "THAIST-7");
				strcpy(time_zone_dst_string, "THAIDT");
                break;
			case TZ_GMT_PLUS08TAIST:
            	strcpy(tw_time_zone_val, "TAIST-8");
				strcpy(time_zone_dst_string, "TAIDT");
                break;
			case TZ_GMT_PLUS08WAUST:
            	strcpy(tw_time_zone_val, "WAUST-8");
				strcpy(time_zone_dst_string, "WAUDT");
                break;
			case TZ_GMT_PLUS09KORST:
            	strcpy(tw_time_zone_val, "KORST-9");
				strcpy(time_zone_dst_string, "KORDT");
                break;
			case TZ_GMT_PLUS09JST:
            	strcpy(tw_time_zone_val, "JST-9");
				strcpy(time_zone_dst_string, "JSTDT");
                break;
			case TZ_GMT_PLUS10:
            	strcpy(tw_time_zone_val, "EET-10");
				strcpy(time_zone_dst_string, "EETDT");
                break;
			case TZ_GMT_PLUS11:
            	strcpy(tw_time_zone_val, "MET-11");
				strcpy(time_zone_dst_string, "METDT");
                break;
			case TZ_GMT_PLUS12:
            	strcpy(tw_time_zone_val, "NZST-12");
				strcpy(time_zone_dst_string, "NZDT");
                break;
            case TZ_PLUS_MENU_BACK:
            	dec_menu_loc();
            	return;
        }
	    if (go_home) { 
	        dec_menu_loc();
	        return;
	    } else {
			ui_print("New time zone: %s", tw_time_zone_val);
			time_zone_offset();
			time_zone_dst();
			update_tz_environment_variables();
			write_s_file();
			ui_print("\nTime zone changes take effect after reboot.\n");
			dec_menu_loc();
			return;
		}
    }
}

void time_zone_offset() {
    #define TZ_OFF00             0
	#define TZ_OFF15             1
	#define TZ_OFF30             2
	#define TZ_OFF45             3
	#define TZ_OFF_BACK          4

    static char* MENU_TZOFF_HEADERS[] = { "Time Zone",
    								   "Select Offset:",
                                              NULL };
    
	char* MENU_TZOFF[] =       {  "No Offset",
					    		  ":15",
								  ":30",
								  ":45",
	                              "<-- Back To Time Zones",
	                          NULL };

    char** headers = prepend_title(MENU_TZOFF_HEADERS);
    
    inc_menu_loc(TZ_OFF_BACK);
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, MENU_TZOFF, 0, 0);
        switch (chosen_item)
        {
            case TZ_OFF00:
				strcpy(time_zone_offset_string, "");
				break;
			case TZ_OFF15:
            	strcpy(time_zone_offset_string, ":15");
                break;
            case TZ_OFF30:
            	strcpy(time_zone_offset_string, ":30");
                break;
			case TZ_OFF45:
            	strcpy(time_zone_offset_string, ":45");
                break;
            case TZ_OFF_BACK:
            	dec_menu_loc();
            	return;
        }
	    if (go_home) { 
	        dec_menu_loc();
	        return;
	    } else {
			dec_menu_loc();
			ui_print_overwrite("New time zone: %s%s", tw_time_zone_val, time_zone_offset_string);
			return;
		}
    }
}

void time_zone_dst() {
	#define TZ_DST_YES           0
	#define TZ_DST_NO            1
	#define TZ_DST_BACK          2

    static char* MENU_TZDST_HEADERS[] = { "Time Zone",
    								      "Select Daylight Savings Option:",
                                              NULL };
    
	char* MENU_TZDST[] =       { "Yes, I have DST",
							     "No DST",
	                             "<-- Back To Offsets",
	                             NULL };
	
    char** headers = prepend_title(MENU_TZDST_HEADERS);
    
    inc_menu_loc(TZ_DST_BACK);
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, MENU_TZDST, 0, 0);
        switch (chosen_item)
        {
            case TZ_DST_YES:
            	strcat(tw_time_zone_val, time_zone_offset_string);
				strcat(tw_time_zone_val, time_zone_dst_string);
				ui_print_overwrite("New time zone: %s", tw_time_zone_val);
                break;
            case TZ_DST_NO:
				strcat(tw_time_zone_val, time_zone_offset_string);
                break;
            case TZ_DST_BACK:
            	dec_menu_loc();
            	return;
        }
	    if (go_home) { 
	        dec_menu_loc();
	        return;
	    } else {
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
	//ui_print("=> Increased Menu Level; %d : %d\n",menu_loc_idx,menu_loc[menu_loc_idx]);  //TURN THIS ON TO DEBUG
}
void dec_menu_loc()
{
	menu_loc_idx--;
	//ui_print("=> Decreased Menu Level; %d : %d\n",menu_loc_idx,menu_loc[menu_loc_idx]); //TURN THIS ON TO DEBUG
}

#define MNT_SYSTEM 	0
#define MNT_DATA	1
#define MNT_CACHE	2
#define MNT_SDCARD	3
#define MNT_SDEXT	4
#define MNT_BACK	5

void chkMounts()
{
	FILE *fp;
	char tmpOutput[255];
	sysIsMounted = 0;
	datIsMounted = 0;
	cacIsMounted = 0;
	sdcIsMounted = 0;
	sdeIsMounted = 0;
	fp = __popen("cat /proc/mounts", "r");
	while (fgets(tmpOutput,255,fp) != NULL)
	{
	    sscanf(tmpOutput,"%s %*s %*s %*s %*d %*d",tmp.blk);
	    if (strcmp(tmp.blk,sys.blk) == 0)
	    {
	    	sysIsMounted = 1;
	    }
	    if (strcmp(tmp.blk,dat.blk) == 0)
	    {
	    	datIsMounted = 1;
	    }
	    if (strcmp(tmp.blk,cac.blk) == 0)
	    {
	    	cacIsMounted = 1;
	    }
	    if (strcmp(tmp.blk,sdc.blk) == 0)
	    {
	    	sdcIsMounted = 1;
	    }
	    if (strcmp(tmp.blk,sde.blk) == 0)
	    {
	    	sdeIsMounted = 1;
	    }
	}
	__pclose(fp);
}

char* isMounted(int mInt)
{
	int isTrue = 0;
	struct stat st;
	char* tmp_set = (char*)malloc(25);
	if (mInt == MNT_SYSTEM)
	{
	    if (sysIsMounted == 1) {
			strcpy(tmp_set, "unmount");
	    } else {
			strcpy(tmp_set, "mount");
	    }
		strcat(tmp_set, " /system");
	}
	if (mInt == MNT_DATA)
	{
	    if (datIsMounted == 1) {
			strcpy(tmp_set, "unmount");
	    } else {
			strcpy(tmp_set, "mount");
	    }
		strcat(tmp_set, " /data");
	}
	if (mInt == MNT_CACHE)
	{
	    if (cacIsMounted == 1) {
			strcpy(tmp_set, "unmount");
	    } else {
			strcpy(tmp_set, "mount");
	    }
		strcat(tmp_set, " /cache");
	}
	if (mInt == MNT_SDCARD)
	{
	    if (sdcIsMounted == 1) {
			strcpy(tmp_set, "unmount");
	    } else {
			strcpy(tmp_set, "mount");
	    }
		strcat(tmp_set, " /sdcard");
	}
	if (mInt == MNT_SDEXT)
	{
		if (stat(sde.blk,&st) != 0)
		{
			strcpy(tmp_set, "-------");
			sdeIsMounted = -1;
		} else {
		    if (sdeIsMounted == 1) {
				strcpy(tmp_set, "unmount");
		    } else {
				strcpy(tmp_set, "mount");
		    }
		}
		strcat(tmp_set, " /sd-ext");
	}
	return tmp_set;
}

void mount_menu(int pIdx)
{
	chkMounts();
	
	static char* MENU_MNT_HEADERS[] = { "Mount Menu",
										"Pick a Partition to Mount:",
										NULL };
	
	char* MENU_MNT[] = { isMounted(MNT_SYSTEM),
						 isMounted(MNT_DATA),
						 isMounted(MNT_CACHE),
						 isMounted(MNT_SDCARD),
						 isMounted(MNT_SDEXT),
						 "<-- Back To Main Menu",
						 NULL };
	
	char** headers = prepend_title(MENU_MNT_HEADERS);
	
	inc_menu_loc(MNT_BACK);
	for (;;)
	{
		int chosen_item = get_menu_selection(headers, MENU_MNT, 0, pIdx);
		pIdx = chosen_item;
		switch (chosen_item)
		{
		case MNT_SYSTEM:
			if (sysIsMounted == 0)
			{
				__system("mount /system");
				ui_print("/system has been mounted.\n");
			} else if (sysIsMounted == 1) {
				__system("umount /system");
				ui_print("/system has been unmounted.\n");
			}
			break;
		case MNT_DATA:
			if (datIsMounted == 0)
			{
				__system("mount /data");
				ui_print("/data has been mounted.\n");
			} else if (datIsMounted == 1) {
				__system("umount /data");
				ui_print("/data has been unmounted.\n");
			}
			break;
		case MNT_CACHE:
			if (cacIsMounted == 0)
			{
				__system("mount /cache");
				ui_print("/cache has been mounted.\n");
			} else if (cacIsMounted == 1) {
				__system("umount /cache");
				ui_print("/cache has been unmounted.\n");
			}
			break; 
		case MNT_SDCARD:
			if (sdcIsMounted == 0)
			{
				__system("mount /sdcard");
				ui_print("/sdcard has been mounted.\n");
			} else if (sdcIsMounted == 1) {
				__system("umount /sdcard");
				ui_print("/sdcard has been unmounted.\n");
			}
			break;
		case MNT_SDEXT:
			if (sdeIsMounted == 0)
			{
				__system("mount /sd-ext");
				ui_print("/sd-ext has been mounted.\n");
			} else if (sdeIsMounted == 1) {
				__system("umount /sd-ext");
				ui_print("/sd-ext has been unmounted.\n");
			}
			break;
		case MNT_BACK:
			dec_menu_loc();
			return;
		}
	    break;
	}
	ui_end_menu();
    dec_menu_loc();
    mount_menu(pIdx);
}

void main_wipe_menu()
{
	#define MAIN_WIPE_CACHE           0
	#define MAIN_WIPE_DALVIK   	  	  1
	#define MAIN_WIPE_DATA            2
	#define ITEM_BATTERY_STATS     	  3
	#define ITEM_ROTATE_DATA       	  4
	#define MAIN_WIPE_BACK            5

    static char* MENU_MAIN_WIPE_HEADERS[] = { "Wipe Menu",
    										  "Wipe Front to Back:",
                                              NULL };
    
	char* MENU_MAIN_WIPE[] = { "Wipe Cache",
	                           "Wipe Dalvik Cache",
	                           "Wipe Everything (Data Factory Reset)",
		                       "Wipe Battery Stats",
		                       "Wipe Rotation Data",
	                           "<-- Back To Main Menu",
	                           NULL };

    char** headers = prepend_title(MENU_MAIN_WIPE_HEADERS);

    int isSdExt = 0;
    struct stat st;
    if (stat("/sd-ext",&st) == 0)
    {
    	__system("mount /sd-ext");
    	isSdExt = 1;
    }
    
    inc_menu_loc(MAIN_WIPE_BACK);
    for (;;)
    {
	ui_set_background(BACKGROUND_ICON_WIPE_CHOOSE);
        int chosen_item = get_menu_selection(headers, MENU_MAIN_WIPE, 0, 0);
        switch (chosen_item)
        {
            case MAIN_WIPE_DALVIK:
                wipe_dalvik_cache();
                break;

            case MAIN_WIPE_CACHE:
            	ui_set_background(BACKGROUND_ICON_WIPE);
                ui_print("\n-- Wiping Cache Partition...\n");
                erase_volume("/cache");
                ui_print("-- Cache Partition Wipe Complete!\n");
                ui_set_background(BACKGROUND_ICON_WIPE_CHOOSE);
                if (!ui_text_visible()) return;
                break;

            case MAIN_WIPE_DATA:
                wipe_data(ui_text_visible());
                ui_set_background(BACKGROUND_ICON_WIPE_CHOOSE);
                if (!ui_text_visible()) return;
                break;

            case ITEM_BATTERY_STATS:
                wipe_battery_stats();
                break;

            case ITEM_ROTATE_DATA:
                wipe_rotate_data();
                break;
                
            case MAIN_WIPE_BACK:
            	dec_menu_loc();
                ui_set_background(BACKGROUND_ICON_MAIN);
            	return;
        }
	    if (go_home) { 
	        dec_menu_loc();
                ui_set_background(BACKGROUND_ICON_MAIN);
	        return;
	    }
        break;
    }
	ui_end_menu();
        dec_menu_loc();
        ui_set_background(BACKGROUND_ICON_MAIN);
	main_wipe_menu();
}

char* toggle_spam()
{
	char* tmp_set = (char*)malloc(40);
	strcpy(tmp_set, "[ ] Toggle twrp Spam");
	if (is_true(tw_show_spam_val) == 1) {
		tmp_set[1] = 'x';
	}
	return tmp_set;
}

void all_settings_menu(int pIdx)
{
	// ALL SETTINGS MENU (ALLS for ALL Settings)
	#define ALLS_SIG_TOGGLE           0
	#define ALLS_REBOOT_AFTER_FLASH   1
	#define ALLS_SPAM				  2
	#define ALLS_TIME_ZONE            3
	#define ALLS_ZIP_LOCATION   	  4
	#define ALLS_THEMES               5
	#define ALLS_DEFAULT              6
	#define ALLS_MENU_BACK            7

    static char* MENU_ALLS_HEADERS[] = { "Change twrp Settings",
    									 "twrp or gtfo:",
                                         NULL };
    
	char* MENU_ALLS[] =     { zip_verify(),
	                          reboot_after_flash(),
	                          toggle_spam(),
	                          "Change Time Zone",
	                          "Change Zip Default Folder",
	                          "Change twrp Color Theme",
	                          "Reset Settings to Defaults",
	                          "<-- Back To Advanced Menu",
	                          NULL };

    char** headers = prepend_title(MENU_ALLS_HEADERS);
    
    inc_menu_loc(ALLS_MENU_BACK);
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, MENU_ALLS, 0, pIdx);
        pIdx = chosen_item;
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
			case ALLS_SPAM:
                if (is_true(tw_show_spam_val)) {
            		strcpy(tw_show_spam_val, "0");
            	} else {
            		strcpy(tw_show_spam_val, "1");
            	}
                write_s_file();
				break;
			case ALLS_THEMES:
			    twrp_themes_menu();
				break;
			case ALLS_TIME_ZONE:
			    time_zone_menu();
				break;
            case ALLS_ZIP_LOCATION:
            	get_new_zip_dir = 1;
            	sdcard_directory(SDCARD_ROOT);
            	get_new_zip_dir = 0;
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
	all_settings_menu(pIdx);
}
