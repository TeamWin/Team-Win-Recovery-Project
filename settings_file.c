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
#include <stdlib.h>
#include <time.h>

#include "backstore.h"
#include "settings_file.h"
#include "common.h"
#include "roots.h"
#include "themes.h"

// Default Settings
void
tw_set_defaults() {
	strcpy(tw_nan_system_val, "1");
	strcpy(tw_nan_data_val, "1");
	strcpy(tw_nan_boot_val, "1");
	strcpy(tw_nan_recovery_val, "0");
	strcpy(tw_nan_cache_val, "0");
	strcpy(tw_nan_wimax_val, "0");
	strcpy(tw_nan_andsec_val, "0");
	strcpy(tw_nan_sdext_val, "0");
	strcpy(tw_reboot_after_flash_option, "0");
    strcpy(tw_signed_zip_val, "0");
	strcpy(tw_force_md5_check_val, "0");
	strcpy(tw_color_theme_val, "0");
	strcpy(tw_use_compression_val, "0");
	strcpy(tw_show_spam_val, "0");
    strcpy(tw_time_zone_val, "CST6CDT");
	strcpy(tw_zip_location_val, "/sdcard");
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
		LOGI("=> Can not mount /sdcard, running on default settings\n"); // Can't mount sdcard, default settings should be unchanged.
	} else {
		struct stat st;
		if(stat("/sdcard/TWRP",&st) != 0) {
			LOGI("=> /sdcard/TWRP directory not present!\n");
			if(mkdir("/sdcard/TWRP",0777) == -1) { // create directory
				nan_dir_status = 0;
				LOGI("=> Can not create directory: /sdcard/TWRP\n");
			} else {
				nan_dir_status = 1;
				LOGI("=> Created directory: /sdcard/TWRP\n");
			}
		}
		if (nan_dir_status == 1) {
			FILE *fp; // define file
			fp = fopen(TW_SETTINGS_FILE, "w"); // open file, create if not exist, if exist, overwrite contents
			if (fp == NULL) {
				LOGI("=> Can not open settings file to write.\n"); // Can't open/create file, default settings still loaded into memory.
			} else {
				// Save all settings info to settings file in format KEY:VALUE
                fprintf(fp, "%s:%s\n", TW_VERSION, tw_version_val);
                fprintf(fp, "%s:%s\n", TW_NAN_SYSTEM, tw_nan_system_val);
                fprintf(fp, "%s:%s\n", TW_NAN_DATA, tw_nan_data_val);
                fprintf(fp, "%s:%s\n", TW_NAN_BOOT, tw_nan_boot_val);
                fprintf(fp, "%s:%s\n", TW_NAN_RECOVERY, tw_nan_recovery_val);
                fprintf(fp, "%s:%s\n", TW_NAN_CACHE, tw_nan_cache_val);
                fprintf(fp, "%s:%s\n", TW_NAN_WIMAX, tw_nan_wimax_val);
                fprintf(fp, "%s:%s\n", TW_NAN_ANDSEC, tw_nan_andsec_val);
                fprintf(fp, "%s:%s\n", TW_NAN_SDEXT, tw_nan_sdext_val);
                fprintf(fp, "%s:%s\n", TW_REBOOT_AFTER_FLASH, tw_reboot_after_flash_option);
                fprintf(fp, "%s:%s\n", TW_SIGNED_ZIP, tw_signed_zip_val);
                fprintf(fp, "%s:%s\n", TW_COLOR_THEME, tw_color_theme_val);
                fprintf(fp, "%s:%s\n", TW_USE_COMPRESSION, tw_use_compression_val);
                fprintf(fp, "%s:%s\n", TW_SHOW_SPAM, tw_show_spam_val);
                fprintf(fp, "%s:%s\n", TW_TIME_ZONE, tw_time_zone_val);
                fprintf(fp, "%s:%s\n", TW_ZIP_LOCATION, tw_zip_location_val);
                fprintf(fp, "%s:%s\n", TW_FORCE_MD5_CHECK, tw_force_md5_check_val);
				fclose(fp); // close file
				//LOGI("=> Wrote to configuration file: %s\n", TW_SETTINGS_FILE); // log
			}
		}
	}
}

