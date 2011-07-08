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
#include <string.h>

#include "backstore.h"
#include "ddftw.h"
#include "extra-functions.h"
#include "roots.h"
#include "settings_file.h"

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

    inc_menu_loc(ITEM_MENU_BACK);
	ui_print("=> index: %i\n", menu_loc_idx);
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
            	menu_loc_idx--;
            	ui_print("=> index: %i\n", menu_loc_idx);
				return;
		}
	    if (go_home) { 
	        menu_loc_idx--;
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

    inc_menu_loc(ITEM_NAN_BACK);
	ui_print("=> index: %i\n", menu_loc_idx);
	for (;;)
	{
		int chosen_item = get_menu_selection(nan_b_headers, nan_b_items, 0, 0);
		switch (chosen_item)
		{
			case ITEM_NAN_BACKUP:
				//nandroid_string();
				nandroid_back_exe();
				//ui_print("\nNandroid String : %s\n\n", tw_nandroid_string);
				return;
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
            	menu_loc_idx--;
            	ui_print("=> index: %i\n", menu_loc_idx);
				return;
		}
	    if (go_home) { 
	        menu_loc_idx--;
	        return;
	    }
		break;
	}
	ui_end_menu();
    menu_loc_idx--;
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

    inc_menu_loc(ITEM_NAN_RESBACK);
	ui_print("=> index: %i\n", menu_loc_idx);
	for (;;)
	{
		int chosen_item = get_menu_selection(nan_r_headers, nan_r_items, 0, 0);
		switch (chosen_item)
		{
			case ITEM_NAN_RESTORE:
				break;
			case ITEM_NAN_RESBACK:
		        menu_loc_idx--;
            	ui_print("=> index: %i\n", menu_loc_idx);
				return;
		}
	    if (go_home) { 
	        menu_loc_idx--;
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
nandroid_back_exe()
{
	char exe[255];
	char tw_image[150];
	if (is_true(tw_nan_system_val)) {
		strcpy(tw_image, "/sdcard/nandroid/system.img");
		sprintf(exe,"dd bs=2048 if=%s of=%s", tw_system_loc, tw_image);
		__system(exe);
	}
	if (is_true(tw_nan_data_val)) {
		strcpy(tw_image, "/sdcard/nandroid/data.img");
		sprintf(exe,"dd bs=2048 if=%s of=%s", tw_data_loc, tw_image);
		__system(exe);
	}
	if (is_true(tw_nan_cache_val)) {
		strcpy(tw_image, "/sdcard/nandroid/cache.img");
		sprintf(exe,"dd bs=2048 if=%s of=%s", tw_cache_loc, tw_image);
		__system(exe);
	}
	if (is_true(tw_nan_boot_val)) {
		strcpy(tw_image, "/sdcard/nandroid/boot.img");
		sprintf(exe,"dd bs=2048 if=%s of=%s", tw_boot_loc, tw_image);
		__system(exe);
	}
	if (is_true(tw_nan_wimax_val)) {
		strcpy(tw_image, "/sdcard/nandroid/wimax.img");
		sprintf(exe,"dd bs=2048 if=%s of=%s", tw_wimax_loc, tw_image);
		__system(exe);
	}
	if (is_true(tw_nan_recovery_val)) {
		strcpy(tw_image, "/sdcard/nandroid/recovery.img");
		sprintf(exe,"dd bs=2048 if=%s of=%s", tw_recovery_loc, tw_image);
		__system(exe);
	}
//	if (is_true(tw_nan_sdext_val) == 1) {
//		nandroid_back_exe(tw_system_loc, "");
//	}
//	if (is_true(tw_nan_andsec_val) == 1) {
//		nandroid_back_exe(tw_system_loc, "");
//	}
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