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

// Settings Constants 
static const char* TW_SETTINGS_FILE = "/sdcard/nandroid/.twrs"; // Actual File
static const int TW_MAX_SETTINGS_CHARS = 255; // Max Character Length Per Line
static const int TW_MAX_NUM_SETTINGS = 4; // Total Number of Settings (Change this as we add more settings)
static const int TW_SIGNED_ZIP = 0; // Zip Signed Toggle (Constant number corresponds to line number in file .twrs)
static const int TW_NAN_SYSTEM = 1; // system is backed up during nandroid (Constant number corresponds to line number in file .twrs)
static const int TW_NAN_DATA = 2; // data is backed up during nandroid (Constant number corresponds to line number in file .twrs)
static const int TW_ZIP_LOCATION = 3; // Last location zip flashed from (remembers last folder) (Constant number corresponds to line number in file .twrs)

static char* tw_signed_zip_val = "0"; // Variable that holds value, default is defined
static char* tw_nan_system_val = "1"; //
static char* tw_nan_data_val = "1"; //
static char* tw_zip_location_val = "/sdcard"; //

// Write Settings to file Function
void
write_s_file() {
	FILE *fp; // define file
	fp = fopen(TW_SETTINGS_FILE, "w"); // open file, create if not exist, if exist, overwrite contents
	if (fp == NULL) {
		LOGI("--> Can not open settings file to write.\n"); // Can't open/create file, default settings still loaded into memory.
	} else {
		int i = 0;
		while(i < TW_MAX_NUM_SETTINGS) {
			if (i == TW_SIGNED_ZIP) {
				fputs(tw_signed_zip_val, fp); // write tw_signed_zip_val to fp
				LOGI("--> TW_SIGNED_ZIP (%d) = %s\n", i, tw_signed_zip_val);
			} else if (i == TW_NAN_SYSTEM) {
				fputs(tw_nan_system_val, fp); //
				LOGI("--> TW_NAN_SYSTEM (%d) = %s\n", i, tw_nan_system_val);
			} else if (i == TW_NAN_DATA) {
				fputs(tw_nan_data_val, fp); //
				LOGI("--> TW_NAN_DATA (%d) = %s\n", i, tw_nan_data_val);
			} else if (i == TW_ZIP_LOCATION) {
				fputs(tw_zip_location_val, fp); //
				LOGI("--> TW_ZIP_LOCATION (%d) = %s\n", i, tw_zip_location_val);
			}
			fputs("\n", fp); // add a carriage return to finish line
			i++; // increment loop
		}
		fclose(fp); // close file
		LOGI("--> Wrote configuration file to: %s\n\n", TW_SETTINGS_FILE); // log
	}
}

// Read from Settings file Function
void
read_s_file() {
	FILE *fp; // define file
	if (ensure_path_mounted(SDCARD_ROOT) != 0) {
		LOGI("--> Can not mount /sdcard, running on default settings\n"); // Can't mount sdcard, default settings should be unchanged.
	} else {
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
			    if (i == TW_SIGNED_ZIP) {
				    tw_signed_zip_val = s_line; // i = 0
				    signature_check_enabled = strcmp(tw_signed_zip_val, "0");
				    LOGI("--> TW_SIGNED_ZIP (%d) = %s\n", i, tw_signed_zip_val); // log in /tmp/recovery.log
			    } else if (i == TW_NAN_SYSTEM) {
				    tw_nan_system_val = s_line; // i = 1
				    LOGI("--> TW_NAN_SYSTEM (%d) = %s\n", i, tw_nan_system_val);
                } else if (i == TW_NAN_DATA) {
					tw_nan_data_val = s_line; //  i = 2
					LOGI("--> TW_NAN_DATA (%d) = %s\n", i, tw_nan_data_val);
				} else if (i == TW_ZIP_LOCATION) {
					tw_zip_location_val = s_line; // i = 3
					LOGI("--> TW_ZIP_LOCATION (%d) = %s\n\n", i, tw_zip_location_val);
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
        return -1;
    }

    if ((write(fd, vol->device, strlen(vol->device)) < 0) &&
        (!vol->device2 || (write(fd, vol->device, strlen(vol->device2)) < 0))) {
        LOGE("Unable to write to ums lunfile: (%s)", strerror(errno));
        close(fd);
        return -1;

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
        			return -1;
        		}

        		char ch = 0;
        		if (write(fd, &ch, 1) < 0) {
        			LOGE("Unable to write to ums lunfile: (%s)", strerror(errno));
        			close(fd);
        			return -1;
        		}
        		ui_print("\nUSB as storage device unmounted!\n");
        		break;
        	}
        }
    }
}

