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
				int i = 0;
				while(i < TW_MAX_NUM_SETTINGS) {
					if (i == TW_VERSION) {
						fputs(tw_version_val, fp);
					} else if (i == TW_NAN_SYSTEM) {
						fputs(tw_nan_system_val, fp);
					} else if (i == TW_NAN_DATA) {
						fputs(tw_nan_data_val, fp);
					} else if (i == TW_NAN_BOOT) {
						fputs(tw_nan_boot_val, fp);
					} else if (i == TW_NAN_RECOVERY) {
						fputs(tw_nan_recovery_val, fp);
					} else if (i == TW_NAN_CACHE) {
						fputs(tw_nan_cache_val, fp);
					} else if (i == TW_NAN_WIMAX) {
						fputs(tw_nan_wimax_val, fp);
					} else if (i == TW_NAN_ANDSEC) {
						fputs(tw_nan_andsec_val, fp);
					} else if (i == TW_NAN_SDEXT) {
						fputs(tw_nan_sdext_val, fp);
					} else if (i == TW_REBOOT_AFTER_FLASH) {
						fputs(tw_reboot_after_flash_option, fp);
					} else if (i == TW_SIGNED_ZIP) {
						fputs(tw_signed_zip_val, fp);
					} else if (i == TW_COLOR_THEME) {
						fputs(tw_color_theme_val, fp);
					} else if (i == TW_USE_COMPRESSION) {
						fputs(tw_use_compression_val, fp);
					} else if (i == TW_SHOW_SPAM) {
						fputs(tw_show_spam_val, fp);
					} else if (i == TW_TIME_ZONE) {
						fputs(tw_time_zone_val, fp);
					} else if (i == TW_ZIP_LOCATION) {
						fputs(tw_zip_location_val, fp);
					} 
					fputs("\n", fp); // add a carriage return to finish line
					i++; // increment loop
				}
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
						LOGI("=> Wrong recoverywin version detected, default settings applied.\n"); //
						tw_set_defaults();
						write_s_file();
						break;
					}
				} else if (i == TW_NAN_SYSTEM) {
			    	strcpy(tw_nan_system_val, s_line);
                } else if (i == TW_NAN_DATA) {
			    	strcpy(tw_nan_data_val, s_line);
				} else if (i == TW_NAN_BOOT) {
			    	strcpy(tw_nan_boot_val, s_line);
				} else if (i == TW_NAN_RECOVERY) {
			    	strcpy(tw_nan_recovery_val, s_line);
				} else if (i == TW_NAN_CACHE) {
			    	strcpy(tw_nan_cache_val, s_line);
				} else if (i == TW_NAN_WIMAX) {
			    	strcpy(tw_nan_wimax_val, s_line);
				} else if (i == TW_NAN_ANDSEC) {
			    	strcpy(tw_nan_andsec_val, s_line);
				} else if (i == TW_NAN_SDEXT) {
			    	strcpy(tw_nan_sdext_val, s_line);
				} else if (i == TW_REBOOT_AFTER_FLASH) {
			    	strcpy(tw_reboot_after_flash_option, s_line);
				} else if (i == TW_SIGNED_ZIP) {
			    	strcpy(tw_signed_zip_val, s_line);
			    } else if (i == TW_COLOR_THEME) {
			    	strcpy(tw_color_theme_val, s_line);
			    } else if (i == TW_USE_COMPRESSION) {
			    	strcpy(tw_use_compression_val, s_line);
				} else if (i == TW_SHOW_SPAM) {
			    	strcpy(tw_show_spam_val, s_line);
				} else if (i == TW_TIME_ZONE) {
			    	strcpy(tw_time_zone_val, s_line);
				} else if (i == TW_ZIP_LOCATION) {
			    	strcpy(tw_zip_location_val, s_line);
				} 
				i++; // increment loop
			}
			fclose(fp); // close file
		}
	}
	update_tz_environment_variables();
	set_theme(tw_color_theme_val);
}