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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/vfs.h>
#include <sys/mount.h>

#include "tw_reboot.h"
#include "backstore.h"
#include "ddftw.h"
#include "extra-functions.h"
#include "roots.h"
#include "format.h"
#include "data.h"

int getWordFromString(int word, const char* string, char* buffer, int bufferLen)
{
    char* start = NULL;

    do
    {
        // Ignore leading whitespace
        while (*string > 0 && *string < 33)     ++string;

        start = (char*) string;
        while (*string > 32)                    ++string;
    } while (--word > 0);

    // Handle word not found
    if (*start == '\0')
    {
        buffer[0] = '\0';
        return 0;
    }

    memset(buffer, 0, bufferLen);
    if ((string - start) > bufferLen)
        memcpy(buffer, start, bufferLen-1);
    else
        memcpy(buffer, start, string - start);

    return (string - start);
}

void SetDataState(char* operation, char* partition, int errorCode, int done)
{
    DataManager_SetStrValue("tw_operation", operation);
    DataManager_SetStrValue("tw_partition", partition);
    DataManager_SetIntValue("tw_operation_status", errorCode);
    DataManager_SetIntValue("tw_operation_state", done);
}

void
nandroid_menu()
{
	// define constants for menu selection
#define ITEM_BACKUP_MENU         0
#define ITEM_RESTORE_MENU        1
#define ITEM_MENU_RBOOT 	     2
#define ITEM_MENU_BACK           3
	
	// build headers and items in menu
	char* nan_headers[] = { "Nandroid Menu",
							"Choose Backup or Restore:",
							NULL };
	
	char* nan_items[] = { "Backup Partitions",
						  "Restore Partitions",
						  "--> Reboot System",
						  "<-- Back To Main Menu",
						  NULL };
	
	char** headers = prepend_title((const char**) nan_headers);
	
	int chosen_item;
    inc_menu_loc(ITEM_MENU_BACK); // record back selection into array
	for (;;) {
         ui_set_background(BACKGROUND_ICON_NANDROID);
		chosen_item = get_menu_selection(headers, nan_items, 0, 0);
		switch (chosen_item) {
			case ITEM_BACKUP_MENU:
				nan_backup_menu(0);
				break;
			case ITEM_RESTORE_MENU:
                choose_backup_folder();
				break;
			case ITEM_MENU_RBOOT:
				tw_reboot(rb_system);
                break;
			case ITEM_MENU_BACK:
				dec_menu_loc();
                ui_set_background(BACKGROUND_ICON_MAIN);
				return;
		}
	    if (go_home) { 		// if home was called
	        dec_menu_loc();
                ui_set_background(BACKGROUND_ICON_MAIN);
	        return;
	    }
	}
}

#define ITEM_NAN_BACKUP		0
#define ITEM_NAN_SYSTEM		1
#define ITEM_NAN_DATA      	2
#define ITEM_NAN_BOOT       3
#define ITEM_NAN_RECOVERY   4
#define ITEM_NAN_CACHE		5
#define ITEM_NAN_SP1        6
#define ITEM_NAN_SP2        7
#define ITEM_NAN_SP3        8
#define ITEM_NAN_ANDSEC     9
#define ITEM_NAN_SDEXT      10
#define ITEM_NAN_COMPRESS   11
#define ITEM_NAN_BACK       12

char* nan_compress()
{
	char* tmp_set = (char*)malloc(40);
	strcpy(tmp_set, "[ ] Compress Backup (slow but saves space)");
	if (DataManager_GetIntValue(TW_USE_COMPRESSION_VAR) == 1) {
		tmp_set[1] = 'x';
	}
	return tmp_set;
}

void
nan_backup_menu(int pIdx)
{
	char* nan_b_headers[] = { "Nandroid Backup",
							   "Choose Backup Options:",
							   NULL	};
	
	char* nan_b_items[] = { "--> Backup Naowz!",
							 nan_img_set(ITEM_NAN_SYSTEM,0),
							 nan_img_set(ITEM_NAN_DATA,0),
							 nan_img_set(ITEM_NAN_BOOT,0),
							 nan_img_set(ITEM_NAN_RECOVERY,0),
							 nan_img_set(ITEM_NAN_CACHE,0),
							 nan_img_set(ITEM_NAN_SP1,0),
                             nan_img_set(ITEM_NAN_SP2,0),
                             nan_img_set(ITEM_NAN_SP3,0),
							 nan_img_set(ITEM_NAN_ANDSEC,0),
							 nan_img_set(ITEM_NAN_SDEXT,0),
							 nan_compress(),
							 "<-- Back To Nandroid Menu",
							 NULL };

	char** headers = prepend_title((const char**) nan_b_headers);
	
    inc_menu_loc(ITEM_NAN_BACK);
	for (;;) {
		int chosen_item = get_menu_selection(headers, nan_b_items, 0, pIdx); // get key presses
		pIdx = chosen_item; // remember last selection location

        int tw_total = 0;
        tw_total += (DataManager_GetIntValue(TW_BACKUP_SYSTEM_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_DATA_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_CACHE_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_RECOVERY_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_SP1_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_SP2_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_SP3_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_BOOT_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_ANDSEC_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_SDEXT_VAR) == 1 ? 1 : 0);

		switch (chosen_item) {
			case ITEM_NAN_BACKUP:
				if (tw_total > 0) {
					nandroid_back_exe();
					dec_menu_loc();
					return;
				}
				break;
			case ITEM_NAN_SYSTEM:
            	if (DataManager_GetIntValue(TW_BACKUP_SYSTEM_VAR) >= 0)
                    DataManager_ToggleIntValue(TW_BACKUP_SYSTEM_VAR);
                break;
			case ITEM_NAN_DATA:
                if (DataManager_GetIntValue(TW_BACKUP_DATA_VAR) >= 0)
                    DataManager_ToggleIntValue(TW_BACKUP_DATA_VAR);
				break;
			case ITEM_NAN_BOOT:
                if (DataManager_GetIntValue(TW_BACKUP_BOOT_VAR) >= 0)
                    DataManager_ToggleIntValue(TW_BACKUP_BOOT_VAR);
				break;
			case ITEM_NAN_RECOVERY:
                if (DataManager_GetIntValue(TW_BACKUP_RECOVERY_VAR) >= 0)
                    DataManager_ToggleIntValue(TW_BACKUP_RECOVERY_VAR);
				break;
			case ITEM_NAN_CACHE:
                if (DataManager_GetIntValue(TW_BACKUP_CACHE_VAR) >= 0)
                    DataManager_ToggleIntValue(TW_BACKUP_CACHE_VAR);
				break;
			case ITEM_NAN_SP1:
                if (DataManager_GetIntValue(TW_BACKUP_SP1_VAR) >= 0)
                    DataManager_ToggleIntValue(TW_BACKUP_SP1_VAR);
				break;
            case ITEM_NAN_SP2:
                if (DataManager_GetIntValue(TW_BACKUP_SP2_VAR) >= 0)
                    DataManager_ToggleIntValue(TW_BACKUP_SP2_VAR);
                break;
            case ITEM_NAN_SP3:
                if (DataManager_GetIntValue(TW_BACKUP_SP3_VAR) >= 0)
                    DataManager_ToggleIntValue(TW_BACKUP_SP3_VAR);
                break;
			case ITEM_NAN_ANDSEC:
                if (DataManager_GetIntValue(TW_BACKUP_ANDSEC_VAR) >= 0)
                    DataManager_ToggleIntValue(TW_BACKUP_ANDSEC_VAR);
				break;
			case ITEM_NAN_SDEXT:
                if (DataManager_GetIntValue(TW_BACKUP_SDEXT_VAR) >= 0)
                    DataManager_ToggleIntValue(TW_BACKUP_SDEXT_VAR);
				break;
			case ITEM_NAN_COMPRESS:
            	DataManager_ToggleIntValue(TW_USE_COMPRESSION_VAR);
				break;
			case ITEM_NAN_BACK:
            	dec_menu_loc();
				return;
		}
	    if (go_home) { 
	        dec_menu_loc();
	        return;
	    }
		break;
	}
	ui_end_menu(); // end menu
    dec_menu_loc(); // decrease menu location
	nan_backup_menu(pIdx); // restart menu (to refresh it)
}