// TOGGLE SIGNATURE CHECK
int signature_check_enabled = 1;

void
toggle_signature_check()
{
    signature_check_enabled = !signature_check_enabled;
    ui_print("Signature Check: %s\n", signature_check_enabled ? "Enabled" : "Disabled");
    if (signature_check_enabled) {
        tw_signed_zip_val = "1";
    } else {
        tw_signed_zip_val = "0";
    }
    write_s_file();
}

// INSTALL ZIP MENU
#define ITEM_CHOOSE_ZIP      0
#define ITEM_TOGGLE_SIG      1

void install_zip_menu()
{
    int result;
    int chosen_item = 2;
    char* toggle_menu_option = "Toggle Signature Verification - Currently ";
    strcat(toggle_menu_option, signature_check_enabled ? "Enabled" : "Disabled");
    char* MENU_INSTALL_ZIP[] = {  "Choose zip To Flash",
    		                      toggle_menu_option,
                                   "<-Back To Main Menu",
                                    NULL };
    static char* MENU_FLASH_HEADERS[] = {  "Flash zip From SD card",
                                "",
                                NULL
    };
    for (;;)
    {
        int chosen_item = get_menu_selection(MENU_FLASH_HEADERS, MENU_INSTALL_ZIP, 0, 0);
        switch (chosen_item)
        {
        // force item 2 always to go "back"
        if (chosen_item == 2) {
            result = -1;
            break;
            }

            case ITEM_CHOOSE_ZIP:
                ;
                int status = sdcard_directory(SDCARD_ROOT);
                if (status >= 0) {
                    if (status != INSTALL_SUCCESS) {
                        ui_set_background(BACKGROUND_ICON_ERROR);
                        ui_print("Installation aborted.\n");
                    } else if (!ui_text_visible()) {
                        return;  // reboot if logs aren't visible
                    } else {
                        ui_print("\nInstall from sdcard complete.\n");
                    }
                }
                break;
            case ITEM_TOGGLE_SIG:
                toggle_signature_check();
                // TO DO: need to add code to update the menu to display the toggle change
                break;
            default:
                return;
        }
    }
}

void wipe_dalvik_cache()
{
       ensure_path_mounted("/data");
       ensure_path_mounted("/cache");
       ui_print("\n-- Wiping Dalvik Cache...\n");
       __system("rm -rf /data/dalvik-cache");
       ui_print("Cleaned: /data/dalvik-cache...\n");
       __system("rm -rf /cache/dalvik-cache");
       ui_print("Cleaned: /cache/dalvik-cache...\n");

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
       ui_print("-- Dalvik Cache Wipe Complete!\n");
       if (!ui_text_visible()) return;
}

// REBOOT MENU
char* MENU_REBOOT[] = {  "Reboot To System",
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
    ui_print("No Battery Stats Found, No Need To Wipe.\n");
    } else {
    remove("/data/system/batterystats.bin");
    ui_print("Cleared: Battery Stats...\n");
    ensure_path_unmounted("/data");
    }
}  

// ADVANCED MENU
char* MENU_ADVANCED[] = {  "Reboot Menu",
                           "Wipe Battery Stats",
                           "Wipe Rotation Data",
                           "<-Back To Main Menu",
                                NULL };
#define ITEM_REBOOT_MENU       0
#define ITEM_BATTERY_STATS     1
#define ITEM_ROTATE_DATA       2

void advanced_menu()
{
    int result;
    int chosen_item = 3;

    static char* MENU_ADVANCED_HEADERS[] = {  "Advanced Options",
                                "",
                                NULL
    };
    for (;;)
    {
        int chosen_item = get_menu_selection(MENU_ADVANCED_HEADERS, MENU_ADVANCED, 0, 0);
        switch (chosen_item)
        {
        // force item 3 always to go "back"
        if (chosen_item == 3) {
            result = -1;
            break;
            }

            case ITEM_REBOOT_MENU:
                reboot_menu();
                break;

            case ITEM_BATTERY_STATS:
                wipe_battery_stats();
                break;
            default:
                return;
        }
    }
}
