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
static const char *TW_SETTINGS_FILE = "/sdcard/nandroid/.twrs"; // Actual File
static const int TW_MAX_SETTINGS_CHARS = 255; // Max Character Length Per Line
static const int TW_MAX_NUM_SETTINGS = 4; // Total Number of Settings (Change this as we add more settings)
static const int TW_SIGNED_ZIP = 0; // Zip Signed Toggle (Constant number corresponds to line number in file .twrs)
static const int TW_NAN_SYSTEM = 1; // system is backed up during nandroid (Constant number corresponds to line number in file .twrs)
static const int TW_NAN_DATA = 2; // data is backed up during nandroid (Constant number corresponds to line number in file .twrs)
static const int TW_ZIP_LOCATION = 3; // Last location zip flashed from (remembers last folder) (Constant number corresponds to line number in file .twrs)

static char *tw_signed_zip_val = "0"; // Variable that holds value, default is defined
static char *tw_nan_system_val = "1"; //
static char *tw_nan_data_val = "1"; //
static char *tw_zip_location_val = "/sdcard"; //

// Read from Settings file Function
void
read_s_file() {
	FILE *fp; // define file
	if (ensure_path_mounted(SDCARD_ROOT) != 0) {
		LOGI("Can not mount /sdcard, running on default settings\n"); // Can't mount sdcard, default settings should be unchanged.
	} else {
		fp = fopen(TW_SETTINGS_FILE, "r"); // Open file for read
		if (fp == NULL) {
			LOGI("Can not open settings file\n"); // Can't open file, default settings should be unchanged.
		} else {
			char s_line[TW_MAX_SETTINGS_CHARS+2]; // Set max characters + 2 (because of terminating and carriage return)
			int i = 0;
			while(i < TW_MAX_NUM_SETTINGS) {
				fgets(s_line, TW_MAX_SETTINGS_CHARS+2, fp); // Read a line from file
				if (i == TW_SIGNED_ZIP) {
					tw_signed_zip_val = s_line; // i = 0
					LOGI("--> TW_SIGNED_ZIP (%d) = %s", i, tw_signed_zip_val); // log in /tmp/recovery.log
				} else if (i == TW_NAN_SYSTEM) {
					tw_nan_system_val = s_line; // i = 1
					LOGI("--> TW_NAN_SYSTEM (%d) = %s", i, tw_nan_system_val);
				} else if (i == TW_NAN_DATA) {
					tw_nan_data_val = s_line; //  i = 2
					LOGI("--> TW_NAN_DATA (%d) = %s", i, tw_nan_data_val);
				} else if (i == TW_ZIP_LOCATION) {
					tw_zip_location_val = s_line; // i = 3
					LOGI("--> TW_ZIP_LOCATION (%d) = %s", i, tw_zip_location_val);
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

// toggle signature check
int signature_check_enabled = 1;

void
toggle_signature_check()
{
    signature_check_enabled = !signature_check_enabled;
    ui_print("Signature Check: %s\n", signature_check_enabled ? "Enabled" : "Disabled");
}

// INSTALL ZIP MENU

char* MENU_INSTALL_ZIP[] = {  "Choose zip To Flash",
                               "Toggle Signature Verification",
                               "<-Back To Main Menu",
                                NULL };
#define ITEM_CHOOSE_ZIP      0
#define ITEM_TOGGLE_SIG      1

void install_zip_menu()
{
    int result;
    int chosen_item = 2;

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