void
set_restore_files()
{
    const char* nan_dir = DataManager_GetStrValue("tw_restore");

    // Start with the default values
    int tw_restore_system = -1;
    int tw_restore_data = -1;
    int tw_restore_cache = -1;
    int tw_restore_recovery = -1;
    int tw_restore_boot = -1;
    int tw_restore_andsec = -1;
    int tw_restore_sdext = -1;
    int tw_restore_sp1 = -1;
    int tw_restore_sp2 = -1;
    int tw_restore_sp3 = -1;

    DIR* d;
    d = opendir(nan_dir);
    if (d == NULL)
    {
        LOGE("error opening %s\n", nan_dir);
        return;
    }

    struct dirent* de;
    while ((de = readdir(d)) != NULL)
    {
        // Strip off three components
        char str[256];
        char* label;
        char* fstype = NULL;
        char* extn = NULL;
        char* ptr;
        struct dInfo* dev = NULL;

        strcpy(str, de->d_name);
        label = str;
        ptr = label;
        while (*ptr && *ptr != '.')     ptr++;
        if (*ptr == '.')
        {
            *ptr = 0x00;
            ptr++;
            fstype = ptr;
        }
        while (*ptr && *ptr != '.')     ptr++;
        if (*ptr == '.')
        {
            *ptr = 0x00;
            ptr++;
            extn = ptr;
        }

        if (extn == NULL || strcmp(extn, "win") != 0)   continue;

        dev = findDeviceByLabel(label);
        if (dev == NULL)
        {
            LOGE(" Unable to locate device by label\n");
            continue;
        }

        strncpy(dev->fnm, de->d_name, 256);
        dev->fnm[255] = '\0';

        // Now, we just need to find the correct label
        if (dev == &sys)        tw_restore_system = 1;
        if (dev == &dat)        tw_restore_data = 1;
        if (dev == &boo)        tw_restore_boot = 1;
        if (dev == &rec)        tw_restore_recovery = 1;
        if (dev == &cac)        tw_restore_cache = 1;
        if (dev == &sde)        tw_restore_sdext = 1;
        if (dev == &sp1)        tw_restore_sp1 = 1;
        if (dev == &sp2)        tw_restore_sp2 = 1;
        if (dev == &sp3)        tw_restore_sp3 = 1;
        if (dev == &ase)        tw_restore_andsec = 1;
    }
    closedir(d);

    // Set the final values
    DataManager_SetIntValue(TW_RESTORE_SYSTEM_VAR, tw_restore_system);
    DataManager_SetIntValue(TW_RESTORE_DATA_VAR, tw_restore_data);
    DataManager_SetIntValue(TW_RESTORE_CACHE_VAR, tw_restore_cache);
    DataManager_SetIntValue(TW_RESTORE_RECOVERY_VAR, tw_restore_recovery);
    DataManager_SetIntValue(TW_RESTORE_BOOT_VAR, tw_restore_boot);
    DataManager_SetIntValue(TW_RESTORE_ANDSEC_VAR, tw_restore_andsec);
    DataManager_SetIntValue(TW_RESTORE_SDEXT_VAR, tw_restore_sdext);
    DataManager_SetIntValue(TW_RESTORE_SP1_VAR, tw_restore_sp1);
    DataManager_SetIntValue(TW_RESTORE_SP2_VAR, tw_restore_sp2);
    DataManager_SetIntValue(TW_RESTORE_SP3_VAR, tw_restore_sp3);

    return;
}

