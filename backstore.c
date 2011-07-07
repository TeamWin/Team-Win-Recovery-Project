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
#include <sys/types.h>
#include <sys/stat.h>

#include "backstore.h"
#include "ddftw.h"
#include "extra-functions.h"
#include "common.h"
#include "roots.h"

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
	if (ensure_path_mounted("/sdcard") != 0) {
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
	if (ensure_path_mounted("/sdcard") != 0) {
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
        save_up_a_level_menu_location(ITEM_MENU_BACK);
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
				return;
		}
		break;
	}
	ui_end_menu();
	decrement_menu_location();
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