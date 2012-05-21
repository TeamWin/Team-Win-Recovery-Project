/*
 * Copyright (C) 2007 The Android Open Source Project
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/statfs.h>

#include "bootloader.h"
#include "common.h"
#include "cutils/properties.h"
#include "install.h"
#include "minuitwrp/minui.h"
#include "minzip/DirUtil.h"
#include "roots.h"
#include "format.h"
#include "themes.h"
#include "recovery_ui.h"
#include "encryptedfs_provisioning.h"
#include "tw_reboot.h"

#include "extra-functions.h"
#include "data.h"
#include "ddftw.h"
#include "backstore.h"

int notError;

int gui_init(void);
int gui_loadResources(void);
int gui_start(void);
int gui_console_only(void);

static const struct option OPTIONS[] = {
  { "send_intent", required_argument, NULL, 's' },
  { "update_package", required_argument, NULL, 'u' },
  { "wipe_data", no_argument, NULL, 'w' },
  { "wipe_cache", no_argument, NULL, 'c' },
  { "set_encrypted_filesystems", required_argument, NULL, 'e' },
  { "show_text", no_argument, NULL, 't' },
  { NULL, 0, NULL, 0 },
};

static const char *COMMAND_FILE = "/cache/recovery/command";
static const char *INTENT_FILE = "/cache/recovery/intent";
static const char *LOG_FILE = "/cache/recovery/log";
static const char *LAST_LOG_FILE = "/cache/recovery/last_log";
//static const char *SDCARD_ROOT = "/sdcard";
static const char *TEMPORARY_LOG_FILE = "/tmp/recovery.log";

/*
 * The recovery tool communicates with the main system through /cache files.
 *   /cache/recovery/command - INPUT - command line for tool, one arg per line
 *   /cache/recovery/log - OUTPUT - combined log file from recovery run(s)
 *   /cache/recovery/intent - OUTPUT - intent that was passed in
 *
 * The arguments which may be supplied in the recovery.command file:
 *   --send_intent=anystring - write the text out to recovery.intent
 *   --update_package=path - verify install an OTA package file
 *   --wipe_data - erase user data (and cache), then reboot
 *   --wipe_cache - wipe cache (but not user data), then reboot
 *   --set_encrypted_filesystem=on|off - enables / diasables encrypted fs
 *
 * After completing, we remove /cache/recovery/command and reboot.
 * Arguments may also be supplied in the bootloader control block (BCB).
 * These important scenarios must be safely restartable at any point:
 *
 * FACTORY RESET
 * 1. user selects "factory reset"
 * 2. main system writes "--wipe_data" to /cache/recovery/command
 * 3. main system reboots into recovery
 * 4. get_args() writes BCB with "boot-recovery" and "--wipe_data"
 *    -- after this, rebooting will restart the erase --
 * 5. erase_volume() reformats /data
 * 6. erase_volume() reformats /cache
 * 7. finish_recovery() erases BCB
 *    -- after this, rebooting will restart the main system --
 * 8. main() calls reboot() to boot main system
 *
 * OTA INSTALL
 * 1. main system downloads OTA package to /cache/some-filename.zip
 * 2. main system writes "--update_package=/cache/some-filename.zip"
 * 3. main system reboots into recovery
 * 4. get_args() writes BCB with "boot-recovery" and "--update_package=..."
 *    -- after this, rebooting will attempt to reinstall the update --
 * 5. install_package() attempts to install the update
 *    NOTE: the package install must itself be restartable from any point
 * 6. finish_recovery() erases BCB
 *    -- after this, rebooting will (try to) restart the main system --
 * 7. ** if install failed **
 *    7a. prompt_and_wait() shows an error icon and waits for the user
 *    7b; the user reboots (pulling the battery, etc) into the main system
 * 8. main() calls maybe_install_firmware_update()
 *    ** if the update contained radio/hboot firmware **:
 *    8a. m_i_f_u() writes BCB with "boot-recovery" and "--wipe_cache"
 *        -- after this, rebooting will reformat cache & restart main system --
 *    8b. m_i_f_u() writes firmware image into raw cache partition
 *    8c. m_i_f_u() writes BCB with "update-radio/hboot" and "--wipe_cache"
 *        -- after this, rebooting will attempt to reinstall firmware --
 *    8d. bootloader tries to flash firmware
 *    8e. bootloader writes BCB with "boot-recovery" (keeping "--wipe_cache")
 *        -- after this, rebooting will reformat cache & restart main system --
 *    8f. erase_volume() reformats /cache
 *    8g. finish_recovery() erases BCB
 *        -- after this, rebooting will (try to) restart the main system --
 * 9. main() calls reboot() to boot main system
 *
 * SECURE FILE SYSTEMS ENABLE/DISABLE
 * 1. user selects "enable encrypted file systems"
 * 2. main system writes "--set_encrypted_filesystems=on|off" to
 *    /cache/recovery/command
 * 3. main system reboots into recovery
 * 4. get_args() writes BCB with "boot-recovery" and
 *    "--set_encrypted_filesystems=on|off"
 *    -- after this, rebooting will restart the transition --
 * 5. read_encrypted_fs_info() retrieves encrypted file systems settings from /data
 *    Settings include: property to specify the Encrypted FS istatus and
 *    FS encryption key if enabled (not yet implemented)
 * 6. erase_volume() reformats /data
 * 7. erase_volume() reformats /cache
 * 8. restore_encrypted_fs_info() writes required encrypted file systems settings to /data
 *    Settings include: property to specify the Encrypted FS status and
 *    FS encryption key if enabled (not yet implemented)
 * 9. finish_recovery() erases BCB
 *    -- after this, rebooting will restart the main system --
 * 10. main() calls reboot() to boot main system
 */

static const int MAX_ARG_LENGTH = 4096;
static const int MAX_ARGS = 100;

// open a given path, mounting partitions as necessary
FILE*
fopen_path(const char *path, const char *mode) {
    if (ensure_path_mounted(path) != 0) {
        LOGE("Can't mount %s\n", path);
        return NULL;
    }

    // When writing, try to create the containing directory, if necessary.
    // Use generous permissions, the system (init.rc) will reset them.
    if (strchr("wa", mode[0])) dirCreateHierarchy(path, 0777, NULL, 1);

    FILE *fp = fopen(path, mode);
    return fp;
}