void
nan_restore_menu(int pIdx)
{
	char* nan_r_headers[] = { "Nandroid Restore",
							  "Choose Restore Options:",
							  NULL	};
	
	char* nan_r_items[] = { "--> Restore Naowz!",
			 	 	 	 	nan_img_set(ITEM_NAN_SYSTEM,1),
			 	 	 	 	nan_img_set(ITEM_NAN_DATA,1),
			 	 	 	 	nan_img_set(ITEM_NAN_BOOT,1),
			 	 	 	 	nan_img_set(ITEM_NAN_RECOVERY,1),
			 	 	 	 	nan_img_set(ITEM_NAN_CACHE,1),
			 	 	 	 	nan_img_set(ITEM_NAN_SP1,1),
                            nan_img_set(ITEM_NAN_SP2,1),
                            nan_img_set(ITEM_NAN_SP3,1),
			 	 	 	 	nan_img_set(ITEM_NAN_ANDSEC,1),
			 	 	 	 	nan_img_set(ITEM_NAN_SDEXT,1),
							"<-- Back To Nandroid Menu",
							 NULL };

	char** headers = prepend_title((const char**) nan_r_headers);
	
    inc_menu_loc(ITEM_NAN_BACK);
	for (;;) {
		int chosen_item = get_menu_selection(headers, nan_r_items, 0, pIdx);
		pIdx = chosen_item; // remember last selection location

        int tw_total = 0;
        tw_total += (DataManager_GetIntValue(TW_BACKUP_SYSTEM_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_DATA_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_CACHE_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_RECOVERY_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_SP1_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_SP2_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_SP3_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_BOOT_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_ANDSEC_VAR) == 1 ? 1 : 0);
        tw_total += (DataManager_GetIntValue(TW_BACKUP_SDEXT_VAR) == 1 ? 1 : 0);

        switch (chosen_item)
		{
		case ITEM_NAN_BACKUP:
			if (tw_total > 0) {
				nandroid_rest_exe();
				dec_menu_loc();
				return;
			}
			break;
		case ITEM_NAN_SYSTEM:
            if (DataManager_GetIntValue(TW_RESTORE_SYSTEM_VAR) >= 0)
                DataManager_ToggleIntValue(TW_RESTORE_SYSTEM_VAR);
            break;
		case ITEM_NAN_DATA:
            if (DataManager_GetIntValue(TW_RESTORE_DATA_VAR) >= 0)
                DataManager_ToggleIntValue(TW_RESTORE_DATA_VAR);
			break;
		case ITEM_NAN_BOOT:
            if (DataManager_GetIntValue(TW_RESTORE_BOOT_VAR) >= 0)
                DataManager_ToggleIntValue(TW_RESTORE_BOOT_VAR);
			break;
		case ITEM_NAN_RECOVERY:
            if (DataManager_GetIntValue(TW_RESTORE_RECOVERY_VAR) >= 0)
                DataManager_ToggleIntValue(TW_RESTORE_RECOVERY_VAR);
			break;
		case ITEM_NAN_CACHE:
            if (DataManager_GetIntValue(TW_RESTORE_CACHE_VAR) >= 0)
                DataManager_ToggleIntValue(TW_RESTORE_CACHE_VAR);
			break;
		case ITEM_NAN_SP1:
            if (DataManager_GetIntValue(TW_RESTORE_SP1_VAR) >= 0)
                DataManager_ToggleIntValue(TW_RESTORE_SP1_VAR);
			break;
        case ITEM_NAN_SP2:
            if (DataManager_GetIntValue(TW_RESTORE_SP2_VAR) >= 0)
                DataManager_ToggleIntValue(TW_RESTORE_SP2_VAR);
            break;
        case ITEM_NAN_SP3:
            if (DataManager_GetIntValue(TW_RESTORE_SP3_VAR) >= 0)
                DataManager_ToggleIntValue(TW_RESTORE_SP3_VAR);
            break;
		case ITEM_NAN_ANDSEC:
            if (DataManager_GetIntValue(TW_RESTORE_ANDSEC_VAR) >= 0)
                DataManager_ToggleIntValue(TW_RESTORE_ANDSEC_VAR);
			break;
		case ITEM_NAN_SDEXT:
            if (DataManager_GetIntValue(TW_RESTORE_SDEXT_VAR) >= 0)
                DataManager_ToggleIntValue(TW_RESTORE_SDEXT_VAR);
			break;
		case ITEM_NAN_BACK - 1:
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
    nan_restore_menu(pIdx);
}

char* 
nan_img_set(int tw_setting, int tw_backstore)
{
	mount_current_storage();
	int isTrue = 0;
	char* tmp_set = (char*)malloc(25);
	struct stat st;
	switch (tw_setting) {
		case ITEM_NAN_SYSTEM:
			strcpy(tmp_set, "[ ] system");
			if (tw_backstore) {
				isTrue = DataManager_GetIntValue(TW_RESTORE_SYSTEM_VAR);
			} else {
				isTrue = DataManager_GetIntValue(TW_BACKUP_SYSTEM_VAR);
			}
			break;
		case ITEM_NAN_DATA:
			strcpy(tmp_set, "[ ] data");
			if (tw_backstore) {
                isTrue = DataManager_GetIntValue(TW_RESTORE_DATA_VAR);
			} else {
				isTrue = DataManager_GetIntValue(TW_BACKUP_DATA_VAR);
			}
			break;
		case ITEM_NAN_BOOT:
			strcpy(tmp_set, "[ ] boot");
			if (tw_backstore) {
                isTrue = DataManager_GetIntValue(TW_RESTORE_BOOT_VAR);
			} else {
				isTrue = DataManager_GetIntValue(TW_BACKUP_BOOT_VAR);
			}
			break;
		case ITEM_NAN_RECOVERY:
			strcpy(tmp_set, "[ ] recovery");
			if (tw_backstore) {
                isTrue = DataManager_GetIntValue(TW_RESTORE_RECOVERY_VAR);
			} else {
				isTrue = DataManager_GetIntValue(TW_BACKUP_RECOVERY_VAR);
			}
			break;
		case ITEM_NAN_CACHE:
			strcpy(tmp_set, "[ ] cache");
			if (tw_backstore) {
                isTrue = DataManager_GetIntValue(TW_RESTORE_CACHE_VAR);
			} else {
				isTrue = DataManager_GetIntValue(TW_BACKUP_CACHE_VAR);
			}
			break;
		case ITEM_NAN_SP1:
			strcpy(tmp_set, "[ ] ");
            strcat(tmp_set, sp1.mnt);
			if (tw_backstore) {
                isTrue = DataManager_GetIntValue(TW_RESTORE_SP1_VAR);
			} else {
				if (strlen(sp1.mnt) > 0) {
					isTrue = DataManager_GetIntValue(TW_BACKUP_SP1_VAR);
				} else {
                    DataManager_SetIntValue(TW_RESTORE_SP1_VAR, -1);
					isTrue = -1;
				}
			}
			break;
        case ITEM_NAN_SP2:
            strcpy(tmp_set, "[ ] ");
            strcat(tmp_set, sp2.mnt);
            if (tw_backstore) {
                isTrue = DataManager_GetIntValue(TW_RESTORE_SP2_VAR);
            } else {
                if (strlen(sp2.mnt) > 0) {
                    isTrue = DataManager_GetIntValue(TW_BACKUP_SP2_VAR);
                } else {
                    DataManager_SetIntValue(TW_RESTORE_SP2_VAR, -1);
                    isTrue = -1;
                }
            }
            break;
        case ITEM_NAN_SP3:
            strcpy(tmp_set, "[ ] ");
            strcat(tmp_set, sp3.mnt);
            if (tw_backstore) {
                isTrue = DataManager_GetIntValue(TW_RESTORE_SP3_VAR);
            } else {
                if (strlen(sp3.mnt) > 0) {
                    isTrue = DataManager_GetIntValue(TW_BACKUP_SP3_VAR);
                } else {
                    DataManager_SetIntValue(TW_RESTORE_SP3_VAR, -1);
                    isTrue = -1;
                }
            }
            break;
		case ITEM_NAN_ANDSEC:
			strcpy(tmp_set, "[ ] .android_secure");
			if (tw_backstore) {
				isTrue = DataManager_GetIntValue(TW_RESTORE_ANDSEC_VAR);
			} else {
				if (stat(ase.dev,&st) != 0) {
                    DataManager_SetIntValue(TW_RESTORE_ANDSEC_VAR, -1);
					isTrue = -1;
				} else {
					isTrue = DataManager_GetIntValue(TW_BACKUP_ANDSEC_VAR);
				}
			}
			break;
		case ITEM_NAN_SDEXT:
			strcpy(tmp_set, "[ ] sd-ext");
			if (tw_backstore) {
				isTrue = DataManager_GetIntValue(TW_RESTORE_SDEXT_VAR);
			} else {
				if (stat(sde.blk,&st) != 0) {
                    DataManager_SetIntValue(TW_RESTORE_SDEXT_VAR, -1);
					isTrue = -1;
				} else {
					isTrue = DataManager_GetIntValue(TW_BACKUP_SDEXT_VAR);
				}
			}
			break;
	}
	if (isTrue == 1) {
		tmp_set[1] = 'x';
	} 
	if (isTrue == -1) {
		tmp_set[0] = ' ';
		tmp_set[1] = ' ';
		tmp_set[2] = ' ';
	}
	return tmp_set;
}

int tw_isMounted(struct dInfo mMnt)
{
    // Do not try to mount non-mountable partitions
    if (!mMnt.mountable)        return -1;

    struct stat st1, st2;
    char path[255];

    sprintf(path, "/%s/.", mMnt.mnt);
    if (stat(path, &st1) != 0)  return -1;

    sprintf(path, "/%s/../.", mMnt.mnt);
    if (stat(path, &st2) != 0)  return -1;

    int ret = (st1.st_dev != st2.st_dev) ? 1 : 0;

    return ret;
}

/* Made my own mount, because aosp api "ensure_path_mounted" relies on recovery.fstab
** and we needed one that can mount based on what the file system really is (from blkid), 
** and not what's in fstab or recovery.fstab
*/
int tw_mount(struct dInfo mMnt)
{
    char target[255];
    int ret = 0;

    // Do not try to mount non-mountable partitions
    if (!mMnt.mountable)        return 0;
    if (tw_isMounted(mMnt))     return 0;

    sprintf(target, "/%s", mMnt.mnt);
    if (mount(mMnt.blk, target, mMnt.fst, 0, NULL) != 0)
    {
        if (strcmp(target, "/sd-ext") != 0)
			LOGE("Unable to mount %s\n", target);
        ret = 1;
    }
    return ret;
}

int tw_unmount(struct dInfo uMnt)
{
    char target[255];
    int ret = 0;

    // Do not try to mount non-mountable partitions
    if (!uMnt.mountable)        return 0;
    if (!tw_isMounted(uMnt))    return 0;

	if (DataManager_GetIntValue(TW_DONT_UNMOUNT_SYSTEM) == 1 && strcmp(uMnt.mnt, "system") == 0)
		return 0; // never unmount system on this device

	if (DataManager_GetIntValue(TW_HAS_DATA_MEDIA) == 1 && DataManager_GetIntValue(TW_USE_EXTERNAL_STORAGE) == 0 && strcmp(uMnt.mnt, "data") == 0)
		return 0; // don't unmount data if we have data/media and using internal storage

    sprintf(target, "/%s", uMnt.mnt);
    if (umount(target) != 0)
    {
        LOGE("Unable to unmount %s\n", target);
        ret = 1;
    }
	return ret;
}

unsigned long long get_backup_size(struct dInfo* mnt)
{
    if (!mnt)
    {
        LOGE("Invalid call to get_backup_size with NULL parameter\n");
        return 0;
    } else
		return (unsigned long long) mnt->bsze;

    /*switch (mnt->backup)
    {
    case files:
		if (DataManager_GetIntValue(TW_HAS_DATA_MEDIA) == 1 && strcmp(mnt->blk, dat.blk) == 0) {
			LOGI("Device has /data/media\n");
			unsigned long long data_used, data_media_used, actual_data;
			data_used = mnt->used;
			LOGI("Total used space on /data is: %llu\nMounting /data\n", data_used);
			tw_mount(dat);
			data_media_used = getUsedSizeViaDu("/data/media");
			LOGI("Total in /data/media is: %llu\n", data_media_used);
			actual_data = data_used - data_media_used;
			LOGI("Actual data used: %llu\n", actual_data);
			return actual_data;
		} else
			return mnt->used;
    case image:
        return (unsigned long long) mnt->sze;
    case none:
        LOGW("Request backup details for partition '%s' which has no backup method.", mnt->mnt);
        break;

    default:
        LOGE("Backup type unknown for partition '%s'\n", mnt->mnt);
        break;
    }
    return 0;*/
}

/* New backup function
** Condensed all partitions into one function
*/
int tw_backup(struct dInfo bMnt, const char *bDir)
{
    // set compression or not
	char bTarArg[255];
	if (DataManager_GetIntValue(TW_USE_COMPRESSION_VAR)) {
		strcpy(bTarArg,"-czv");
	} else {
		strcpy(bTarArg,"-cv");
	}
    FILE *bFp;

#ifdef RECOVERY_SDCARD_ON_DATA
    //strcat(bTarArg, " --exclude='data/media*'");
	/*strcat(bTarArg, " -X /tmp/excludes.lst");

    bFp = fopen("/tmp/excludes.lst", "wt");
    if (bFp)
    {
        fprintf(bFp, "data/media\n");
        fprintf(bFp, "./media\n");
        fprintf(bFp, "media\n");
        fclose(bFp);
    }*/
#endif

    char str[512];
	unsigned long long bPartSize;
	char *bImage = malloc(sizeof(char)*255);
	char *bMount = malloc(sizeof(char)*255);
	char *bCommand = malloc(sizeof(char)*255);

    if (bMnt.backup == files)
    {
        // detect mountable partitions
		if (strcmp(bMnt.mnt,".android_secure") == 0) { // if it's android secure, add sdcard to prefix
			if (DataManager_GetIntValue(TW_HAS_INTERNAL))
				mount_internal_storage();
			else
				mount_current_storage();
			strcpy(bMount, bMnt.dev);
			sprintf(bImage,"and-sec.%s.win",bMnt.fst); // set image name based on filesystem, should always be vfat for android_secure
		} else {
			strcpy(bMount,"/");
			strcat(bMount,bMnt.mnt);
			sprintf(bImage,"%s.%s.win",bMnt.mnt,bMnt.fst); // anything else that is mountable, will be partition.filesystem.win
            SetDataState("Mounting", bMnt.mnt, 0, 0);
			if (tw_mount(bMnt))
            {
				ui_print("-- Could not mount: %s\n-- Aborting.\n",bMount);
				free(bCommand);
				free(bMount);
				free(bImage);
				return 1;
			}
		}
		bPartSize = bMnt.bsze;
		if (DataManager_GetIntValue(TW_HAS_DATA_MEDIA) == 1 && strcmp(bMount, "/data") == 0) {
			LOGI("Using special tar command for /data/media setups.\n");
			sprintf(bCommand, "cd /data && tar %s ./ --exclude='media*' -f %s%s", bTarArg, bDir, bImage);
		} else
			sprintf(bCommand,"cd %s && tar %s -f %s%s ./*",bMount,bTarArg,bDir,bImage); // form backup command
	} else if (bMnt.backup == image) {
		strcpy(bMount,bMnt.mnt);
		bPartSize = bMnt.bsze;
		sprintf(bImage,"%s.%s.win",bMnt.mnt,bMnt.fst); // non-mountable partitions such as boot/sp1/recovery
		if (strcmp(bMnt.fst,"mtd") == 0) {
			sprintf(bCommand,"dump_image %s %s%s",bMnt.mnt,bDir,bImage); // if it's mtd, we use dump image
		} else {
			sprintf(bCommand,"dd bs=%s if=%s of=%s%s",bs_size,bMnt.blk,bDir,bImage); // if it's emmc, use dd
		}
		ui_print("\n");
	}
    else
    {
        LOGE("Unknown backup method for mount %s\n", bMnt.mnt);
        return 1;
    }

	LOGI("=> Filename: %s\n",bImage);
	LOGI("=> Size of %s is %lu KB.\n\n", bMount, (unsigned long) (bPartSize / 1024));
	int i;
	char bUppr[255];
	strcpy(bUppr,bMnt.mnt);
	for (i = 0; i < (int) strlen(bUppr); i++) { // make uppercase of mount name
		bUppr[i] = toupper(bUppr[i]);
	}
	ui_print("[%s (%lu MB)]\n", bUppr, (unsigned long) (bPartSize / (1024 * 1024))); // show size in MB

    SetDataState("Backup", bMnt.mnt, 0, 0);
	int bProgTime;
	time_t bStart, bStop;
	char bOutput[1024];

    time(&bStart); // start timer
    ui_print("...Backing up %s partition.\n",bMount);
	LOGI("Backup command: '%s'\n", bCommand);
    bFp = __popen(bCommand, "r"); // sending backup command formed earlier above
    if(DataManager_GetIntValue(TW_SHOW_SPAM_VAR) == 2) { // if twrp spam is on, show all lines
        while (fgets(bOutput,sizeof(bOutput),bFp) != NULL) {
            ui_print_overwrite("%s",bOutput);
        }
    } else { // else just show single line
        while (fscanf(bFp,"%s",bOutput) == 1) {
            if(DataManager_GetIntValue(TW_SHOW_SPAM_VAR) == 1) ui_print_overwrite("%s",bOutput);
        }
    }
    ui_print_overwrite(" * Done.\n");
    __pclose(bFp);

    ui_print(" * Verifying backup size.\n");
    SetDataState("Verifying", bMnt.mnt, 0, 0);

    sprintf(bCommand, "%s%s", bDir, bImage);
    struct stat st;
    if (stat(bCommand, &st) != 0 || st.st_size == 0)
    {
        ui_print("E: File size is zero bytes. Aborting...\n\n"); // oh noes! file size is 0, abort! abort!
        tw_unmount(bMnt);
        free(bCommand);
        free(bMount);
        free(bImage);
        return 1;
    }

    ui_print(" * File size: %llu bytes.\n", st.st_size); // file size

    // Only verify image sizes
    if (bMnt.backup == image)
    {
        LOGI(" * Expected size: %llu Got: %lld\n", bMnt.sze, st.st_size);
        if (bMnt.sze != (unsigned long long) st.st_size)
        {
            if (DataManager_GetIntValue(TW_IGNORE_IMAGE_SIZE) == 1) {
				ui_print("E: File size is incorrect. Aborting.\n\n"); // they dont :(
				free(bCommand);
				free(bMount);
				free(bImage);
				return 1;
			} else
				LOGW("Image size doesn't match, ignoring error due to TW_IGNORE_IMAGE_SIZE setting.\n");
        }
    }

    ui_print(" * Generating md5...\n");
    SetDataState("Generating MD5", bMnt.mnt, 0, 0);
    makeMD5(bDir, bImage); // make md5 file
    time(&bStop); // stop timer
    ui_print("[%s DONE (%d SECONDS)]\n\n",bUppr,(int)difftime(bStop,bStart)); // done, finally. How long did it take?
	if (strcmp(bMnt.mnt, ".android_secure") != 0) // any partition other than android secure,
		tw_unmount(bMnt); // unmount partition we just restored to (if it's not a mountable partition, it will just bypass)

    free(bCommand);
	free(bMount);
	free(bImage);
	return 0;
}

int CalculateBackupDetails(int* total_partitions, unsigned long long* total_img_bytes, unsigned long long* total_file_bytes)
{
    *total_partitions = 0;

    if (DataManager_GetIntValue(TW_BACKUP_SYSTEM_VAR) == 1)
    {
        *total_partitions = *total_partitions + 1;
        if (sys.backup == image)    *total_img_bytes += get_backup_size(&sys);
        else                        *total_file_bytes += get_backup_size(&sys);
    }

	if (DataManager_GetIntValue(TW_BACKUP_DATA_VAR) == 1)
	{
		*total_partitions = *total_partitions + 1;
		if (dat.backup == image)    *total_img_bytes += get_backup_size(&dat);
		else                        *total_file_bytes += get_backup_size(&dat);
		if (DataManager_GetIntValue(TW_HAS_DATADATA) == 1) {
			*total_partitions = *total_partitions + 1;
			if (datdat.backup == image)    *total_img_bytes += get_backup_size(&datdat);
			else                           *total_file_bytes += get_backup_size(&datdat);
		}
	}
    if (DataManager_GetIntValue(TW_BACKUP_CACHE_VAR) == 1)
    {
        *total_partitions = *total_partitions + 1;
        if (cac.backup == image)    *total_img_bytes += get_backup_size(&cac);
        else                        *total_file_bytes += get_backup_size(&cac);
    }
    if (DataManager_GetIntValue(TW_BACKUP_RECOVERY_VAR) == 1)
    {
        *total_partitions = *total_partitions + 1;
        if (rec.backup == image)    *total_img_bytes += get_backup_size(&rec);
        else                        *total_file_bytes += get_backup_size(&rec);
    }
    if (DataManager_GetIntValue(TW_BACKUP_SP1_VAR) == 1)
    {
        *total_partitions = *total_partitions + 1;
        if (sp1.backup == image)    *total_img_bytes += get_backup_size(&sp1);
        else                        *total_file_bytes += get_backup_size(&sp1);
    }
    if (DataManager_GetIntValue(TW_BACKUP_SP2_VAR) == 1)
    {
        *total_partitions = *total_partitions + 1;
        if (sp2.backup == image)    *total_img_bytes += get_backup_size(&sp2);
        else                        *total_file_bytes += get_backup_size(&sp2);
    }
    if (DataManager_GetIntValue(TW_BACKUP_SP3_VAR) == 1)
    {
        *total_partitions = *total_partitions + 1;
        if (sp3.backup == image)    *total_img_bytes += get_backup_size(&sp3);
        else                        *total_file_bytes += get_backup_size(&sp3);
    }
    if (DataManager_GetIntValue(TW_BACKUP_BOOT_VAR) == 1)
    {
        *total_partitions = *total_partitions + 1;
        if (boo.backup == image)    *total_img_bytes += get_backup_size(&boo);
        else                        *total_file_bytes += get_backup_size(&boo);
    }
    if (DataManager_GetIntValue(TW_BACKUP_ANDSEC_VAR) == 1)
    {
        *total_partitions = *total_partitions + 1;
        if (ase.backup == image)    *total_img_bytes += get_backup_size(&ase);
        else                        *total_file_bytes += get_backup_size(&ase);
    }
    if (DataManager_GetIntValue(TW_BACKUP_SDEXT_VAR) == 1)
    {
        *total_partitions = *total_partitions + 1;
        if (sde.backup == image)    *total_img_bytes += get_backup_size(&sde);
        else                        *total_file_bytes += get_backup_size(&sde);
    }
    return 0;
}

int recursive_mkdir(const char* path)
{
    char pathCpy[512];
    char wholePath[512];
    strcpy(pathCpy, path);
    strcpy(wholePath, "/");

    char* tok = strtok(pathCpy, "/");
    while (tok)
    {
        strcat(wholePath, tok);
        if (mkdir(wholePath, 0777) && errno != EEXIST)
        {
            LOGE("Unable to create folder: %s  (errno=%d)\n", wholePath, errno);
            return -1;
        }
        strcat(wholePath, "/");

        tok = strtok(NULL, "/");
    }
    if (mkdir(wholePath, 0777) && errno != EEXIST)      return -1;
    return 0;
}

int tw_do_backup(const char* enableVar, struct dInfo* mnt, const char* tw_image_dir, unsigned long long img_bytes, unsigned long long file_bytes, unsigned long long* img_bytes_remaining, unsigned long long* file_bytes_remaining, unsigned long* img_byte_time, unsigned long* file_byte_time)
{
    // Check if this partition is being backed up...
	if (!DataManager_GetIntValue(enableVar))    return 0;

    // Let's calculate the percentage of the bar this section is expected to take
    unsigned long int img_bps = DataManager_GetIntValue(TW_BACKUP_AVG_IMG_RATE);
    unsigned long int file_bps;

    if (DataManager_GetIntValue(TW_USE_COMPRESSION_VAR))    file_bps = DataManager_GetIntValue(TW_BACKUP_AVG_FILE_COMP_RATE);
    else                                                    file_bps = DataManager_GetIntValue(TW_BACKUP_AVG_FILE_RATE);

    if (DataManager_GetIntValue(TW_SKIP_MD5_GENERATE_VAR) == 1)
    {
        // If we're skipping MD5 generation, our BPS is faster by about 1.65
        file_bps = (unsigned long) (file_bps * 1.65);
        img_bps = (unsigned long) (img_bps * 1.65);
    }

    // We know the speed for both, how far into the whole backup are we, based on time
    unsigned long total_time = (img_bytes / img_bps) + (file_bytes / file_bps);
    unsigned long remain_time = (*img_bytes_remaining / img_bps) + (*file_bytes_remaining / file_bps);

    float pos = (total_time - remain_time) / (float) total_time;
    ui_set_progress(pos);

    LOGI("Estimated Total time: %lu  Estimated remaining time: %lu\n", total_time, remain_time);

    // Determine how much we're about to process
    unsigned long long blocksize = get_backup_size(mnt);

    // And get the time
    unsigned long section_time;
    if (mnt->backup == image)       section_time = blocksize / img_bps;
    else                            section_time = blocksize / file_bps;

    // Set the position
    pos = section_time / (float) total_time;
    ui_show_progress(pos, section_time);

    time_t start, stop;
    time(&start);

	if (tw_backup(*mnt, tw_image_dir) == 1) { // did the backup process return an error ? 0 = no error
        SetDataState("Backup failed", mnt->mnt, 1, 1);
        ui_print("-- Error occured, check recovery.log. Aborting.\n"); //oh noes! abort abort!
        return 1;
    }
    time(&stop);

    LOGI("Partition Backup time: %d\n", (int) difftime(stop, start));

    // Now, decrement out byte counts
    if (mnt->backup == image)
    {
        *img_bytes_remaining -= blocksize;
        *img_byte_time += (int) difftime(stop, start);
    }
    else
    {
        *file_bytes_remaining -= blocksize;
        *file_byte_time += (int) difftime(stop, start);
    }

    return 0;
}

int nandroid_back_exe()
{
    SetDataState("Starting", "backup", 0, 0);

    if (mount_current_storage() != 0) {
		ui_print("-- Could not mount: %s.\n-- Aborting.\n",SDCARD_ROOT);
        SetDataState("Unable to mount", "storage", 1, 1);
		return 1;
	}

    // Create backup folder
    struct tm *t;
    char timestamp[64];
	char tw_image_dir[255];
	char backup_loc[255];
	char exe[255];
	time_t start, stop;
	time_t seconds;
	seconds = time(0);
    t = localtime(&seconds);

	if (strcmp(DataManager_GetStrValue(TW_BACKUP_NAME), "0") == 0) {
		sprintf(timestamp,"%04d-%02d-%02d--%02d-%02d-%02d",t->tm_year+1900,t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec); // make time stamp
	} else {
		unsigned int copy_size = strlen(DataManager_GetStrValue(TW_BACKUP_NAME));
		if (copy_size > sizeof(timestamp))
			copy_size = sizeof(timestamp);
		memset(timestamp, 0 , sizeof(timestamp));
		strncpy(timestamp, DataManager_GetStrValue(TW_BACKUP_NAME), copy_size);
	}
	strcpy(backup_loc, DataManager_GetStrValue(TW_BACKUPS_FOLDER_VAR));
	sprintf(tw_image_dir,"%s/%s/", backup_loc, timestamp); // for backup folder
	LOGI("Attempt to create folder '%s'\n", tw_image_dir);
    if (recursive_mkdir(tw_image_dir))
    {
        LOGE("Unable to create folder: '%s'\n", tw_image_dir);
        SetDataState("Backup failed", tw_image_dir, 1, 1);
        return -1;
    }

    // Record the start time
    time(&start);

    // Prepare operation
    ui_print("\n[BACKUP STARTED]\n");
    ui_print(" * Backup Folder: %s\n", tw_image_dir);
    update_system_details();
    unsigned long long sdc_free;

	if (DataManager_GetIntValue(TW_USE_EXTERNAL_STORAGE) == 0) {
		struct dInfo* dev;
		LOGI("Using internal storage.\n");
		dev = findDeviceByLabel(DataManager_GetStrValue(TW_INTERNAL_LABEL));
		LOGI("Internal label is: '%s'\n", DataManager_GetStrValue(TW_INTERNAL_LABEL));
		if (dev != NULL) {
			sdc_free = dev->sze - dev->used;
		} else {
			LOGI("Unable to determine free space of internal storage.\n");
			return -1;
		}
	} else {
		LOGI("Using external storage.\n");
		sdc_free = sdcext.sze - sdcext.used;
	}

    // Compute totals
    int tw_total = 0;
    unsigned long long total_img_bytes = 0, total_file_bytes = 0;
	LOGI("Calculating backup details...\n");
    CalculateBackupDetails(&tw_total, &total_img_bytes, &total_file_bytes);
	LOGI("Done calculating backup details.\n");
    unsigned long long total_bytes = total_img_bytes + total_file_bytes;

    if (tw_total == 0 || total_bytes == 0)
    {
        LOGE("Unable to compute target usage (%d partitions, %llu bytes)\n", tw_total, total_bytes);
        SetDataState("Backup failed", tw_image_dir, 1, 1);
        return -1;
    }

    ui_print(" * Total number of partition to back up: %d\n", tw_total);
    ui_print(" * Total size of all data, in KB: %llu\n", total_bytes / 1024);
    ui_print(" * Available space on the SD card, in KB: %llu\n", sdc_free / 1024);

    // Verify space
    if (sdc_free < (total_bytes + 0x2000000))       // We require at least 32MB of additional space
    {
        LOGE("Insufficient space. Required space is %lluKB, available %lluKB\n", (total_bytes + 0x2000000) / 1024, sdc_free / 1024);
        SetDataState("Backup failed", tw_image_dir, 1, 1);
        return -1;
    }

    // Prepare progress bar...
    unsigned long long img_bytes_remaining = total_img_bytes;
    unsigned long long file_bytes_remaining = total_file_bytes;
    unsigned long img_byte_time = 0, file_byte_time = 0;
	struct stat st;

    ui_set_progress(0.0);

	// SYSTEM
    if (tw_do_backup(TW_BACKUP_SYSTEM_VAR, &sys, tw_image_dir, total_img_bytes, total_file_bytes, &img_bytes_remaining, &file_bytes_remaining, &img_byte_time, &file_byte_time))       return 1;

	// DATA
    if (tw_do_backup(TW_BACKUP_DATA_VAR, &dat, tw_image_dir, total_img_bytes, total_file_bytes, &img_bytes_remaining, &file_bytes_remaining, &img_byte_time, &file_byte_time))         return 1;
	if (DataManager_GetIntValue(TW_HAS_DATADATA) == 1)
		if (tw_do_backup(TW_BACKUP_DATA_VAR, &datdat, tw_image_dir, total_img_bytes, total_file_bytes, &img_bytes_remaining, &file_bytes_remaining, &img_byte_time, &file_byte_time))  return 1;

    // BOOT
    if (tw_do_backup(TW_BACKUP_BOOT_VAR, &boo, tw_image_dir, total_img_bytes, total_file_bytes, &img_bytes_remaining, &file_bytes_remaining, &img_byte_time, &file_byte_time))         return 1;

    // RECOVERY
    if (tw_do_backup(TW_BACKUP_RECOVERY_VAR, &rec, tw_image_dir, total_img_bytes, total_file_bytes, &img_bytes_remaining, &file_bytes_remaining, &img_byte_time, &file_byte_time))     return 1;

    // CACHE
    if (tw_do_backup(TW_BACKUP_CACHE_VAR, &cac, tw_image_dir, total_img_bytes, total_file_bytes, &img_bytes_remaining, &file_bytes_remaining, &img_byte_time, &file_byte_time))        return 1;

    // SP1
    if (tw_do_backup(TW_BACKUP_SP1_VAR, &sp1, tw_image_dir, total_img_bytes, total_file_bytes, &img_bytes_remaining, &file_bytes_remaining, &img_byte_time, &file_byte_time))          return 1;

    // SP2
    if (tw_do_backup(TW_BACKUP_SP2_VAR, &sp2, tw_image_dir, total_img_bytes, total_file_bytes, &img_bytes_remaining, &file_bytes_remaining, &img_byte_time, &file_byte_time))          return 1;

    // SP3
    if (tw_do_backup(TW_BACKUP_SP3_VAR, &sp3, tw_image_dir, total_img_bytes, total_file_bytes, &img_bytes_remaining, &file_bytes_remaining, &img_byte_time, &file_byte_time))          return 1;

    // ANDROID-SECURE
	if (stat(ase.dev, &st) ==0)
		if (tw_do_backup(TW_BACKUP_ANDSEC_VAR, &ase, tw_image_dir, total_img_bytes, total_file_bytes, &img_bytes_remaining, &file_bytes_remaining, &img_byte_time, &file_byte_time))   return 1;

    // SD-EXT
	if (stat(sde.dev, &st) ==0)
		if (tw_do_backup(TW_BACKUP_SDEXT_VAR, &sde, tw_image_dir, total_img_bytes, total_file_bytes, &img_bytes_remaining, &file_bytes_remaining, &img_byte_time, &file_byte_time))    return 1;

    update_system_details();

    time(&stop);

    // Average BPS
	if (img_byte_time == 0)
		img_byte_time = 1;
	if (file_byte_time == 0)
		file_byte_time = 1;
    unsigned long int img_bps = total_img_bytes / img_byte_time;
    unsigned long int file_bps = total_file_bytes / file_byte_time;

    LOGI("img_bps = %lu  total_img_bytes = %llu  img_byte_time = %lu\n", img_bps, total_img_bytes, img_byte_time);
    ui_print("Average backup rate for file systems: %lu MB/sec\n", (file_bps / (1024 * 1024)));
    ui_print("Average backup rate for imaged drives: %lu MB/sec\n", (img_bps / (1024 * 1024)));

    if (DataManager_GetIntValue(TW_SKIP_MD5_GENERATE_VAR) == 1)
    {
        // If we're skipping MD5 generation, our BPS is faster by about 1.65
        file_bps = (unsigned long) (file_bps / 1.65);
        img_bps = (unsigned long) (img_bps / 1.65);
    }

    img_bps += (DataManager_GetIntValue(TW_BACKUP_AVG_IMG_RATE) * 4);
    img_bps /= 5;

    if (DataManager_GetIntValue(TW_USE_COMPRESSION_VAR))    file_bps += (DataManager_GetIntValue(TW_BACKUP_AVG_FILE_COMP_RATE) * 4);
    else                                                    file_bps += (DataManager_GetIntValue(TW_BACKUP_AVG_FILE_RATE) * 4);
    file_bps /= 5;

    DataManager_SetIntValue(TW_BACKUP_AVG_IMG_RATE, img_bps);
    if (DataManager_GetIntValue(TW_USE_COMPRESSION_VAR))    DataManager_SetIntValue(TW_BACKUP_AVG_FILE_COMP_RATE, file_bps);
    else                                                    DataManager_SetIntValue(TW_BACKUP_AVG_FILE_RATE, file_bps);

    int total_time = (int) difftime(stop, start);
	unsigned long long actual_backup_size = getUsedSizeViaDu(tw_image_dir);
    actual_backup_size /= (1024LLU * 1024LLU);

	ui_print("[%llu MB TOTAL BACKED UP]\n", actual_backup_size);
	ui_print("[BACKUP COMPLETED IN %d SECONDS]\n\n", total_time); // the end
    SetDataState("Backup Succeeded", "", 0, 1);
	return 0;
}

int tw_restore(struct dInfo rMnt, const char *rDir)
{
	int i;
	FILE *reFp;
	char rUppr[20];
	char rMount[30];
	char rFilesystem[10];
	char rFilename[255];
	char rCommand[255];
	char rOutput[255];
	time_t rStart, rStop;
	strcpy(rUppr,rMnt.mnt);
	for (i = 0; i < (int) strlen(rUppr); i++) {
		rUppr[i] = toupper(rUppr[i]);
	}
	ui_print("[%s]\n",rUppr);
	time(&rStart);
    int md5_result;
	if (DataManager_GetIntValue(TW_SKIP_MD5_CHECK_VAR)) {
		SetDataState("Verifying MD5", rMnt.mnt, 0, 0);
		ui_print("...Verifying md5 hash for %s.\n",rMnt.mnt);
		md5_result = checkMD5(rDir, rMnt.fnm); // verify md5, check if no error; 0 = no error.
	} else {
		md5_result = 0;
		ui_print("Skipping MD5 check based on user setting.\n");
	}
	if(md5_result == 0)
	{
		strcpy(rFilename,rDir);
        if (rFilename[strlen(rFilename)-1] != '/')
        {
            strcat(rFilename, "/");
        }
		strcat(rFilename,rMnt.fnm);
		sprintf(rCommand,"ls -l %s | awk -F'.' '{ print $2 }'",rFilename); // let's get the filesystem type from filename
        reFp = __popen(rCommand, "r");
		LOGI("=> Filename is: %s\n",rMnt.fnm);
		while (fscanf(reFp,"%s",rFilesystem) == 1) { // if we get a match, store filesystem type
			LOGI("=> Filesystem is: %s\n",rFilesystem); // show it off to the world!
		}
		__pclose(reFp);

		if (DataManager_GetIntValue(TW_HAS_DATA_MEDIA) == 1 && strcmp(rMnt.mnt,"data") == 0) {
			wipe_data_without_wiping_media();
		} else if ((DataManager_GetIntValue(TW_RM_RF_VAR) == 1 && (strcmp(rMnt.mnt,"system") == 0 || strcmp(rMnt.mnt,"data") == 0 || strcmp(rMnt.mnt,"cache") == 0)) || strcmp(rMnt.mnt,".android_secure") == 0) { // we'll use rm -rf instead of formatting for system, data and cache if the option is set, always use rm -rf for android secure
			char rCommand2[255];
			ui_print("...using rm -rf to wipe %s\n", rMnt.mnt);
			if (strcmp(rMnt.mnt,".android_secure") == 0) {
				if (DataManager_GetIntValue(TW_HAS_INTERNAL)) {
					ensure_path_mounted(DataManager_GetSettingsStoragePath());
					sprintf(rCommand, "rm -rf %s/%s/*", DataManager_GetStrValue(TW_INTERNAL_PATH), rMnt.dev);
					sprintf(rCommand2, "rm -rf %s/%s/.*", DataManager_GetStrValue(TW_INTERNAL_PATH), rMnt.dev);
				} else {
					ensure_path_mounted(DataManager_GetStrValue(TW_EXTERNAL_PATH));
					sprintf(rCommand, "rm -rf %s/%s/*", DataManager_GetStrValue(TW_EXTERNAL_PATH), rMnt.dev);
					sprintf(rCommand2, "rm -rf %s/%s/.*", DataManager_GetStrValue(TW_EXTERNAL_PATH), rMnt.dev);
				}
			} else {
				tw_mount(rMnt); // mount the partition first
				sprintf(rCommand,"rm -rf %s%s/*", "/", rMnt.mnt);
				sprintf(rCommand2,"rm -rf %s%s/.*", "/", rMnt.mnt);
			}
            SetDataState("Wiping", rMnt.mnt, 0, 0);
			__system(rCommand);
			__system(rCommand2);
			ui_print("....done wiping.\n");
		} else if (rMnt.backup == files) {
			ui_print("...Formatting %s\n",rMnt.mnt);
            SetDataState("Formatting", rMnt.mnt, 0, 0);
			if (strcmp(rMnt.fst, "yaffs2") == 0) {
				if (strcmp(rMnt.mnt, "data") == 0) {
					tw_format(rFilesystem,"userdata"); // on MTD yaffs2, data is actually found under userdata
				} else {
					tw_format(rFilesystem,rMnt.mnt); // use mount location instead of block for formatting on mtd devices
				}
            } else {
			    tw_format(rFilesystem,rMnt.blk); // let's format block, based on filesystem from filename above
            }
			ui_print("....done formatting.\n");
		}

		if (rMnt.backup == files) {
            if (strcmp(rMnt.mnt,".android_secure") == 0) { // if it's android_secure, we have add prefix
                strcpy(rMount, rMnt.dev);
            } else {
				tw_mount(rMnt);
				strcpy(rMount, "/");
				strcat(rMount, rMnt.mnt);
			}
            sprintf(rCommand, "cd %s && tar -xvf %s", rMount, rFilename); // formulate shell command to restore
        } else if (rMnt.backup == image) {
            if (strcmp(rFilesystem, "mtd") == 0) { // if filesystem is mtd, we use flash image
    			sprintf(rCommand, "flash_image %s %s", rMnt.mnt, rFilename);
    			strcpy(rMount, rMnt.mnt);
    		} else { // if filesystem is emmc, we use dd
#ifdef TW_INCLUDE_BLOBPACK
				char blobCommand[1024];

				sprintf(blobCommand, "blobpack /tmp/boot.blob LNX %s", rFilename);
				ui_print("Packing boot into blob\n");
				LOGI("command: %s\n", blobCommand);
				__system(blobCommand);
				sprintf(rCommand, "dd bs=%s if=/tmp/boot.blob of=/dev/block/mmcblk0p4", bs_size);
#else
    			sprintf(rCommand, "dd bs=%s if=%s of=%s", bs_size, rFilename, rMnt.dev);
#endif
    			strcpy(rMount, rMnt.mnt);
    		}
        } else {
            LOGE("Unknown backup method for mount %s\n", rMnt.mnt);
            return 1;
        }

		ui_print("...Restoring %s\n\n",rMount);
        SetDataState("Restoring", rMnt.mnt, 0, 0);
		LOGI("Restore command is: '%s'\n", rCommand);
		reFp = __popen(rCommand, "r");
		if(DataManager_GetIntValue(TW_SHOW_SPAM_VAR) == 2) { // twrp spam
			while (fgets(rOutput,sizeof(rOutput),reFp) != NULL) {
				ui_print_overwrite("%s",rOutput);
			}
		} else {
			while (fscanf(reFp,"%s",rOutput) == 1) {
				if(DataManager_GetIntValue(TW_SHOW_SPAM_VAR) == 1) ui_print_overwrite("%s",rOutput);
			}
		}
		__pclose(reFp);
		ui_print_overwrite("....done restoring.\n");
		if (strcmp(rMnt.mnt, ".android_secure") != 0) // any partition other than android secure,
			tw_unmount(rMnt); // let's unmount (unmountable partitions won't matter)
	} else {
		ui_print("...Failed md5 check. Aborted.\n\n");
		return 1;
	}
	time(&rStop);
	ui_print("[%s DONE (%d SECONDS)]\n\n",rUppr,(int)difftime(rStop,rStart));
	return 0;
}

int 
nandroid_rest_exe()
{
    SetDataState("", "", 0, 0);

    const char* nan_dir = DataManager_GetStrValue("tw_restore");
	ui_print("Restore folder: '%s'\n", nan_dir);
	if (mount_current_storage() != 0) {
		ui_print("-- Could not mount storage.\n-- Aborting.\n");
		return 1;
	}

    int tw_total = 0;
    tw_total += (DataManager_GetIntValue(TW_RESTORE_SYSTEM_VAR) == 1 ? 1 : 0);
    tw_total += (DataManager_GetIntValue(TW_RESTORE_DATA_VAR) == 1 ? 1 : 0);
	if (DataManager_GetIntValue(TW_HAS_DATADATA) == 1 && DataManager_GetIntValue(TW_RESTORE_DATA_VAR) == 1)
		tw_total++;
    tw_total += (DataManager_GetIntValue(TW_RESTORE_CACHE_VAR) == 1 ? 1 : 0);
    //tw_total += (DataManager_GetIntValue(TW_RESTORE_RECOVERY_VAR) == 1 ? 1 : 0);
    tw_total += (DataManager_GetIntValue(TW_RESTORE_SP1_VAR) == 1 ? 1 : 0);
    tw_total += (DataManager_GetIntValue(TW_RESTORE_SP2_VAR) == 1 ? 1 : 0);
    tw_total += (DataManager_GetIntValue(TW_RESTORE_SP3_VAR) == 1 ? 1 : 0);
    tw_total += (DataManager_GetIntValue(TW_RESTORE_BOOT_VAR) == 1 ? 1 : 0);
    tw_total += (DataManager_GetIntValue(TW_RESTORE_ANDSEC_VAR) == 1 ? 1 : 0);
    tw_total += (DataManager_GetIntValue(TW_RESTORE_SDEXT_VAR) == 1 ? 1 : 0);

    float sections = 1.0 / tw_total;

	time_t rStart, rStop;
	time(&rStart);
	ui_print("\n[RESTORE STARTED]\n\n");
	if (DataManager_GetIntValue(TW_RESTORE_SYSTEM_VAR) == 1) {
        ui_show_progress(sections, 150);
		if (tw_restore(sys,nan_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
            SetDataState("Restore failed", "", 1, 1);
			return 1;
		}
	}
    if (DataManager_GetIntValue(TW_RESTORE_DATA_VAR) == 1) {
        ui_show_progress(sections, 150);
		if (tw_restore(dat,nan_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
            SetDataState("Restore failed", "", 1, 1);
			return 1;
		}
		if (DataManager_GetIntValue(TW_HAS_DATADATA) == 1) {
			ui_show_progress(sections, 150);
			if (tw_restore(datdat,nan_dir) == 1) {
				ui_print("-- Error occured, check recovery.log. Aborting.\n");
				SetDataState("Restore failed", "", 1, 1);
				return 1;
			}
		}
	}
    if (DataManager_GetIntValue(TW_RESTORE_BOOT_VAR) == 1) {
        ui_show_progress(sections, 150);
		if (tw_restore(boo,nan_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
            SetDataState("Restore failed", "", 1, 1);
			return 1;
		}
	}
    /*if (DataManager_GetIntValue(TW_RESTORE_RECOVERY_VAR) == 1) {
        ui_show_progress(sections, 150);
		if (tw_restore(rec,nan_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
            SetDataState("Restore failed", "", 1, 1);
			return 1;
		}
	}*/
    if (DataManager_GetIntValue(TW_RESTORE_CACHE_VAR) == 1) {
        ui_show_progress(sections, 150);
		if (tw_restore(cac,nan_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
            SetDataState("Restore failed", "", 1, 1);
			return 1;
		}
	}
    if (DataManager_GetIntValue(TW_RESTORE_SP1_VAR) == 1) {
        ui_show_progress(sections, 150);
		if (tw_restore(sp1,nan_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
            SetDataState("Restore failed", "", 1, 1);
			return 1;
		}
	}
    if (DataManager_GetIntValue(TW_RESTORE_SP2_VAR) == 1) {
        ui_show_progress(sections, 150);
        if (tw_restore(sp2,nan_dir) == 1) {
            ui_print("-- Error occured, check recovery.log. Aborting.\n");
            SetDataState("Restore failed", "", 1, 1);
            return 1;
        }
    }
    if (DataManager_GetIntValue(TW_RESTORE_SP3_VAR) == 1) {
        ui_show_progress(sections, 150);
        if (tw_restore(sp3,nan_dir) == 1) {
            ui_print("-- Error occured, check recovery.log. Aborting.\n");
            SetDataState("Restore failed", "", 1, 1);
            return 1;
        }
    }
    if (DataManager_GetIntValue(TW_RESTORE_ANDSEC_VAR) == 1) {
        ui_show_progress(sections, 150);
		if (tw_restore(ase,nan_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
            SetDataState("Restore failed", "", 1, 1);
			return 1;
		}
	}
    if (DataManager_GetIntValue(TW_RESTORE_SDEXT_VAR) == 1) {
        ui_show_progress(sections, 150);
		if (tw_restore(sde,nan_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
            SetDataState("Restore failed", "", 1, 1);
			return 1;
		}
	}
	time(&rStop);
	ui_print("[RESTORE COMPLETED IN %d SECONDS]\n\n",(int)difftime(rStop,rStart));
	__system("sync");
	update_system_details();
    SetDataState("Restore Succeeded", "", 0, 1);
	return 0;
}

static int compare_string(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

void choose_backup_folder() 
{
    mount_current_storage();

	struct stat st;
    char tw_dir[255];
    sprintf(tw_dir, "%s/%s/", backup_folder, device_id);
    if (stat(backup_folder,&st) != 0) {
    	if (mkdir(backup_folder, 0777) != 0) {
    		LOGI("=> Can not create %s\n", backup_folder);
    	} else {
    		LOGI("=> Created folder %s\n", backup_folder);
    	}
    }
    if (stat(tw_dir,&st) != 0) {
    	if (mkdir(tw_dir, 0777) != 0) {
    		LOGI("=> Can not create %s\n", tw_dir);
    	} else {
    		LOGI("=> Created folder %s\n", tw_dir);
    	}
    }
    
    const char* MENU_HEADERS[] = { "Choose a Nandroid Folder:",
    							   tw_dir,
                                   NULL };
    
    char** headers = prepend_title((const char**) MENU_HEADERS);
    
    DIR* d;
    struct dirent* de;
    d = opendir(tw_dir);
    if (d == NULL) {
        LOGE("error opening %s: %s\n", tw_dir, strerror(errno));
        return;
    }

    int d_size = 0;
    int d_alloc = 10;
    char** dirs = malloc(d_alloc * sizeof(char*));
    int z_size = 1;
    int z_alloc = 10;
    char** zips = malloc(z_alloc * sizeof(char*));
    zips[0] = strdup("../");
    
    inc_menu_loc(0);
    while ((de = readdir(d)) != NULL) {
        int name_len = strlen(de->d_name);

        if (de->d_type == DT_DIR) {
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
        }
    }
    closedir(d);

    qsort(dirs, d_size, sizeof(char*), compare_string);
    qsort(zips, z_size, sizeof(char*), compare_string);

    if (d_size + z_size + 1 > z_alloc) {
        z_alloc = d_size + z_size + 1;
        zips = realloc(zips, z_alloc * sizeof(char*));
    }
    memcpy(zips + z_size, dirs, d_size * sizeof(char*));
    free(dirs);
    z_size += d_size;
    zips[z_size] = NULL;

    int chosen_item = 0;
    for (;;) {
        chosen_item = get_menu_selection(headers, zips, 1, chosen_item);

        char* item = zips[chosen_item];
        int item_len = strlen(item);

        if (chosen_item == 0) {
            break;
        } else if (item[item_len-1] == '/') {
            char nan_dir[512];

            strcpy(nan_dir, tw_dir);
            strcat(nan_dir, item);

            DataManager_SetStrValue("tw_restore", nan_dir);

            set_restore_files();
            nan_restore_menu(0);
            break;
        }
    }

    int i;
    for (i = 0; i < z_size; ++i) free(zips[i]);
    free(zips);

    dec_menu_loc();
}

int makeMD5(const char *imgDir, const char *imgFile)
{
	int bool = 1;

    if (DataManager_GetIntValue(TW_SKIP_MD5_GENERATE_VAR) == 1) {
        // When skipping the generate, we return success
        return 0;
    }

	if (mount_current_storage() != 0) {
		LOGI("=> Can not mount storage.\n");
		return bool;
	} else {
		FILE *fp;
		char tmpString[255];
		char tmpAnswer[15];
		sprintf(tmpString,"cd %s && md5sum %s > %s.md5",imgDir,imgFile,imgFile);
		fp = __popen(tmpString, "r");
		fgets(tmpString, 255, fp);
		getWordFromString(1, tmpString, tmpAnswer, sizeof(tmpAnswer));
		if (strcmp(tmpAnswer,"md5sum:") == 0) {
            ui_print("....MD5 Error: %s (%s)", tmpString, tmpAnswer);
		} else {
			ui_print("....MD5 Created.\n");
			bool = 0;
		}
		__pclose(fp);
	}
	return bool;
}

int checkMD5(const char *imgDir, const char *imgFile)
{
	int bool = 1;

    if (DataManager_GetIntValue(TW_SKIP_MD5_CHECK_VAR) == 1) {
        // When skipping the test, we return success
        return 0;
    }

	if (mount_current_storage() != 0) {
		LOGI("=> Can not mount storage.\n");
		return bool;
	} else {
		FILE *fp;
		char tmpString[255];
		char tmpAnswer[15];
		sprintf(tmpString,"cd %s && md5sum -c %s.md5",imgDir,imgFile);
		fp = __popen(tmpString, "r");
		fgets(tmpString, 255, fp);
		getWordFromString(2, tmpString, tmpAnswer, sizeof(tmpAnswer));
		if (strcmp(tmpAnswer,"OK") == 0) {
			ui_print("....MD5 Check: %s\n", tmpAnswer);
			bool = 0;
		} else {
			ui_print("....MD5 Error: %s (%s)", tmpString, tmpAnswer);
		}
		__pclose(fp);
	}
	return bool;
}
