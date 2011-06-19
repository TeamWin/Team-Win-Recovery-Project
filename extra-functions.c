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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cutils/misc.h"
#include "cutils/properties.h"
#include <signal.h>
#include <ctype.h>
#include <getopt.h>
#include <linux/input.h>
#include <dirent.h>
#include <sys/reboot.h>
#include <time.h>
#include <termios.h>
#include <sys/limits.h>

#include "bootloader.h"
#include "install.h"
#include "minui/minui.h"
#include "minzip/DirUtil.h"
#include "minzip/Zip.h"
#include "roots.h"
#include "common.h"
#include "recovery_ui.h"
#include "extra-functions.h"

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
    ui_print("\nMounting USB as storage device!\n");

    int fd;
    Volume *vol = volume_for_path("/sdcard"); 
    if ((fd = open(CUSTOM_LUN_FILE"0/file", O_WRONLY)) < 0) {
                   LOGE("Unable to open ums lunfile (%s)", strerror(errno));
                   return -1;
               }

                   if ((write(fd, vol->device, strlen(vol->device)) < 0) &&
                   (!vol->device2 || (write(fd, vol->device, strlen(vol->device2)) < 0))) {
                   LOGE("Unable to write to ums lunfile (%s)", strerror(errno));
                   close(fd);
                   return -1;
               
               } else {

                   ui_clear_key_queue();
                   ui_print("\nUSB mounted as storage device!\n");
                   ui_print("\nPress Power to disable,");
                   ui_print("\nand return to menu\n");

                                          for (;;) {
                                          int key = ui_wait_key();
                                          if (key == KEY_POWER) {
                                          ui_print("\nDisabling USB storage device\n");

                                          if ((fd = open(CUSTOM_LUN_FILE"0/file", O_WRONLY)) < 0) {
                                          LOGE("Unable to open ums lunfile (%s)", strerror(errno));
                                          return -1;
                                      }

                                          char ch = 0;
                                          if (write(fd, &ch, 1) < 0) {
                                          LOGE("Unable to write to ums lunfile (%s)", strerror(errno));
                                          close(fd);
                                          return -1;
                                      }
                                          ui_print("\nUSB storage device unmounted!\n");
                                      }
                                          break;
                                      }
                    }
}


// INSTALL ZIP MENU
static const char *SDCARD_ROOT = "/sdcard";

char* MENU_INSTALL_ZIP[] = {  "Choose zip To Flash",
                               "Toggle Signature Verification",
                               "-Back",

                                NULL };
#define ITEM_CHOOSE_ZIP       0
#define ITEM_TOGGLE_SIG       1
#define ITEM_BACK_MAIN        2

void install_zip_menu()
{
    static char* MENU_FLASH_HEADERS[] = {  "Flash zip From SD card",
                                "",
                                NULL
    };
    for (;;)
    {
        int chosen_item = get_menu_selection(MENU_FLASH_HEADERS, MENU_INSTALL_ZIP, 0, 0);
        switch (chosen_item)
        {
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
//signature check
                break;
            case ITEM_BACK_MAIN:
                 prompt_and_wait();
                break;
            default:
                return;
        }
    }
}