// close a file, log an error if the error indicator is set
static void
check_and_fclose(FILE *fp, const char *name) {
    fflush(fp);
    if (ferror(fp)) LOGE("Error in %s\n(%s)\n", name, strerror(errno));
    fclose(fp);
}

// command line args come from, in decreasing precedence:
//   - the actual command line
//   - the bootloader control block (one per line, after "recovery")
//   - the contents of COMMAND_FILE (one per line)
static void
get_args(int *argc, char ***argv) {
    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    get_bootloader_message(&boot);  // this may fail, leaving a zeroed structure

    if (boot.command[0] != 0 && boot.command[0] != 255) {
        LOGI("Boot command: %.*s\n", sizeof(boot.command), boot.command);
    }

    if (boot.status[0] != 0 && boot.status[0] != 255) {
        LOGI("Boot status: %.*s\n", sizeof(boot.status), boot.status);
    }

    // --- if arguments weren't supplied, look in the bootloader control block
    if (*argc <= 1) {
        boot.recovery[sizeof(boot.recovery) - 1] = '\0';  // Ensure termination
        const char *arg = strtok(boot.recovery, "\n");
        if (arg != NULL && !strcmp(arg, "recovery")) {
            *argv = (char **) malloc(sizeof(char *) * MAX_ARGS);
            (*argv)[0] = strdup(arg);
            for (*argc = 1; *argc < MAX_ARGS; ++*argc) {
                if ((arg = strtok(NULL, "\n")) == NULL) break;
                (*argv)[*argc] = strdup(arg);
            }
            LOGI("Got arguments from boot message\n");
        } else if (boot.recovery[0] != 0 && boot.recovery[0] != 255) {
            LOGE("Bad boot message\n\"%.20s\"\n", boot.recovery);
        }
    }

    // --- if that doesn't work, try the command file
    if (*argc <= 1) {
        FILE *fp = fopen_path(COMMAND_FILE, "r");
        if (fp != NULL) {
            char *argv0 = (*argv)[0];
            *argv = (char **) malloc(sizeof(char *) * MAX_ARGS);
            (*argv)[0] = argv0;  // use the same program name

            char buf[MAX_ARG_LENGTH];
            for (*argc = 1; *argc < MAX_ARGS; ++*argc) {
                if (!fgets(buf, sizeof(buf), fp)) break;
                (*argv)[*argc] = strdup(strtok(buf, "\r\n"));  // Strip newline.
            }

            check_and_fclose(fp, COMMAND_FILE);
            LOGI("Got arguments from %s\n", COMMAND_FILE);
        }
    }

    // --> write the arguments we have back into the bootloader control block
    // always boot into recovery after this (until finish_recovery() is called)
    strlcpy(boot.command, "boot-recovery", sizeof(boot.command));
    strlcpy(boot.recovery, "recovery\n", sizeof(boot.recovery));
    int i;
    for (i = 1; i < *argc; ++i) {
        strlcat(boot.recovery, (*argv)[i], sizeof(boot.recovery));
        strlcat(boot.recovery, "\n", sizeof(boot.recovery));
    }
    set_bootloader_message(&boot);
}

// How much of the temp log we have copied to the copy in cache.
static void
copy_log_file(const char* destination, int append) {
    FILE *log = fopen_path(destination, append ? "a" : "w");
    if (log == NULL) {
        LOGE("Can't open %s\n", destination);
    } else {
        FILE *tmplog = fopen(TEMPORARY_LOG_FILE, "r");
        if (tmplog == NULL) {
            LOGE("Can't open %s\n", TEMPORARY_LOG_FILE);
        } else {
            if (append) {
                fseek(tmplog, tmplog_offset, SEEK_SET);  // Since last write
            }
            char buf[4096];
            while (fgets(buf, sizeof(buf), tmplog)) fputs(buf, log);
            if (append) {
                tmplog_offset = ftell(tmplog);
            }
            check_and_fclose(tmplog, TEMPORARY_LOG_FILE);
        }
        check_and_fclose(log, destination);
    }
}


// clear the recovery command and prepare to boot a (hopefully working) system,
// copy our log file to cache as well (for the system to read), and
// record any intent we were asked to communicate back to the system.
// this function is idempotent: call it as many times as you like.
void
finish_recovery(const char *send_intent) {
    // By this point, we're ready to return to the main system...
    if (send_intent != NULL) {
        FILE *fp = fopen_path(INTENT_FILE, "w");
        if (fp == NULL) {
            LOGE("Can't open %s\n", INTENT_FILE);
        } else {
            fputs(send_intent, fp);
            check_and_fclose(fp, INTENT_FILE);
        }
    }

    // Copy logs to cache so the system can find out what happened.
    copy_log_file(LOG_FILE, true);
    copy_log_file(LAST_LOG_FILE, false);
    chmod(LAST_LOG_FILE, 0640);

    // Reset to mormal system boot so recovery won't cycle indefinitely.
    struct bootloader_message boot;
    memset(&boot, 0, sizeof(boot));
    set_bootloader_message(&boot);

    // Remove the command file, so recovery won't repeat indefinitely.
    if (ensure_path_mounted(COMMAND_FILE) != 0 ||
        (unlink(COMMAND_FILE) && errno != ENOENT)) {
        LOGW("Can't unlink %s\n", COMMAND_FILE);
    }

    sync();  // For good measure.
}

char**
prepend_title(const char** headers) {
    char* title1 = (char*)malloc(40);
    strcpy(title1, "Team Win Recovery Project (twrp) v");
    char* header1 = strcat(title1, DataManager_GetStrValue(TW_VERSION_VAR));
    char* title[] = { header1,
                      "Based on Android System Recovery <"
                      EXPAND(RECOVERY_API_VERSION) "e>",
                      "", //
                      print_batt_cap(),
                      "", //
                      NULL };

    // count the number of lines in our title, plus the
    // caller-provided headers.
    int count = 0;
    char** p;
    for (p = title; *p; ++p, ++count);
    for (p = (char**) headers; *p; ++p, ++count);

    char** new_headers = malloc((count+1) * sizeof(char*));
    char** h = new_headers;
    for (p = title; *p; ++p, ++h) *h = *p;
    for (p = (char**) headers; *p; ++p, ++h) *h = *p;
    *h = NULL;

    return new_headers;
}

