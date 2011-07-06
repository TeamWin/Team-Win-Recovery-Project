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

//kang system() from bionic/libc/unistd and rename it __system() so we can be even more hackish :)
#undef _PATH_BSHELL
#define _PATH_BSHELL "/sbin/sh"

static const char *SDCARD_ROOT = "/sdcard";

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

// Default Settings
void
tw_set_defaults() {
	strcpy(tw_signed_zip_val, "0");
	strcpy(tw_nan_system_val, "1");
	strcpy(tw_nan_data_val, "1");
	strcpy(tw_nan_cache_val, "1");
	strcpy(tw_nan_boot_val, "1");
	strcpy(tw_nan_wimax_val, "1");
	strcpy(tw_nan_recovery_val, "0");
	strcpy(tw_nan_sdext_val, "0");
	strcpy(tw_nan_andsec_val, "0");
	strcpy(tw_zip_location_val, "/sdcard");
    strcpy(tw_time_zone_val, "-5");
}

int is_true(char* tw_setting) {
	if (strcmp(tw_setting,"1") == 0) {
		return 1;
	} else {
		return 0;
	}
}

// Write Settings to file Function
void
write_s_file() {
	int nan_dir_status = 1;
	if (ensure_path_mounted(SDCARD_ROOT) != 0) {
		LOGI("--> Can not mount /sdcard, running on default settings\n"); // Can't mount sdcard, default settings should be unchanged.
	} else {
		struct stat st;
		if(stat("/sdcard/nandroid",&st) != 0) {
			LOGI("--> /sdcard/nandroid directory not present!\n");
			if(mkdir("/sdcard/nandroid",0777) == -1) { // create directory
				nan_dir_status = 0;
				LOGI("--> Can not create directory: /sdcard/nandroid\n");
			} else {
				nan_dir_status = 1;
				LOGI("--> Created directory: /sdcard/nandroid\n");
			}
		}
		if (nan_dir_status == 1) {
			FILE *fp; // define file
			fp = fopen(TW_SETTINGS_FILE, "w"); // open file, create if not exist, if exist, overwrite contents
			if (fp == NULL) {
				LOGI("--> Can not open settings file to write.\n"); // Can't open/create file, default settings still loaded into memory.
			} else {
				int i = 0;
				while(i < TW_MAX_NUM_SETTINGS) {
					if (i == TW_VERSION) {
						fputs(tw_version_val, fp);
					} else if (i == TW_SIGNED_ZIP) {
						fputs(tw_signed_zip_val, fp);
					} else if (i == TW_NAN_SYSTEM) {
						fputs(tw_nan_system_val, fp);
					} else if (i == TW_NAN_DATA) {
						fputs(tw_nan_data_val, fp);
					} else if (i == TW_NAN_CACHE) {
						fputs(tw_nan_cache_val, fp);
					} else if (i == TW_NAN_BOOT) {
						fputs(tw_nan_boot_val, fp);
					} else if (i == TW_NAN_WIMAX) {
						fputs(tw_nan_wimax_val, fp);
					} else if (i == TW_NAN_RECOVERY) {
						fputs(tw_nan_recovery_val, fp);
					} else if (i == TW_NAN_SDEXT) {
						fputs(tw_nan_sdext_val, fp);
					} else if (i == TW_NAN_ANDSEC) {
						fputs(tw_nan_andsec_val, fp);
					} else if (i == TW_ZIP_LOCATION) {
						fputs(tw_zip_location_val, fp);
					} else if (i == TW_TIME_ZONE) {
						fputs(tw_time_zone_val, fp);
					}
					fputs("\n", fp); // add a carriage return to finish line
					i++; // increment loop
				}
				fclose(fp); // close file
				LOGI("--> Wrote to configuration file: %s\n", TW_SETTINGS_FILE); // log
			}
		}
	}
}