// Read from Settings file Function
void
read_s_file() {
	if (ensure_path_mounted("/sdcard") != 0) {
		LOGI("=> Can not mount /sdcard, running on default settings\n"); // Can't mount sdcard, default settings should be unchanged.
	} else {
		FILE *fp; // define file
		fp = fopen(TW_SETTINGS_FILE, "r"); // Open file for read
		if (fp == NULL) {
			LOGI("=> Can not open settings file, will try to create file.\n"); // Can't open file, default settings should be unchanged.
			write_s_file(); // call save settings function if settings file doesn't exist
		} else {
			tw_set_defaults(); // Set defaults to unwritten settings will be present as well
			char s_buffer[2 * TW_MAX_SETTINGS_CHARS];
            char s_key[TW_MAX_SETTINGS_CHARS];
            char s_value[TW_MAX_SETTINGS_CHARS]; // Set max characters + 2 (because of terminating and carriage return)
			while(fgets(s_buffer, 2 * TW_MAX_SETTINGS_CHARS, fp)) {
			    // Parse the line
			    sscanf(s_buffer, "%[^:] %*c %s", s_key, s_value);
			
				if (strcmp(s_key, TW_VERSION) == 0) {
					if (strcmp(s_value, tw_version_val) != 0) {
						LOGI("=> Wrong recoverywin version detected, default settings applied.\n"); //
					}
				} else if (strcmp(s_key, TW_NAN_SYSTEM) == 0 ) {
			    	strcpy(tw_nan_system_val, s_value);
                } else if (strcmp(s_key, TW_NAN_DATA) == 0 ) {
			    	strcpy(tw_nan_data_val, s_value);
				} else if (strcmp(s_key, TW_NAN_BOOT) == 0 ) {
			    	strcpy(tw_nan_boot_val, s_value);
				} else if (strcmp(s_key, TW_NAN_RECOVERY) == 0 ) {
			    	strcpy(tw_nan_recovery_val, s_value);
				} else if (strcmp(s_key, TW_NAN_CACHE) == 0 ) {
			    	strcpy(tw_nan_cache_val, s_value);
				} else if (strcmp(s_key, TW_NAN_WIMAX) == 0 ) {
			    	strcpy(tw_nan_wimax_val, s_value);
				} else if (strcmp(s_key, TW_NAN_ANDSEC) == 0 ) {
			    	strcpy(tw_nan_andsec_val, s_value);
				} else if (strcmp(s_key, TW_NAN_SDEXT) == 0 ) {
			    	strcpy(tw_nan_sdext_val, s_value);
				} else if (strcmp(s_key, TW_REBOOT_AFTER_FLASH) == 0 ) {
			    	strcpy(tw_reboot_after_flash_option, s_value);
				} else if (strcmp(s_key, TW_SIGNED_ZIP) == 0 ) {
			    	strcpy(tw_signed_zip_val, s_value);
			    } else if (strcmp(s_key, TW_COLOR_THEME) == 0 ) {
			    	strcpy(tw_color_theme_val, s_value);
			    } else if (strcmp(s_key, TW_USE_COMPRESSION) == 0 ) {
			    	strcpy(tw_use_compression_val, s_value);
				} else if (strcmp(s_key, TW_SHOW_SPAM) == 0 ) {
			    	strcpy(tw_show_spam_val, s_value);
				} else if (strcmp(s_key, TW_TIME_ZONE) == 0 ) {
			    	strcpy(tw_time_zone_val, s_value);
				} else if (strcmp(s_key, TW_ZIP_LOCATION) == 0 ) {
			    	strcpy(tw_zip_location_val, s_value);
                } else if (strcmp(s_key, TW_FORCE_MD5_CHECK) == 0) {
                    strcpy(tw_force_md5_check_val, s_value);
				} 
			}
			fclose(fp); // close file
		}
	}
	update_tz_environment_variables();
	set_theme(tw_color_theme_val);
}