int 
get_menu_selection(char** headers, char** items, int menu_only,
                   int initial_selection) {
    // throw away keys pressed previously, so user doesn't
    // accidentally trigger menu items.
    ui_clear_key_queue();

    ui_start_menu(headers, items, initial_selection);
    int selected = initial_selection;
    int chosen_item = -1;
    
    while (chosen_item < 0) {
        int key = ui_wait_key();
        int visible = ui_text_visible();

        int action = device_handle_key(key, visible);
        if (action < 0) {
            switch (action) {
                case HIGHLIGHT_UP:
                    --selected;
                    selected = ui_menu_select(selected);
                    break;
                case HIGHLIGHT_DOWN:
                    ++selected;
                    selected = ui_menu_select(selected);
                    break;
                case KEY_POWER:
                case SELECT_ITEM:
                    chosen_item = selected;
                    break;
                case UP_A_LEVEL:
                	if (menu_loc_idx != 0)
                	{
                		chosen_item = menu_loc[menu_loc_idx];
                	}
                    break;
                case HOME_MENU:
                	if (menu_loc_idx != 0)
                	{
                		go_home = 1;
                		chosen_item = menu_loc[menu_loc_idx];
                	}
                    break;
                case MENU_MENU:
                	if (menu_loc_idx == 0)
                	{
                	    return 3;
                	} else
                	{
                    	go_home = 1;
                    	go_menu = 1;
                    	chosen_item = menu_loc[menu_loc_idx];
                	}
                    break;
                case NO_ACTION:
                    break;
            }
        } else if (!menu_only) {
            chosen_item = action;
        }
    }

    ui_end_menu();
    return chosen_item;
}

static int compare_string(const void* a, const void* b) {
    return strcasecmp(*(const char**)a, *(const char**)b);
}

int
sdcard_directory(const char* path) {
    mount_current_storage();

    const char* MENU_HEADERS[] = { "Choose a package to install:",
                                   path,
                                   NULL };
    DIR* d;
    struct dirent* de;
    d = opendir(path);
    if (d == NULL) {
        LOGE("error opening %s: %s\n", path, strerror(errno));
        unmount_current_storage();
        return 0;
    }

    char** headers = prepend_title(MENU_HEADERS);

    int s_size = 0;
    int s_alloc = 10;
    char** sele = malloc(s_alloc * sizeof(char*));
    int d_size = 0;
    int d_alloc = 10;
    char** dirs = malloc(d_alloc * sizeof(char*));
    int z_size = 0;
    int z_alloc = 10;
    char** zips = malloc(z_alloc * sizeof(char*));
    if (get_new_zip_dir > 0)
    {
    	sele[0] = strdup("[SELECT CURRENT FOLDER]");
    	s_size++;
    }
	sele[s_size] = strdup("../");
    inc_menu_loc(s_size);
	s_size++;
    
    while ((de = readdir(d)) != NULL) {
        int name_len = strlen(de->d_name);

        if (de->d_type == DT_DIR) {
            // skip "." and ".." entries
            if (name_len == 1 && de->d_name[0] == '.') continue;
            if (name_len == 2 && de->d_name[0] == '.' &&
                de->d_name[1] == '.') continue;

            if (d_size >= d_alloc) {
                d_alloc *= 2;
                dirs = realloc(dirs, d_alloc * sizeof(char*));
            }
            dirs[d_size] = malloc(name_len + 2);
            strcpy(dirs[d_size], de->d_name);
            dirs[d_size][name_len] = '/';
            dirs[d_size][name_len+1] = '\0';
            ++d_size;
        } else if (de->d_type == DT_REG &&
                   name_len >= 4 &&
                   strncasecmp(de->d_name + (name_len-4), ".zip", 4) == 0 /* &&
				   get_new_zip_dir < 1 */) { // this commented out section would remove zips from the list if we're picking a default folder and not a zip
            if (z_size >= z_alloc) {
                z_alloc *= 2;
                zips = realloc(zips, z_alloc * sizeof(char*));
            }
            zips[z_size++] = strdup(de->d_name);
        }
    }
    closedir(d);

    notError = 0;
	
	qsort(dirs, d_size, sizeof(char*), compare_string);
	if (DataManager_GetIntValue(TW_SORT_FILES_BY_DATE_VAR)) {
		char* tempzip = malloc(z_alloc * sizeof(char*)); // sort zips by last modified date
		char file_path_name[PATH_MAX];
		int bubble1, bubble2, swap_flag = 1;
		struct stat read_file;
		time_t file1, file2;
		
		for (bubble1 = 0; bubble1 < z_size && swap_flag; bubble1++) {
			swap_flag = 0;
			for (bubble2 = 0; bubble2 < z_size - 1; bubble2++) {
				strcpy(file_path_name, path);
				strcat(file_path_name, "/");
				strcat(file_path_name, zips[bubble2]);
				stat(file_path_name, &read_file);
				file1 = read_file.st_mtime;
				strcpy(file_path_name, path);
				strcat(file_path_name, "/");
				strcat(file_path_name, zips[bubble2 + 1]);
				stat(file_path_name, &read_file);
				file2 = read_file.st_mtime;
				if (file1 < file2) {
					swap_flag = 1;
					tempzip = strdup(zips[bubble2]);
					zips[bubble2] = strdup(zips[bubble2 + 1]);
					zips[bubble2 + 1] = strdup(tempzip);
				}
			}
		}
	} else {
		qsort(zips, z_size, sizeof(char*), compare_string); // sort zips by filename
	}
    
    // append dirs to the zips list
    if (d_size + z_size + 1 > z_alloc) {
        z_alloc = d_size + z_size + 1;
        zips = realloc(zips, z_alloc * sizeof(char*));
    }
    memcpy(zips + z_size, dirs, d_size * sizeof(char*));
    free(dirs);
    z_size += d_size;
    zips[z_size] = NULL;

    if (z_size + s_size + 1 > s_alloc) {
        s_alloc = z_size + s_size + 1;
        sele = realloc(sele, s_alloc * sizeof(char*));
    }
    memcpy(sele + s_size, zips, z_size * sizeof(char*));
    s_size += z_size;
    sele[s_size] = NULL;
    
    int result;
    int chosen_item = 0;
    do {
        chosen_item = get_menu_selection(headers, sele, 1, chosen_item);

        char* item = sele[chosen_item];
        int item_len = strlen(item);

        if (chosen_item == 0) {          // item 0 is always "../"
            // go up but continue browsing (if the caller is sdcard_directory)
        	if (get_new_zip_dir > 0)
        	{
        		DataManager_SetStrValue(TW_ZIP_LOCATION_VAR, (char*) path);
                return 1;
        	} else {
            	dec_menu_loc();
                result = -1;
                break;
        	}
        } else if (chosen_item == 1 && get_new_zip_dir > 0) {
        	dec_menu_loc();
            result = -1;
            break;
        } else if (item[item_len-1] == '/') {
            // recurse down into a subdirectory
            char new_path[PATH_MAX];
            strlcpy(new_path, path, PATH_MAX);
            strlcat(new_path, "/", PATH_MAX);
            strlcat(new_path, item, PATH_MAX);
            new_path[strlen(new_path)-1] = '\0';  // truncate the trailing '/'
            result = sdcard_directory(new_path);
    	    if (go_home) { 
    	    	notError = 1;
    	        dec_menu_loc();
    	        if (get_new_zip_dir > 0)
    	        {
        	        return 1;
    	        } else {
        	        return 0;
    	        }
    	    }
            if (result >= 0) break;
        } else {
        	if (get_new_zip_dir < 1)
        	{
                // selected a zip file
                // the status to the caller.
                char new_path[PATH_MAX];

                strlcpy(multi_zip_array[multi_zip_index], path, PATH_MAX);
                strlcat(multi_zip_array[multi_zip_index], "/", PATH_MAX);
                strlcat(multi_zip_array[multi_zip_index], item, PATH_MAX);
				
                ui_print("Added %s\n", multi_zip_array[multi_zip_index]);
				multi_zip_index++;
				result = 1;
				break;
			}
        }
    } while (true);
    
    free(zips);
    free(sele);
    free(headers);

    //ensure_path_unmounted(SDCARD_ROOT);
    return result;
}