// Read from Settings file Function
void
read_s_file() {
	if (ensure_path_mounted(SDCARD_ROOT) != 0) {
		LOGI("--> Can not mount /sdcard, running on default settings\n"); // Can't mount sdcard, default settings should be unchanged.
	} else {
		FILE *fp; // define file
		fp = fopen(TW_SETTINGS_FILE, "r"); // Open file for read
		if (fp == NULL) {
			LOGI("--> Can not open settings file, will try to create file.\n"); // Can't open file, default settings should be unchanged.
			write_s_file(); // call save settings function if settings file doesn't exist
		} else {
			int i = 0;
			int len;
			char s_line[TW_MAX_SETTINGS_CHARS+2]; // Set max characters + 2 (because of terminating and carriage return)
			while(i < TW_MAX_NUM_SETTINGS) {
				fgets(s_line, TW_MAX_SETTINGS_CHARS+2, fp); // Read a line from file
				len = strlen(s_line); // get length of line
				if (s_line[len-1] == '\n') { // if last char is carriage return
					s_line[len-1] = 0; // remove it by setting it to 0
				}
				if (i == TW_VERSION) {
					if (strcmp(s_line,tw_version_val) != 0) {
						ui_print("\n--> Wrong recoverywin version detected, default settings applied.\n\n"); //
						tw_set_defaults();
						write_s_file();
						break;
					}
				} else if (i == TW_SIGNED_ZIP) {
			    	strcpy(tw_signed_zip_val, s_line);
			    } else if (i == TW_NAN_SYSTEM) {
			    	strcpy(tw_nan_system_val, s_line);
                } else if (i == TW_NAN_DATA) {
			    	strcpy(tw_nan_data_val, s_line);
				} else if (i == TW_NAN_CACHE) {
			    	strcpy(tw_nan_cache_val, s_line);
				} else if (i == TW_NAN_BOOT) {
			    	strcpy(tw_nan_boot_val, s_line);
				} else if (i == TW_NAN_WIMAX) {
			    	strcpy(tw_nan_wimax_val, s_line);
				} else if (i == TW_NAN_RECOVERY) {
			    	strcpy(tw_nan_recovery_val, s_line);
				} else if (i == TW_NAN_SDEXT) {
			    	strcpy(tw_nan_sdext_val, s_line);
				} else if (i == TW_NAN_ANDSEC) {
			    	strcpy(tw_nan_andsec_val, s_line);
				} else if (i == TW_ZIP_LOCATION) {
			    	strcpy(tw_zip_location_val, s_line);
				} else if (i == TW_TIME_ZONE) {
			    	strcpy(tw_time_zone_val, s_line);
				}
				i++; // increment loop
			}
			fclose(fp); // close file
		}
	}
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

char* MENU_INSTALL_ZIP[] = {  "Choose Zip To Flash",
							  "Toggle Signature Verification",
                              "<- Back To Main Menu",
                              NULL };
// INSTALL ZIP MENU
#define ITEM_CHOOSE_ZIP      0
#define ITEM_TOGGLE_SIG      1
#define ITEM_ZIP_BACK		 2

void install_zip_menu()
{
    ui_set_background(BACKGROUND_ICON_FLASH_ZIP);
    static char* MENU_FLASH_HEADERS[] = {   "Flash zip From SD card",
                                            "",
                                            NULL
    };
    save_up_a_level_menu_location(ITEM_ZIP_BACK);
    ui_print("Signature Check Currently: %s\n", is_true(tw_signed_zip_val) ? "Enabled" : "Disabled");
    
    for (;;)
    {
        int chosen_item = get_menu_selection(MENU_FLASH_HEADERS, MENU_INSTALL_ZIP, 0, 0);
        switch (chosen_item)
        {
            case ITEM_CHOOSE_ZIP:
            	;
                int status = sdcard_directory(SDCARD_ROOT);
                ui_reset_progress();  // reset status bar so it doesnt run off the screen 
                if (status != INSTALL_SUCCESS) {
                    //ui_set_background(BACKGROUND_ICON_ERROR);
                    //ui_print("Installation aborted.\n");
                } else if (!ui_text_visible()) {
                    return;  // reboot if logs aren't visible
                } else {
                    ui_print("\nInstall from sdcard complete.\n");
                }
                break;
            case ITEM_TOGGLE_SIG:
            	if (is_true(tw_signed_zip_val)) {
            		strcpy(tw_signed_zip_val, "0");
            	} else {
            		strcpy(tw_signed_zip_val, "1");
            	}
            	ui_print("Signature Check Changed to: %s\n", is_true(tw_signed_zip_val) ? "Enabled" : "Disabled");
                write_s_file();
                break;
            case ITEM_ZIP_BACK:
                ui_set_background(BACKGROUND_ICON_MAIN);
                return;
        }
    }
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

// REBOOT MENU
char* MENU_REBOOT[] = { "Reboot To System",
                        "Reboot To Recovery",
                        "Reboot To Bootloader",
                        "Power Off",
                        "<-Back To Main Menu",
                        NULL };

#define ITEM_SYSTEM      0
#define ITEM_RECOVERY    1
#define ITEM_BOOTLOADER  2
#define ITEM_POWEROFF    3

void reboot_menu()
{
    int result;
    int chosen_item = 4;
    finish_recovery(NULL);

    static char* MENU_REBOOT_HEADERS[] = {  "Reboot Menu",
                                            "",
                                            NULL
    };
    save_up_a_level_menu_location(4);
    for (;;)
    {
        int chosen_item = get_menu_selection(MENU_REBOOT_HEADERS, MENU_REBOOT, 0, 0);
        switch (chosen_item)
        {
        // force item 4 always to go "back"
        if (chosen_item == 4) {
            result = -1;
            break;
            }

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

            default:
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
        remove("/data/system/batterystats.bin");
        ui_print("Cleared: Battery Stats...\n");
        ensure_path_unmounted("/data");
    }
}

// ROTATION SETTINGS
void wipe_rotate_data()
{
    ensure_path_mounted("/data");
    __system("rm -r /data/misc/akmd*");
    __system("rm -r /data/misc/rild*");
    ui_print("Cleared: Rotatation Data...\n");
    ensure_path_unmounted("/data");
}   

// ADVANCED MENU
char* MENU_ADVANCED[] = {   "Reboot Menu",
                            "Format Menu",
                            "Wipe Battery Stats",
                            "Wipe Rotation Data",
                            "<-Back To Main Menu",
                            NULL };

#define ITEM_REBOOT_MENU       0
#define ITEM_FORMAT_MENU       1
#define ITEM_BATTERY_STATS     2
#define ITEM_ROTATE_DATA       3

void advanced_menu()
{
    int result;
    int chosen_item = 4;

    static char* MENU_ADVANCED_HEADERS[] = {    "Advanced Options",
                                                "",
                                                NULL
    };
    save_up_a_level_menu_location(3);
    for (;;)
    {
        int chosen_item = get_menu_selection(MENU_ADVANCED_HEADERS, MENU_ADVANCED, 0, 0);
        switch (chosen_item)
        {
        // force item 4 always to go "back"
        if (chosen_item == 4) {
            result = -1;
            break;
            }

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

            default:
                return;
        }
    }
}

// NANDROID MENU
void
nandroid_menu()
{	
	#define ITEM_BACKUP_MENU       0
	#define ITEM_RESTORE_MENU      1
	#define ITEM_MENU_BACK         2
	
	char* nan_headers[] = { "Nandroid Menu",
							"Choose Backup or Restore: ",
							"",
							NULL };
	
	char* nan_items[] = { "Backup Menu",
						  "Restore Menu",
						  "<- Back To Main Menu",
						  NULL };
	
	for (;;)
	{
		int chosen_item = get_menu_selection(nan_headers, nan_items, 0, 0);
		switch (chosen_item)
		{
			case ITEM_BACKUP_MENU:
				nan_backup_menu();
				break;
			case ITEM_RESTORE_MENU:
				nan_restore_menu();
				break;
			case ITEM_MENU_BACK:
				return;
		}
	}
}

#define ITEM_NAN_BACKUP		0
#define ITEM_NAN_SYSTEM		1
#define ITEM_NAN_DATA      	2
#define ITEM_NAN_CACHE		3
#define ITEM_NAN_BOOT       4
#define ITEM_NAN_WIMAX      5
#define ITEM_NAN_RECOVERY   6
#define ITEM_NAN_SDEXT      7
#define ITEM_NAN_ANDSEC     8
#define ITEM_NAN_BACK       9

void
nan_backup_menu()
{
	char* nan_b_headers[] = { "Nandroid Backup",
							   "Choose Nandroid Options: ",
							   "",
							   NULL	};
	
	char* nan_b_items[] = { "-> Backup Naowz!",
							 nan_img_set(ITEM_NAN_SYSTEM),
							 nan_img_set(ITEM_NAN_DATA),
							 nan_img_set(ITEM_NAN_CACHE),
							 nan_img_set(ITEM_NAN_BOOT),
							 nan_img_set(ITEM_NAN_WIMAX),
							 nan_img_set(ITEM_NAN_RECOVERY),
							 nan_img_set(ITEM_NAN_SDEXT),
							 nan_img_set(ITEM_NAN_ANDSEC),
							 "<- Back To Main Menu",
							 NULL };
	save_up_a_level_menu_location(ITEM_NAN_BACK);
	for (;;)
	{
		int chosen_item = get_menu_selection(nan_b_headers, nan_b_items, 0, 0);
		switch (chosen_item)
		{
			case ITEM_NAN_BACKUP:
				nandroid_string();
				ui_print("\nNandroid String : %s\n\n", tw_nandroid_string);
				break;
			case ITEM_NAN_SYSTEM:
            	if (is_true(tw_nan_system_val)) {
            		strcpy(tw_nan_system_val, "0");
            	} else {
            		strcpy(tw_nan_system_val, "1");
            	}
                write_s_file();
                break;
			case ITEM_NAN_DATA:
            	if (is_true(tw_nan_data_val)) {
            		strcpy(tw_nan_data_val, "0");
            	} else {
            		strcpy(tw_nan_data_val, "1");
            	}
                write_s_file();
				break;
			case ITEM_NAN_CACHE:
            	if (is_true(tw_nan_cache_val)) {
            		strcpy(tw_nan_cache_val, "0");
            	} else {
            		strcpy(tw_nan_cache_val, "1");
            	}
                write_s_file();
				break;
			case ITEM_NAN_BOOT:
            	if (is_true(tw_nan_boot_val)) {
            		strcpy(tw_nan_boot_val, "0");
            	} else {
            		strcpy(tw_nan_boot_val, "1");
            	}
                write_s_file();
				break;
			case ITEM_NAN_WIMAX:
            	if (is_true(tw_nan_wimax_val)) {
            		strcpy(tw_nan_wimax_val, "0");
            	} else {
            		strcpy(tw_nan_wimax_val, "1");
            	}
                write_s_file();
				break;
			case ITEM_NAN_RECOVERY:
            	if (is_true(tw_nan_recovery_val)) {
            		strcpy(tw_nan_recovery_val, "0");
            	} else {
            		strcpy(tw_nan_recovery_val, "1");
            	}
                write_s_file();
				break;
			case ITEM_NAN_SDEXT:
            	if (is_true(tw_nan_sdext_val)) {
            		strcpy(tw_nan_sdext_val, "0");
            	} else {
            		strcpy(tw_nan_sdext_val, "1");
            	}
                write_s_file();
				break;
			case ITEM_NAN_ANDSEC:
            	if (is_true(tw_nan_andsec_val)) {
            		strcpy(tw_nan_andsec_val, "0");
            	} else {
            		strcpy(tw_nan_andsec_val, "1");
            	}
                write_s_file();
				break;
			case ITEM_NAN_BACK:
				return;
		}
		break;
	}
	ui_end_menu();
	nan_backup_menu();
}

void
nan_restore_menu()
{
	#define ITEM_NAN_RESTORE 	0
	#define ITEM_NAN_RESBACK 	1
	
	char* nan_r_headers[] = { "Nandroid Restore",
							  "Choose Nandroid Options: ",
							  "",
							  NULL	};
	
	char* nan_r_items[] = { "-> Restore Naowz!",
							"<- Back To Main Menu",
							 NULL };
	save_up_a_level_menu_location(ITEM_NAN_RESBACK);
	for (;;)
	{
		int chosen_item = get_menu_selection(nan_r_headers, nan_r_items, 0, 0);
		switch (chosen_item)
		{
			case ITEM_NAN_RESTORE:
				break;
			case ITEM_NAN_RESBACK:
				return;
		}
	}
}

char* 
nan_img_set(int tw_setting)
{
	int isTrue = 0;
	char* tmp_set = (char*)malloc(25);
	switch (tw_setting)
	{
		case ITEM_NAN_SYSTEM:
			strcpy(tmp_set, "[ ] system");
			isTrue = is_true(tw_nan_system_val);
			break;
		case ITEM_NAN_DATA:
			strcpy(tmp_set, "[ ] data");
			isTrue = is_true(tw_nan_data_val);
			break;
		case ITEM_NAN_CACHE:
			strcpy(tmp_set, "[ ] cache");
			isTrue = is_true(tw_nan_cache_val);
			break;
		case ITEM_NAN_BOOT:
			strcpy(tmp_set, "[ ] boot");
			isTrue = is_true(tw_nan_boot_val);
			break;
		case ITEM_NAN_WIMAX:
			strcpy(tmp_set, "[ ] wimax");
			isTrue = is_true(tw_nan_wimax_val);
			break;
		case ITEM_NAN_RECOVERY:
			strcpy(tmp_set, "[ ] recovery");
			isTrue = is_true(tw_nan_recovery_val);
			break;
		case ITEM_NAN_SDEXT:
			strcpy(tmp_set, "[ ] sd-ext");
			isTrue = is_true(tw_nan_sdext_val);
			break;
		case ITEM_NAN_ANDSEC:
			strcpy(tmp_set, "[ ] .android-secure");
			isTrue = is_true(tw_nan_andsec_val);
			break;
	}
	if (isTrue) {
		tmp_set[1] = 'x';
	}
	return tmp_set;
}

void
nandroid_string()
{
	strcpy(tw_nandroid_string, "/sbin/nandroid-mobile.sh -b --nomisc --nosplash1 --nosplash2 --defaultinput");
	if (is_true(tw_nan_system_val) == 0) {
		strcat(tw_nandroid_string, " --nosystem");
	}
	if (is_true(tw_nan_data_val) == 0) {
		strcat(tw_nandroid_string, " --nodata");
	}
	if (is_true(tw_nan_cache_val) == 0) {
		strcat(tw_nandroid_string, " --nocache");
	}
	if (is_true(tw_nan_boot_val) == 0) {
		strcat(tw_nandroid_string, " --noboot");
	}
	if (is_true(tw_nan_wimax_val) == 0) {
		strcat(tw_nandroid_string, " --nowimax");
	}
	if (is_true(tw_nan_recovery_val) == 0) {
		strcat(tw_nandroid_string, " --norecovery");
	}
	if (is_true(tw_nan_sdext_val) == 1) {
		strcat(tw_nandroid_string, " -e");
	}
	if (is_true(tw_nan_andsec_val) == 1) {
		strcat(tw_nandroid_string, " -a");
	}
}

// kang'd this from recovery.c cuz there wasnt a recovery.h!
int
erase_volume(const char *volume) {
    ui_set_background(BACKGROUND_ICON_INSTALLING);
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
                                "",
                                "Choose Partition to Format: ",
    							"",
                                NULL };
	
    char* part_items[] = {  "Format Cache (/cache)",
                            "Format Data (/data)",
                            "Format Sdcard (/sdcard)",
                            "Format System (/system)",
						    "<- Back To Main Menu",
						    NULL };
	
	save_up_a_level_menu_location(ITEM_FORMAT_BACK);
	for (;;)
	{
		int chosen_item = get_menu_selection(part_headers, part_items, 0, 0);
		switch (chosen_item)
		{
			case ITEM_FORMAT_CACHE:
                confirm_format("Cache", "/cache");
                break;
			case ITEM_FORMAT_DATA:
                confirm_format("Data", "/data");
                break;
			case ITEM_FORMAT_SDCARD:
                confirm_format("Sdcard", "/sdcard");
                break;
			case ITEM_FORMAT_SYSTEM:
                confirm_format("System", "/system");
                break;
			case ITEM_FORMAT_BACK:
				return;
		}
	}
}

void
confirm_format(char* volume_name, char* volume_path) {

    char* headers[] = { "Confirm Format of Partition: ",
                        volume_name,
                        "",
                        "  THIS CAN NOT BE UNDONE!",
                        "",
                        NULL };

    char* items[] = {   "No",
                        "Yes -- Permanently Format",
                        NULL };
    //save_up_a_level_menu_location(0); either menu option results in the same menu return, but if they choose yes then the back button gets out of sync
    int chosen_item = get_menu_selection(headers, items, 1, 0);
    if (chosen_item != 1) {
        return;
    }
    else {
        ui_print("\n-- Wiping %s Partition...\n", volume_name);
        erase_volume(volume_path);
        ui_print("-- %s Partition Wipe Complete!\n", volume_name);
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
    time_t now;
    now = time(0);
    struct tm *current;
    current = localtime(&now);
    int hour = current->tm_hour;
    // Account for timezone
    int zone_off = atoi(tw_time_zone_val);
    hour += zone_off;
    if (hour < 0)
        hour += 24;
    
    //ui_print("Init H: %i\nZone Off: %i\nCor Time: %i", current->tm_hour, zone_off, hour);

    // HACK: Not sure if this could be a possible memory leak
    char* full_cap_s = (char*)malloc(30);
    char full_cap_a[30];
    sprintf(full_cap_a, "Battery Level: %s%% @ %i:%i", cap_s, hour, current->tm_min);

    strcpy(full_cap_s, full_cap_a);

    //ui_print("\n%s\n", full_cap_s);
    
    return full_cap_s;
}