void
wipe_data(int confirm) {
    if (confirm) {
        static char** title_headers = NULL;

		ui_set_background(BACKGROUND_ICON_WIPE_CHOOSE);
		if (title_headers == NULL) {
			char* headers[] = { "Confirm wipe of all user data?",
                                "  THIS CAN NOT BE UNDONE.",
                                "",
                                NULL };
			title_headers = prepend_title((const char**)headers);
		}

		char* items[] = { " No",
                          " Yes -- delete all user data",   // [1]
                          NULL };

		int chosen_item = get_menu_selection(title_headers, items, 1, 0);
		if (chosen_item != 1) {
			return;
		}
	}
	ui_print("\n-- Factory reset started.\n");
	ui_set_background(BACKGROUND_ICON_WIPE);
	ui_print("Formatting /data...\n");

	//device_wipe_data(); // ??
#ifdef RECOVERY_SDCARD_ON_DATA
	// The sdcard is actually /data/media
	wipe_data_without_wiping_media();
#else
	erase_volume("/data");
	if (DataManager_GetIntValue(TW_HAS_DATADATA) == 1)
		erase_volume("/datadata");
#endif
	ui_print("Formatting /cache...\n");
	erase_volume("/cache");
	struct stat st;
	char ase_path[255];

	strcpy(ase_path, DataManager_GetSettingsStoragePath());
	strcat(ase_path, "/.android_secure");
	ensure_path_mounted("/sd-ext");
	if (stat(sde.blk,&st) == 0) {
		ui_print("Formatting /sd-ext...\n");
		tw_format(sde.fst,sde.blk);
	}
	mount_internal_storage();
	if (stat(ase_path, &st) == 0) {
		char command[255];
		ui_print("Formatting android_secure...\n");
		sprintf(command, "rm -rf %s/* && rm -rf %s/.*", ase_path, ase_path);
		__system(command);
	}
	ui_reset_progress();
	ui_print("-- Factory reset complete.\n");
}


void
prompt_and_wait() {

	// Main Menu
	#define START_FAKE_MAIN          0
	#define REALMENU_REBOOT     	 1

	go_reboot = 0;
    //ui_reset_progress();

	char** headers = prepend_title((const char**)MENU_HEADERS);
    char* MENU_ITEMS[] = {  "Start Recovery",
                            "Reboot",
                            NULL };
	
    for (;;) {

        go_home = 0;
        go_menu = 0;
        menu_loc_idx = 0;
		//ui_reset_progress();
    	
        /*int chosen_item = get_menu_selection(headers, MENU_ITEMS, 0, 0);

        // device-specific code may take some action here.  It may
        // return one of the core actions handled in the switch
        // statement below.
        chosen_item = device_perform_action(chosen_item);
		
		switch (chosen_item) {
            case START_FAKE_MAIN:
                show_fake_main_menu();
                break;
			
            case REALMENU_REBOOT:
                return;
		break;
        }*/

        if (go_reboot) {
			return;
		}
		show_fake_main_menu();
    }
}

static void
print_property(const char *key, const char *name, void *cookie) {
    printf("%s=%s\n", key, name);
}

// open recovery script file code
static const char *SCRIPT_FILE_CACHE = "/cache/recovery/openrecoveryscript";
static const char *SCRIPT_FILE_TMP = "/tmp/openrecoveryscript";
#define SCRIPT_COMMAND_SIZE 512

int check_for_script_file(void) {
	FILE *fp = fopen(SCRIPT_FILE_CACHE, "r");
	int ret_val = 0;
	char exec[512];

	if (fp != NULL) {
		ret_val = 1;
		LOGI("Script file found: '%s'\n", SCRIPT_FILE_CACHE);
		fclose(fp);
		// Copy script file to /tmp
		strcpy(exec, "cp ");
		strcat(exec, SCRIPT_FILE_CACHE);
		strcat(exec, " ");
		strcat(exec, SCRIPT_FILE_TMP);
		__system(exec);
		// Delete the file from /cache
		strcpy(exec, "rm ");
		strcat(exec, SCRIPT_FILE_CACHE);
		__system(exec);
	}
	return ret_val;
}

int run_script_file(void) {
	FILE *fp = fopen(SCRIPT_FILE_TMP, "r");
	int ret_val = 0, cindex, line_len, i, remove_nl;
	char script_line[SCRIPT_COMMAND_SIZE], command[SCRIPT_COMMAND_SIZE],
		 value[SCRIPT_COMMAND_SIZE], mount[SCRIPT_COMMAND_SIZE],
		 value1[SCRIPT_COMMAND_SIZE], value2[SCRIPT_COMMAND_SIZE];
	char *val_start, *tok;

	if (fp != NULL) {
		while (fgets(script_line, SCRIPT_COMMAND_SIZE, fp) != NULL && ret_val == 0) {
			cindex = 0;
			line_len = strlen(script_line);
			if (line_len < 2)
				continue; // there's a blank line or line is too short to contain a command
			//ui_print("script line: '%s'\n", script_line);
			for (i=0; i<line_len; i++) {
				if ((int)script_line[i] == 32) {
					cindex = i;
					i = line_len;
				}
			}
			memset(command, 0, sizeof(command));
			memset(value, 0, sizeof(value));
			if ((int)script_line[line_len - 1] == 10)
					remove_nl = 2;
				else
					remove_nl = 1;
			if (cindex != 0) {
				strncpy(command, script_line, cindex);
				LOGI("command is: '%s' and ", command);
				val_start = script_line;
				val_start += cindex + 1;
				strncpy(value, val_start, line_len - cindex - remove_nl);
				LOGI("value is: '%s'\n", value);
			} else {
				strncpy(command, script_line, line_len - remove_nl + 1);
				ui_print("command is: '%s' and there is no value\n", command);
			}
			if (strcmp(command, "install") == 0) {
				// Install zip
				struct statfs st;
				if (statfs(value, &st) != 0) {
					if (DataManager_GetIntValue(TW_HAS_DUAL_STORAGE)) {
						if (DataManager_GetIntValue(TW_USE_EXTERNAL_STORAGE)) {
							LOGI("Zip file not found on external storage, trying internal...\n");
							DataManager_SetIntValue(TW_USE_EXTERNAL_STORAGE, 0);
						} else {
							LOGI("Zip file not found on internal storage, trying external...\n");
							DataManager_SetIntValue(TW_USE_EXTERNAL_STORAGE, 1);
						}
						mount_current_storage();
					}
				}
				ui_print("Installing zip file '%s'\n", value);
				ret_val = install_zip_package(value);
				if (ret_val != INSTALL_SUCCESS) {
					LOGE("Error installing zip file '%s'\n", value);
					ret_val = 1;
				}
			} else if (strcmp(command, "wipe") == 0) {
				// Wipe
				if (strcmp(value, "cache") == 0 || strcmp(value, "/cache") == 0) {
					ui_print("-- Wiping Cache Partition...\n");
					erase_volume("/cache");
					ui_print("-- Cache Partition Wipe Complete!\n");
				} else if (strcmp(value, "dalvik") == 0 || strcmp(value, "dalvick") == 0 || strcmp(value, "dalvikcache") == 0 || strcmp(value, "dalvickcache") == 0) {
					ui_print("-- Wiping Dalvik Cache...\n");
					wipe_dalvik_cache();
					ui_print("-- Dalvik Cache Wipe Complete!\n");
				} else if (strcmp(value, "data") == 0 || strcmp(value, "/data") == 0 || strcmp(value, "factory") == 0 || strcmp(value, "factoryreset") == 0) {
					ui_print("-- Wiping Data Partition...\n");
					wipe_data(0);
					ui_print("-- Data Partition Wipe Complete!\n");
				} else {
					LOGE("Error with wipe command value: '%s'\n", value);
					ret_val = 1;
				}
			} else if (strcmp(command, "backup") == 0) {
				// Backup
				tok = strtok(value, " ");
				strcpy(value1, tok);
				tok = strtok(NULL, " ");
				if (tok != NULL) {
					memset(value2, 0, sizeof(value2));
					strcpy(value2, tok);
					line_len = strlen(tok);
					if ((int)value2[line_len - 1] == 10 || (int)value2[line_len - 1] == 13) {
						if ((int)value2[line_len - 1] == 10 || (int)value2[line_len - 1] == 13)
							remove_nl = 2;
						else
							remove_nl = 1;
					} else
						remove_nl = 0;
					strncpy(value2, tok, line_len - remove_nl);
					DataManager_SetStrValue(TW_BACKUP_NAME, value2);
					ui_print("Backup folder set to '%s'\n", value2);
				} else {
					DataManager_SetStrValue(TW_BACKUP_NAME, "0");
				}

				DataManager_SetIntValue(TW_BACKUP_SYSTEM_VAR, 0);
				DataManager_SetIntValue(TW_BACKUP_DATA_VAR, 0);
				DataManager_SetIntValue(TW_BACKUP_CACHE_VAR, 0);
				DataManager_SetIntValue(TW_BACKUP_RECOVERY_VAR, 0);
				DataManager_SetIntValue(TW_BACKUP_SP1_VAR, 0);
				DataManager_SetIntValue(TW_BACKUP_SP2_VAR, 0);
				DataManager_SetIntValue(TW_BACKUP_SP3_VAR, 0);
				DataManager_SetIntValue(TW_BACKUP_BOOT_VAR, 0);
				DataManager_SetIntValue(TW_BACKUP_ANDSEC_VAR, 0);
				DataManager_SetIntValue(TW_BACKUP_SDEXT_VAR, 0);
				DataManager_SetIntValue(TW_BACKUP_SDEXT_VAR, 0);
				DataManager_SetIntValue(TW_USE_COMPRESSION_VAR, 0);
				DataManager_SetIntValue(TW_SKIP_MD5_GENERATE_VAR, 0);

				ui_print("Setting backup options:\n");
				line_len = strlen(value1);
				for (i=0; i<line_len; i++) {
					if (value1[i] == 'S' || value1[i] == 's') {
						DataManager_SetIntValue(TW_BACKUP_SYSTEM_VAR, 1);
						ui_print("System\n");
					} else if (value1[i] == 'D' || value1[i] == 'd') {
						DataManager_SetIntValue(TW_BACKUP_DATA_VAR, 1);
						ui_print("Data\n");
					} else if (value1[i] == 'C' || value1[i] == 'c') {
						DataManager_SetIntValue(TW_BACKUP_CACHE_VAR, 1);
						ui_print("Cache\n");
					} else if (value1[i] == 'R' || value1[i] == 'r') {
						DataManager_SetIntValue(TW_BACKUP_RECOVERY_VAR, 1);
						ui_print("Recovery\n");
					} else if (value1[i] == '1') {
						DataManager_SetIntValue(TW_BACKUP_SP1_VAR, 1);
						ui_print("%s\n", "Special1");
					} else if (value1[i] == '2') {
						DataManager_SetIntValue(TW_BACKUP_SP2_VAR, 1);
						ui_print("%s\n", "Special2");
					} else if (value1[i] == '3') {
						DataManager_SetIntValue(TW_BACKUP_SP3_VAR, 1);
						ui_print("%s\n", "Special3");
					} else if (value1[i] == 'B' || value1[i] == 'b') {
						DataManager_SetIntValue(TW_BACKUP_BOOT_VAR, 1);
						ui_print("Boot\n");
					} else if (value1[i] == 'A' || value1[i] == 'a') {
						DataManager_SetIntValue(TW_BACKUP_ANDSEC_VAR, 1);
						ui_print("Android Secure\n");
					} else if (value1[i] == 'E' || value1[i] == 'e') {
						DataManager_SetIntValue(TW_BACKUP_SDEXT_VAR, 1);
						ui_print("SD-Ext\n");
					} else if (value1[i] == 'O' || value1[i] == 'o') {
						DataManager_SetIntValue(TW_USE_COMPRESSION_VAR, 1);
						ui_print("Compression is on\n");
					} else if (value1[i] == 'M' || value1[i] == 'm') {
						DataManager_SetIntValue(TW_SKIP_MD5_GENERATE_VAR, 1);
						ui_print("MD5 Generation is off\n");
					}
				}
				if (nandroid_back_exe() != 0) {
					ret_val = 1;
					LOGE("Backup failed!\n");
				} else
					ui_print("Backup complete!\n");
			} else if (strcmp(command, "restore") == 0) {
				// Restore
				tok = strtok(value, " ");
				strcpy(value1, tok);
				ui_print("Restoring '%s'\n", value1);
				DataManager_SetStrValue("tw_restore", value1);
				DataManager_SetIntValue(TW_SKIP_MD5_CHECK_VAR, 0);

				struct statfs st;
				char folder_path[512];

				strcpy(folder_path, value);
				if (folder_path[strlen(folder_path) - 1] == '/')
					strcat(folder_path, ".");
				else
					strcat(folder_path, "/.");
				if (statfs(folder_path, &st) != 0) {
					if (DataManager_GetIntValue(TW_HAS_DUAL_STORAGE)) {
						if (DataManager_GetIntValue(TW_USE_EXTERNAL_STORAGE)) {
							LOGI("Backup folder '%s' not found on external storage, trying internal...\n", folder_path);
							DataManager_SetIntValue(TW_USE_EXTERNAL_STORAGE, 0);
						} else {
							LOGI("Backup folder '%s' not found on internal storage, trying external...\n", folder_path);
							DataManager_SetIntValue(TW_USE_EXTERNAL_STORAGE, 1);
						}
						mount_current_storage();
					}
				}

				set_restore_files();
				tok = strtok(NULL, " ");
				if (tok != NULL) {
					int tw_restore_system = 0;
					int tw_restore_data = 0;
					int tw_restore_cache = 0;
					int tw_restore_recovery = 0;
					int tw_restore_boot = 0;
					int tw_restore_andsec = 0;
					int tw_restore_sdext = 0;
					int tw_restore_sp1 = 0;
					int tw_restore_sp2 = 0;
					int tw_restore_sp3 = 0;

					memset(value2, 0, sizeof(value2));
					strcpy(value2, tok);
					ui_print("Setting restore options:\n");
					line_len = strlen(value2);
					for (i=0; i<line_len; i++) {
						if ((value2[i] == 'S' || value2[i] == 's') && DataManager_GetIntValue(TW_RESTORE_SYSTEM_VAR) > 0) {
							tw_restore_system = 1;
							ui_print("System\n");
						} else if ((value2[i] == 'D' || value2[i] == 'd') && DataManager_GetIntValue(TW_RESTORE_DATA_VAR) > 0) {
							tw_restore_data = 1;
							ui_print("Data\n");
						} else if ((value2[i] == 'C' || value2[i] == 'c') && DataManager_GetIntValue(TW_RESTORE_CACHE_VAR) > 0) {
							tw_restore_cache = 1;
							ui_print("Cache\n");
						} else if ((value2[i] == 'R' || value2[i] == 'r') && DataManager_GetIntValue(TW_RESTORE_RECOVERY_VAR) > 0) {
							tw_restore_recovery = 1;
							ui_print("Recovery\n");
						} else if (value2[i] == '1' && DataManager_GetIntValue(TW_RESTORE_SP1_VAR) > 0) {
							tw_restore_sp1 = 1;
							ui_print("%s\n", "Special1");
						} else if (value2[i] == '2' && DataManager_GetIntValue(TW_RESTORE_SP2_VAR) > 0) {
							tw_restore_sp2 = 1;
							ui_print("%s\n", "Special2");
						} else if (value2[i] == '3' && DataManager_GetIntValue(TW_RESTORE_SP3_VAR) > 0) {
							tw_restore_sp3 = 1;
							ui_print("%s\n", "Special3");
						} else if ((value2[i] == 'B' || value2[i] == 'b') && DataManager_GetIntValue(TW_RESTORE_BOOT_VAR) > 0) {
							tw_restore_boot = 1;
							ui_print("Boot\n");
						} else if ((value2[i] == 'A' || value2[i] == 'a') && DataManager_GetIntValue(TW_RESTORE_ANDSEC_VAR) > 0) {
							tw_restore_andsec = 1;
							ui_print("Android Secure\n");
						} else if ((value2[i] == 'E' || value2[i] == 'e') && DataManager_GetIntValue(TW_RESTORE_SDEXT_VAR) > 0) {
							tw_restore_sdext = 1;
							ui_print("SD-Ext\n");
						} else if (value2[i] == 'M' || value2[i] == 'm') {
							DataManager_SetIntValue(TW_SKIP_MD5_CHECK_VAR, 1);
							ui_print("MD5 check skip is on\n");
						}
					}

					if (DataManager_GetIntValue(TW_RESTORE_SYSTEM_VAR) && !tw_restore_system)
						DataManager_SetIntValue(TW_RESTORE_SYSTEM_VAR, 0);
					if (DataManager_GetIntValue(TW_RESTORE_DATA_VAR) && !tw_restore_data)
						DataManager_SetIntValue(TW_RESTORE_DATA_VAR, 0);
					if (DataManager_GetIntValue(TW_RESTORE_CACHE_VAR) && !tw_restore_cache)
						DataManager_SetIntValue(TW_RESTORE_CACHE_VAR, 0);
					if (DataManager_GetIntValue(TW_RESTORE_RECOVERY_VAR) && !tw_restore_recovery)
						DataManager_SetIntValue(TW_RESTORE_RECOVERY_VAR, 0);
					if (DataManager_GetIntValue(TW_RESTORE_BOOT_VAR) && !tw_restore_boot)
						DataManager_SetIntValue(TW_RESTORE_BOOT_VAR, 0);
					if (DataManager_GetIntValue(TW_RESTORE_ANDSEC_VAR) && !tw_restore_andsec)
						DataManager_SetIntValue(TW_RESTORE_ANDSEC_VAR, 0);
					if (DataManager_GetIntValue(TW_RESTORE_SDEXT_VAR) && !tw_restore_sdext)
						DataManager_SetIntValue(TW_RESTORE_SDEXT_VAR, 0);
					if (DataManager_GetIntValue(TW_RESTORE_SP1_VAR) && !tw_restore_sp1)
						DataManager_SetIntValue(TW_RESTORE_SP1_VAR, 0);
					if (DataManager_GetIntValue(TW_RESTORE_SP2_VAR) && !tw_restore_sp2)
						DataManager_SetIntValue(TW_RESTORE_SP2_VAR, 0);
					if (DataManager_GetIntValue(TW_RESTORE_SP3_VAR) && !tw_restore_sp3)
						DataManager_SetIntValue(TW_RESTORE_SP3_VAR, 0);
				} else
					LOGI("No restore options set\n");
				nandroid_rest_exe();
				ui_print("Restore complete!\n");
			} else if (strcmp(command, "mount") == 0) {
				// Mount
				if (value[0] != '/') {
					strcpy(mount, "/");
					strcat(mount, value);
				} else
					strcpy(mount, value);
				ensure_path_mounted(mount);
				ui_print("Mounted '%s'\n", mount);
			} else if (strcmp(command, "unmount") == 0 || strcmp(command, "umount") == 0) {
				// Unmount
				if (value[0] != '/') {
					strcpy(mount, "/");
					strcat(mount, value);
				} else
					strcpy(mount, value);
				ensure_path_unmounted(mount);
				ui_print("Unmounted '%s'\n", mount);
			} else if (strcmp(command, "set") == 0) {
				// Set value
				tok = strtok(value, " ");
				strcpy(value1, tok);
				tok = strtok(NULL, " ");
				strcpy(value2, tok);
				ui_print("Setting '%s' to '%s'\n", value1, value2);
				DataManager_SetStrValue(value1, value2);
			} else if (strcmp(command, "mkdir") == 0) {
				// Make directory (recursive)
				ui_print("Making directory (recursive): '%s'\n", value);
				if (recursive_mkdir(value)) {
					LOGE("Unable to create folder: '%s'\n", value);
					ret_val = 1;
				}
			} else if (strcmp(command, "reboot") == 0) {
				// Reboot
			} else if (strcmp(command, "cmd") == 0) {
				if (cindex != 0) {
					__system(value);
				} else {
					LOGE("No value given for cmd\n");
				}
			} else {
				LOGE("Unrecognized script command: '%s'\n", command);
				ret_val = 1;
			}
		}
		fclose(fp);
		ui_print("Done processing script file\n");
	} else {
		LOGE("Error opening script file '%s'\n", SCRIPT_FILE_TMP);
		return 1;
	}
	return ret_val;
}

// end open recovery script file code

int
main(int argc, char **argv) {
    time_t start = time(NULL);

    // If these fail, there's not really anywhere to complain...
    freopen(TEMPORARY_LOG_FILE, "a", stdout); setbuf(stdout, NULL);
    freopen(TEMPORARY_LOG_FILE, "a", stderr); setbuf(stderr, NULL);
    printf("Starting recovery on %s", ctime(&start));

    printf("Loading volume table...\n");
    load_volume_table();

	// Load default values to set DataManager constants and handle ifdefs
	DataManager_LoadDefaults();

	// Fire up the UI engine
    if (gui_init())
    {
        ui_init();
        ui_set_background(BACKGROUND_ICON_INSTALLING);
    }

	// Load up all the resources
	gui_loadResources();

    printf("Processing arguments (%d)...\n", argc);
    get_args(&argc, &argv);

    LOGI("=> Installing busybox into /sbin\n");
	__system("/sbin/bbinstall.sh"); // Let's install busybox
	LOGI("=> Linking mtab\n");
	__system("ln -s /proc/mounts /etc/mtab"); // And link mtab for mke2fs
	LOGI("=> Getting locations\n");
    if (getLocations())
    {
        LOGE("Failing out of recovery.\n");
        return -1;
    }

    int previous_runs = 0;
    const char *send_intent = NULL;
    const char *update_package = NULL;
    const char *encrypted_fs_mode = NULL;
    int wipe_data = 0, wipe_cache = 0;
    int toggle_secure_fs = 0;
    encrypted_fs_info encrypted_fs_data;

    int arg;
    while ((arg = getopt_long(argc, argv, "", OPTIONS, NULL)) != -1) {
        switch (arg) {
        case 'p': previous_runs = atoi(optarg); break;
        case 's': send_intent = optarg; break;
        case 'u': update_package = optarg; break;
        case 'w': wipe_data = wipe_cache = 1; break;
        case 'c': wipe_cache = 1; break;
        case 'e': encrypted_fs_mode = optarg; toggle_secure_fs = 1; break;
        case 't': ui_show_text(1); break;
        case '?':
            LOGE("Invalid command argument\n");
            continue;
        }
    }

    device_recovery_start();

    printf("Command:");
    for (arg = 0; arg < argc; arg++) {
        printf(" \"%s\"", argv[arg]);
    }
    printf("\n");

    if (update_package) {
        // For backwards compatibility on the cache partition only, if
        // we're given an old 'root' path "CACHE:foo", change it to
        // "/cache/foo".
        if (strncmp(update_package, "CACHE:", 6) == 0) {
            int len = strlen(update_package) + 10;
            char* modified_path = malloc(len);
            strlcpy(modified_path, "/cache/", len);
            strlcat(modified_path, update_package+6, len);
            printf("(replacing path \"%s\" with \"%s\")\n",
                   update_package, modified_path);
            update_package = modified_path;
        }
    }
    printf("\n");

    property_list(print_property, NULL);
    printf("\n");

	// Check for and run startup script if script exists
	check_and_run_script("/sbin/runatboot.sh", "boot");
	check_and_run_script("/sbin/postrecoveryboot.sh", "boot");

#ifdef TW_INCLUDE_INJECTTWRP
	// Back up TWRP Ramdisk if needed:
	LOGI("Backing up TWRP ramdisk...\n");
	__system("injecttwrp --backup /tmp/backup_recovery_ramdisk.img");
	LOGI("Backup of TWRP ramdisk done.\n");
#endif

	int status = INSTALL_SUCCESS;

    if (toggle_secure_fs) {
        gui_console_only();
        if (strcmp(encrypted_fs_mode,"on") == 0) {
            encrypted_fs_data.mode = MODE_ENCRYPTED_FS_ENABLED;
            ui_print("Enabling Encrypted FS.\n");
        } else if (strcmp(encrypted_fs_mode,"off") == 0) {
            encrypted_fs_data.mode = MODE_ENCRYPTED_FS_DISABLED;
            ui_print("Disabling Encrypted FS.\n");
        } else {
            ui_print("Error: invalid Encrypted FS setting.\n");
            status = INSTALL_ERROR;
        }

        // Recovery strategy: if the data partition is damaged, disable encrypted file systems.
        // This prevents the device recycling endlessly in recovery mode.
        if ((encrypted_fs_data.mode == MODE_ENCRYPTED_FS_ENABLED) &&
                (read_encrypted_fs_info(&encrypted_fs_data))) {
            ui_print("Encrypted FS change aborted, resetting to disabled state.\n");
            encrypted_fs_data.mode = MODE_ENCRYPTED_FS_DISABLED;
        }

        if (status != INSTALL_ERROR) {
            if (erase_volume("/data")) {
                ui_print("Data wipe failed.\n");
                status = INSTALL_ERROR;
            } else if (erase_volume("/cache")) {
                ui_print("Cache wipe failed.\n");
                status = INSTALL_ERROR;
            } else if ((encrypted_fs_data.mode == MODE_ENCRYPTED_FS_ENABLED) &&
                      (restore_encrypted_fs_info(&encrypted_fs_data))) {
                ui_print("Encrypted FS change aborted.\n");
                status = INSTALL_ERROR;
            } else {
                ui_print("Successfully updated Encrypted FS.\n");
                status = INSTALL_SUCCESS;
            }
        }
    } else if (update_package != NULL) {
        gui_console_only();
        status = install_package(update_package);
        if (status != INSTALL_SUCCESS) ui_print("Installation aborted.\n");
    } else if (wipe_data) {
        gui_console_only();
        if (device_wipe_data()) status = INSTALL_ERROR;
        if (erase_volume("/data")) status = INSTALL_ERROR;
        if (wipe_cache && erase_volume("/cache")) status = INSTALL_ERROR;
        if (status != INSTALL_SUCCESS) ui_print("Data wipe failed.\n");
    } else if (wipe_cache) {
        gui_console_only();
        if (wipe_cache && erase_volume("/cache")) status = INSTALL_ERROR;
        if (status != INSTALL_SUCCESS) ui_print("Cache wipe failed.\n");
    } else {
        status = INSTALL_ERROR;  // No command specified
    }

    if (status != INSTALL_SUCCESS) ui_set_background(BACKGROUND_ICON_ERROR);

    if (status != INSTALL_SUCCESS && ui_text_visible()) { // We only want to show menu if error && visible
        //assume we want to be here and its not an error - give us the pretty icon!
        ui_set_background(BACKGROUND_ICON_MAIN);

        // Load up the values for TWRP - Sleep to let the card be ready
		char mkdir_path[255], settings_file[255];
		memset(mkdir_path, 0, sizeof(mkdir_path));
		memset(settings_file, 0, sizeof(settings_file));
		sprintf(mkdir_path, "%s/TWRP", DataManager_GetSettingsStoragePath());
		sprintf(settings_file, "%s/.twrps", mkdir_path);

        if (ensure_path_mounted(DataManager_GetSettingsStorageMount()) < 0)
        {
            usleep(500000);
            if (ensure_path_mounted(DataManager_GetSettingsStorageMount()) < 0)
                LOGE("Unable to mount %s\n", DataManager_GetSettingsStorageMount());
        }
		
		mkdir(mkdir_path, 0777);

        LOGI("Attempt to load settings from settings file...\n");
		DataManager_LoadValues(settings_file);
		if (DataManager_GetIntValue(TW_HAS_DUAL_STORAGE) != 0 && DataManager_GetIntValue(TW_USE_EXTERNAL_STORAGE) == 1) {
			// Attempt to sdcard using external storage
			if (mount_current_storage()) {
				LOGE("Failed to mount external storage, using internal storage.\n");
				// Remount failed, default back to internal storage
				DataManager_SetIntValue(TW_USE_EXTERNAL_STORAGE, 0);
				mount_current_storage();
			}
		} else {
			mount_current_storage();
			if (DataManager_GetIntValue(TW_HAS_DUAL_STORAGE) == 0 && DataManager_GetIntValue(TW_HAS_DATA_MEDIA) == 1) {
				__system("mount /data/media /sdcard");
			}
		}

        // Update storage free space after settings file is loaded
		if (DataManager_GetIntValue(TW_USE_EXTERNAL_STORAGE) == 1)
			DataManager_SetIntValue(TW_STORAGE_FREE_SIZE, (int)((sdcext.sze - sdcext.used) / 1048576LLU));
		else
			DataManager_SetIntValue(TW_STORAGE_FREE_SIZE, (int)((sdcint.sze - sdcint.used) / 1048576LLU));

		// Update some of the main data
        update_tz_environment_variables();
        set_theme(DataManager_GetStrValue(TW_COLOR_THEME_VAR));

        // This clears up a bug about reboot coming back to recovery with the GUI
        finish_recovery(NULL);
		if (check_for_script_file()) {
			gui_console_only();
			if (run_script_file() != 0) {
				// There was an error, boot the recovery
				if (gui_start())
					prompt_and_wait();
			} else {
				usleep(2000000); // Sleep for 2 seconds before rebooting
			}
		} else if (gui_start())
			prompt_and_wait();
    }

    // Otherwise, get ready to boot the main system...
    finish_recovery(send_intent);
    ui_print("Rebooting...\n");
    sync();
    reboot(RB_AUTOBOOT);
    return EXIT_SUCCESS;
}
