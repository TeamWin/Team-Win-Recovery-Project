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
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <sys/reboot.h>

#include "backstore.h"
#include "ddftw.h"
#include "extra-functions.h"
#include "roots.h"
#include "settings_file.h"

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
	
	char** headers = prepend_title(nan_headers);
	
	int chosen_item;
    inc_menu_loc(ITEM_MENU_BACK); // record back selection into array
	for (;;)
	{
         ui_set_background(BACKGROUND_ICON_NANDROID);
		chosen_item = get_menu_selection(headers, nan_items, 0, 0);
		switch (chosen_item)
		{
			case ITEM_BACKUP_MENU:
				nan_backup_menu(0);
				break;
			case ITEM_RESTORE_MENU:
                choose_backup_folder();
				break;
			case ITEM_MENU_RBOOT:
				tw_reboot();
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
#define ITEM_NAN_WIMAX      6
#define ITEM_NAN_ANDSEC     7
#define ITEM_NAN_SDEXT      8
#define ITEM_NAN_COMPRESS   9
#define ITEM_NAN_BACK       10

char* nan_compress()
{
	char* tmp_set = (char*)malloc(40);
	strcpy(tmp_set, "[ ] Compress Backup (Slow but Saves Space)");
	if (is_true(tw_use_compression_val) == 1) {
		tmp_set[1] = 'x';
	}
	return tmp_set;
}

void
nan_backup_menu(int pIdx)
{
	tw_total = 0;
	char* nan_b_headers[] = { "Nandroid Backup",
							   "Choose Backup Options:",
							   NULL	};
	
	char* nan_b_items[] = { "--> Backup Naowz!",
							 nan_img_set(ITEM_NAN_SYSTEM,0),
							 nan_img_set(ITEM_NAN_DATA,0),
							 nan_img_set(ITEM_NAN_BOOT,0),
							 nan_img_set(ITEM_NAN_RECOVERY,0),
							 nan_img_set(ITEM_NAN_CACHE,0),
							 nan_img_set(ITEM_NAN_WIMAX,0),
							 nan_img_set(ITEM_NAN_ANDSEC,0),
							 nan_img_set(ITEM_NAN_SDEXT,0),
							 nan_compress(),
							 "<-- Back To Nandroid Menu",
							 NULL };

	char** headers = prepend_title(nan_b_headers);
	
    inc_menu_loc(ITEM_NAN_BACK);
	for (;;)
	{
		int chosen_item = get_menu_selection(headers, nan_b_items, 0, pIdx); // get key presses
		pIdx = chosen_item; // remember last selection location
		switch (chosen_item)
		{
			case ITEM_NAN_BACKUP:
				if (tw_total > 0) // only call backup if something was checked
				{
					nandroid_back_exe();
					dec_menu_loc();
					return;
				}
				break;
			case ITEM_NAN_SYSTEM:
            	if (is_true(tw_nan_system_val)) {
            		strcpy(tw_nan_system_val, "0"); // toggle's value
            		tw_total--;
            	} else {
            		strcpy(tw_nan_system_val, "1");
            		tw_total++;
            	}
                write_s_file();
                break;
			case ITEM_NAN_DATA:
            	if (is_true(tw_nan_data_val)) {
            		strcpy(tw_nan_data_val, "0");
            		tw_total--;
            	} else {
            		strcpy(tw_nan_data_val, "1");
            		tw_total++;
            	}
                write_s_file();
				break;
			case ITEM_NAN_BOOT:
            	if (is_true(tw_nan_boot_val)) {
            		strcpy(tw_nan_boot_val, "0");
            		tw_total--;
            	} else {
            		strcpy(tw_nan_boot_val, "1");
            		tw_total++;
            	}
                write_s_file();
				break;
			case ITEM_NAN_RECOVERY:
            	if (is_true(tw_nan_recovery_val)) {
            		strcpy(tw_nan_recovery_val, "0");
            		tw_total--;
            	} else {
            		strcpy(tw_nan_recovery_val, "1");
            		tw_total++;
            	}
                write_s_file();
				break;
			case ITEM_NAN_CACHE:
            	if (is_true(tw_nan_cache_val)) {
            		strcpy(tw_nan_cache_val, "0");
            		tw_total--;
            	} else {
            		strcpy(tw_nan_cache_val, "1");
            		tw_total++;
            	}
                write_s_file();
				break;
			case ITEM_NAN_WIMAX:
				if (tw_nan_wimax_x != -1)
				{
	            	if (is_true(tw_nan_wimax_val)) {
	            		strcpy(tw_nan_wimax_val, "0");
	            		tw_total--;
	            	} else {
	            		strcpy(tw_nan_wimax_val, "1");
	            		tw_total++;
	            	}
	                write_s_file();
				}
				break;
			case ITEM_NAN_ANDSEC:
				if (tw_nan_andsec_x != -1)
				{
	            	if (is_true(tw_nan_andsec_val)) {
	            		strcpy(tw_nan_andsec_val, "0");
	            		tw_total--;
	            	} else {
	            		strcpy(tw_nan_andsec_val, "1");
	            		tw_total++;
	            	}
	                write_s_file();
				}
				break;
			case ITEM_NAN_SDEXT:
				if (tw_nan_sdext_x != -1)
				{
	            	if (is_true(tw_nan_sdext_val)) {
	            		strcpy(tw_nan_sdext_val, "0");
	            		tw_total--;
	            	} else {
	            		strcpy(tw_nan_sdext_val, "1");
	            		tw_total++;
	           		}
	                write_s_file();
				}
				break;
			case ITEM_NAN_COMPRESS:
            	if (is_true(tw_use_compression_val)) {
            		strcpy(tw_use_compression_val, "0");
            	} else {
            		strcpy(tw_use_compression_val, "1");
           		}
                write_s_file();
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
    dec_menu_loc();
	nan_backup_menu(pIdx); // restart menu (to refresh it)
}

void
set_restore_files()
{
	struct stat st;
	char* tmp_file = (char*)malloc(255);
	
	// check to see if images exist, so put int menu array
	strcpy(tmp_file,nan_dir);
	strcat(tmp_file,tw_nan_system);
	if (stat(tmp_file,&st) == 0) 
	{
		tw_nan_system_x = 1;
		tw_total++;
	} else {
		tw_nan_system_x = -1;
	}
	strcpy(tmp_file,nan_dir);
	strcat(tmp_file,tw_nan_data);
	if (stat(tmp_file,&st) == 0) 
	{
		tw_nan_data_x = 1;
		tw_total++;
	} else {
		tw_nan_data_x = -1;
	}
	strcpy(tmp_file,nan_dir);
	strcat(tmp_file,tw_nan_boot);
	if (stat(tmp_file,&st) == 0) 
	{
		tw_nan_boot_x = 1;
		tw_total++;
	} else {
		tw_nan_boot_x = -1;
	}
	strcpy(tmp_file,nan_dir);
	strcat(tmp_file,tw_nan_recovery);
	if (stat(tmp_file,&st) == 0) 
	{
		tw_nan_recovery_x = 1;
		tw_total++;
	} else {
		tw_nan_recovery_x = -1;
	}	
	strcpy(tmp_file,nan_dir);
	strcat(tmp_file,tw_nan_cache);
	if (stat(tmp_file,&st) == 0) 
	{
		tw_nan_cache_x = 1;
		tw_total++;
	} else {
		tw_nan_cache_x = -1;
	}	
	strcpy(tmp_file,nan_dir);
	strcat(tmp_file,tw_nan_wimax);
	if (stat(tmp_file,&st) == 0) 
	{
		tw_nan_wimax_x = 1;
		tw_total++;
	} else {
		tw_nan_wimax_x = -1;
	}		
	strcpy(tmp_file,nan_dir);
	strcat(tmp_file,tw_nan_andsec);
	if (stat(tmp_file,&st) == 0) 
	{
		tw_nan_andsec_x = 1;
		tw_total++;
	} else {
		tw_nan_andsec_x = -1;
	}		
	strcpy(tmp_file,nan_dir);
	strcat(tmp_file,tw_nan_sdext);
	if (stat(tmp_file,&st) == 0) 
	{
		tw_nan_sdext_x = 1;
		tw_total++;
	} else {
		tw_nan_sdext_x = -1;
	}
	free(tmp_file);
}

void
nan_restore_menu(int pIdx)
{
	tw_total = 0;
	char* nan_r_headers[] = { "Nandroid Restore",
							  "Choose Restore Options:",
							  NULL	};
	
	char* nan_r_items[] = { "--> Restore Naowz!",
			 	 	 	 	nan_img_set(ITEM_NAN_SYSTEM,1),
			 	 	 	 	nan_img_set(ITEM_NAN_DATA,1),
			 	 	 	 	nan_img_set(ITEM_NAN_BOOT,1),
			 	 	 	 	nan_img_set(ITEM_NAN_RECOVERY,1),
			 	 	 	 	nan_img_set(ITEM_NAN_CACHE,1),
			 	 	 	 	nan_img_set(ITEM_NAN_WIMAX,1),
			 	 	 	 	nan_img_set(ITEM_NAN_ANDSEC,1),
			 	 	 	 	nan_img_set(ITEM_NAN_SDEXT,1),
							"<-- Back To Nandroid Menu",
							 NULL };

	char** headers = prepend_title(nan_r_headers);
	
    inc_menu_loc(ITEM_NAN_BACK);
	for (;;)
	{
		int chosen_item = get_menu_selection(headers, nan_r_items, 0, pIdx);
		pIdx = chosen_item; // remember last selection location
		switch (chosen_item)
		{
		case ITEM_NAN_BACKUP:
			if (tw_total > 0) // only call restore if something was checked
			{
				nandroid_rest_exe();
				dec_menu_loc();
				return;
			}
			break;
		case ITEM_NAN_SYSTEM:
			if (tw_nan_system_x == 0)
			{
				tw_nan_system_x = 1;
        		tw_total++;
			} else if (tw_nan_system_x == 1) {
				tw_nan_system_x = 0;
        		tw_total--;
			}
            break;
		case ITEM_NAN_DATA:
			if (tw_nan_data_x == 0)
			{
				tw_nan_data_x = 1;
        		tw_total++;
			} else if (tw_nan_data_x == 1) {
				tw_nan_data_x = 0;
        		tw_total--;
			}
			break;
		case ITEM_NAN_BOOT:
			if (tw_nan_boot_x == 0)
			{
				tw_nan_boot_x = 1;
        		tw_total++;
			} else if (tw_nan_boot_x == 1) {
				tw_nan_boot_x = 0;
        		tw_total--;
			}
			break;
		case ITEM_NAN_RECOVERY:
			if (tw_nan_recovery_x == 0)
			{
				tw_nan_recovery_x = 1;
        		tw_total++;
			} else if (tw_nan_recovery_x == 1) {
				tw_nan_recovery_x = 0;
        		tw_total--;
			}
			break;
		case ITEM_NAN_CACHE:
			if (tw_nan_cache_x == 0)
			{
				tw_nan_cache_x = 1;
        		tw_total++;
			} else if (tw_nan_cache_x == 1) {
				tw_nan_cache_x = 0;
        		tw_total--;
			}
			break;
		case ITEM_NAN_WIMAX:
			if (tw_nan_wimax_x == 0)
			{
				tw_nan_wimax_x = 1;
        		tw_total++;
			} else if (tw_nan_wimax_x == 1) {
				tw_nan_wimax_x = 0;
        		tw_total--;
			}
			break;
		case ITEM_NAN_ANDSEC:
			if (tw_nan_andsec_x == 0)
			{
				tw_nan_andsec_x = 1;
        		tw_total++;
			} else if (tw_nan_andsec_x == 1) {
				tw_nan_andsec_x = 0;
        		tw_total--;
			}
			break;
		case ITEM_NAN_SDEXT:
			if (tw_nan_sdext_x == 0)
			{
				tw_nan_sdext_x = 1;
        		tw_total++;
			} else if (tw_nan_sdext_x == 1) {
				tw_nan_sdext_x = 0;
        		tw_total--;
			}
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
	ensure_path_mounted("/sdcard");
	int isTrue = 0;
	char* tmp_set = (char*)malloc(25);
	struct stat st;
	switch (tw_setting)
	{
		case ITEM_NAN_SYSTEM:
			strcpy(tmp_set, "[ ] system");
			if (tw_backstore)
			{
				isTrue = tw_nan_system_x;
			} else {
				isTrue = is_true(tw_nan_system_val);
			}
			break;
		case ITEM_NAN_DATA:
			strcpy(tmp_set, "[ ] data");
			if (tw_backstore)
			{
				isTrue = tw_nan_data_x;
			} else {
				isTrue = is_true(tw_nan_data_val);
			}
			break;
		case ITEM_NAN_BOOT:
			strcpy(tmp_set, "[ ] boot");
			if (tw_backstore)
			{
				isTrue = tw_nan_boot_x;
			} else {
				isTrue = is_true(tw_nan_boot_val);
			}
			break;
		case ITEM_NAN_RECOVERY:
			strcpy(tmp_set, "[ ] recovery");
			if (tw_backstore)
			{
				isTrue = tw_nan_recovery_x;
			} else {
				isTrue = is_true(tw_nan_recovery_val);
			}
			break;
		case ITEM_NAN_CACHE:
			strcpy(tmp_set, "[ ] cache");
			if (tw_backstore)
			{
				isTrue = tw_nan_cache_x;
			} else {
				isTrue = is_true(tw_nan_cache_val);
			}
			break;
		case ITEM_NAN_WIMAX:
			strcpy(tmp_set, "[ ] ");
			strcat(tmp_set,wim.mnt);
			if (tw_backstore)
			{
				isTrue = tw_nan_wimax_x;
			} else {
				if (strcmp(wim.mnt,"wimax") == 0 || strcmp(wim.mnt,"efs") == 0)
				{
					isTrue = is_true(tw_nan_wimax_val);
				} else {
					tw_nan_wimax_x = -1;
					isTrue = -1;
				}
			}
			break;
		case ITEM_NAN_ANDSEC:
			strcpy(tmp_set, "[ ] .android_secure");
			if (tw_backstore)
			{
				isTrue = tw_nan_andsec_x;
			} else {
				if (stat(ase.dev,&st) != 0) 
				{
					tw_nan_andsec_x = -1;
					isTrue = -1;
				} else {
					isTrue = is_true(tw_nan_andsec_val);
				}
			}
			break;
		case ITEM_NAN_SDEXT:
			strcpy(tmp_set, "[ ] sd-ext");
			if (tw_backstore)
			{
				isTrue = tw_nan_sdext_x;
			} else {
				if (stat(sde.blk,&st) != 0) 
				{
					tw_nan_sdext_x = -1;
					isTrue = -1;
				} else {
					isTrue = is_true(tw_nan_sdext_val);
				}
			}
			break;
	}
	if (isTrue == 1)
	{
		tmp_set[1] = 'x';
		tw_total++;
	} 
	if (isTrue == -1) 
	{
		tmp_set[0] = ' ';
		tmp_set[1] = ' ';
		tmp_set[2] = ' ';
	}
	return tmp_set;
}

void 
nandroid_back_exe()
{
	ensure_path_mounted(SDCARD_ROOT);
	FILE *fp;
	int isContinue = 1;
	int progTime;
	unsigned long sdSpace;
	unsigned long sdSpaceFinal;
	unsigned long imgSpace;
	char tmpOutput[150];
	char tmpString[15];
	char tmpChar;
	char exe[255];
	char tw_image_base[100];
	char tw_image[255];
    char timestamp[14];
    char tar_arg[5];
	struct stat st;
    struct tm * t;
    time_t seconds;
    time_t nan_ttime;
    time_t nan_ctime;
    seconds = time(0);
    t = localtime(&seconds);
    sprintf(timestamp,"%02d%02d%d%02d%02d",t->tm_mon+1,t->tm_mday,t->tm_year+1900,t->tm_hour,t->tm_min); // get timestamp for nandroid
	if (stat(backup_folder,&st) != 0) {
		if(mkdir(backup_folder,0777) == -1) {
			LOGI("=> Can not create directory: %s\n", backup_folder);
		} else {
			LOGI("=> Created directory: %s\n", backup_folder);
		}
	}
	sprintf(tw_image_base, "%s/%s/", backup_folder, device_id);
	if (stat(tw_image_base,&st) != 0) {
		if(mkdir(tw_image_base,0777) == -1) {
			LOGI("=> Can not create directory: %s\n", tw_image_base);
		} else {
			LOGI("=> Created directory: %s\n", tw_image_base);
		}
	}
	strcat(tw_image_base,timestamp);
	strcat(tw_image_base,"/");
	if(mkdir(tw_image_base,0777) == -1) {
		LOGI("=> Can not create directory: %s\n", tw_image_base);
	} else {
		LOGI("=> Created directory: %s\n", tw_image_base);
	}
	
	fp = __popen("df -k /sdcard| grep sdcard | awk '{ print $4 }'", "r");
	LOGI("=> Checking SDCARD Disk Space.\n");
	while (fgets(tmpString,15,fp) != NULL)
	{
		tmpChar = tmpString[strlen(tmpString)-2];
	}
	__pclose(fp);
	if(tmpChar == '%')
	{
		fp = __popen("df -k /sdcard| grep sdcard | awk '{ print $3 }'", "r");
		LOGI("=> Not the response we were looking for, trying again.\n");
		fgets(tmpString,15,fp);
		__pclose(fp);
	}
	sscanf(tmpString,"%lu",&sdSpace);
	sdSpaceFinal = sdSpace;
	LOGI("=> Space Left on SDCARD: %lu\n\n",sdSpaceFinal);
	
	int div;
	if (is_true(tw_use_compression_val) == 1) {
		strcpy(tar_arg,"czvpf");
		div = 500;
	} else {
		strcpy(tar_arg,"cvpf");
		div = 1000;
	}
	
	ui_print("\nStarting Backup...\n\n");
	nan_ttime = time(0);
	if (is_true(tw_nan_system_val)) {
		ensure_path_mounted("/system");
		fp = __popen("du -sk /system", "r");
		LOGI("=> Checking size of /system.\n");
	    fscanf(fp,"%lu %*s",&imgSpace);
		progTime = imgSpace / div;
		ui_print("[SYSTEM (%d MB)]\n",imgSpace/1024);
		__pclose(fp);
		if (sdSpace > imgSpace)
		{
			nan_ctime = time(0);
			strcpy(tw_image,tw_image_base);
			strcat(tw_image,tw_nan_system);
			sprintf(exe,"cd /%s && tar -%s %s ./*", sys.mnt, tar_arg, tw_image);
			ui_print("...Backing up system partition.\n");
			ui_show_progress(1,progTime);
			fp = __popen(exe, "r");
			while (fscanf(fp,"%s",tmpOutput) != EOF)
			{
				if(is_true(tw_show_spam_val))
				{
					ui_print("%s\n",tmpOutput);
				} else {
					ui_print_overwrite("%s",tmpOutput);
				}
			}
			ui_print_overwrite("....Done.\n");
			__pclose(fp);
			ui_print("...Generating %s md5...\n", sys.mnt);
			makeMD5(tw_image_base,tw_nan_system);
			ui_print("....Done.\n");
			ui_print("...Verifying %s md5...\n", sys.mnt);
			checkMD5(tw_image_base,tw_nan_system);
			ui_print("...Done.\n");
			ui_reset_progress();
			ui_print("Backed up in %d Seconds\n\n", time(0) - nan_ctime);
		} else {
			ui_print("\nNot enough space left on /sdcard... Aborting.\n\n");
			isContinue = 0;
		}
		ensure_path_unmounted("/system");
		sdSpace -= imgSpace;
	}
	if (isContinue)
	{
		if (is_true(tw_nan_data_val)) {
			ensure_path_mounted("/data");
			fp = __popen("du -sk /data", "r");
			LOGI("=> Checking size of /data.\n");
		    fscanf(fp,"%lu %*s",&imgSpace);
			progTime = imgSpace / div;
			ui_print("[DATA (%d MB)]\n",imgSpace/1024);
			__pclose(fp);
			if (sdSpace > imgSpace)
			{
				nan_ctime = time(0);
				strcpy(tw_image,tw_image_base);
				strcat(tw_image,tw_nan_data);
				sprintf(exe,"cd /%s && tar -%s %s ./*", dat.mnt, tar_arg, tw_image);
				ui_print("...Backing up data partition.\n");
				ui_show_progress(1,progTime);
				fp = __popen(exe, "r");
				while (fscanf(fp,"%s",tmpOutput) != EOF)
				{
					if(is_true(tw_show_spam_val))
					{
						ui_print("%s\n",tmpOutput);
					} else {
						ui_print_overwrite("%s",tmpOutput);
					}
				}
				ui_print_overwrite("....Done.\n");
				__pclose(fp);
				ui_print("...Generating %s md5...\n", dat.mnt);
				makeMD5(tw_image_base,tw_nan_data);
				ui_print("....Done.\n");
				ui_print("...Verifying %s md5...\n", dat.mnt);
				checkMD5(tw_image_base,tw_nan_data);
				ui_print("...Done.\n");
				ui_reset_progress();
				ui_print("Backed up in %d Seconds\n\n", time(0) - nan_ctime);
			} else {
				ui_print("\nNot enough space left on /sdcard... Aborting.\n\n");
				isContinue = 0;
			}
			ensure_path_unmounted("/data");
			sdSpace -= imgSpace;
		}
	}
	if (isContinue)
	{
		if (is_true(tw_nan_boot_val)) {
			imgSpace = boo.sze / 1024;
			ui_print("[BOOT (%i MB)]\n",imgSpace/1024);
			if (sdSpace > imgSpace)
			{
				nan_ctime = time(0);
				strcpy(tw_image,tw_image_base);
				strcat(tw_image,tw_nan_boot);
				if (strcmp(boo.fst,"mtd") == 0)
				{
					sprintf(exe,"dump_image %s %s", boo.mnt, tw_image);
				} else {
					sprintf(exe,"dd bs=%s if=%s of=%s", bs_size, boo.dev, tw_image);
				}
				ui_print("...Backing up boot partition.\n");
				ui_show_progress(1,5);
				__system(exe);
				LOGI("=> %s\n", exe);
				ui_print("....Done.\n");
				ui_print("...Generating %s md5...\n", boo.mnt);
				makeMD5(tw_image_base,tw_nan_boot);
				ui_print("....Done.\n");
				ui_print("...Verifying %s md5...\n", boo.mnt);
				checkMD5(tw_image_base,tw_nan_boot);
				ui_print("...Done.\n");
				ui_reset_progress();
				ui_print("Backed up in %d Seconds\n\n", time(0) - nan_ctime);
			} else {
				ui_print("\nNot enough space left on /sdcard... Aborting.\n\n");
				isContinue = 0;
			}
			sdSpace -= imgSpace;
		}
	}
	if (isContinue)
	{
		if (is_true(tw_nan_recovery_val)) {
			imgSpace = rec.sze / 1024;
			ui_print("[RECOVERY (%i MB)]\n",imgSpace/1024);
			if (sdSpace > imgSpace)
			{
				nan_ctime = time(0);
				strcpy(tw_image,tw_image_base);
				strcat(tw_image,tw_nan_recovery);
				if (strcmp(rec.fst,"mtd") == 0)
				{
					sprintf(exe,"dump_image %s %s", rec.mnt, tw_image);
				} else {
					sprintf(exe,"dd bs=%s if=%s of=%s", bs_size, rec.dev, tw_image);
				}
				ui_print("...Backing up recovery partition.\n");
				ui_show_progress(1,5);
				__system(exe);
				LOGI("=> %s\n", exe);
				ui_print("....Done.\n");
				ui_print("...Generating %s md5...\n", rec.mnt);
				makeMD5(tw_image_base,tw_nan_recovery);
				ui_print("....Done.\n");
				ui_print("...Verifying %s md5...\n", rec.mnt);
				checkMD5(tw_image_base,tw_nan_recovery);
				ui_print("...Done.\n");
				ui_reset_progress();
				ui_print("Backed up in %d Seconds\n\n", time(0) - nan_ctime);
			} else {
				ui_print("\nNot enough space left on /sdcard... Aborting.\n\n");
				isContinue = 0;
			}
			sdSpace -= imgSpace;
		}
	}
	if (isContinue)
	{
		if (is_true(tw_nan_cache_val)) {
			ensure_path_mounted("/cache");
			fp = __popen("du -sk /cache", "r");
			LOGI("=> Checking size of /cache.\n");
		    fscanf(fp,"%lu %*s",&imgSpace);
			progTime = imgSpace / div;
			ui_print("[CACHE (%d MB)]\n",imgSpace/1024);
			__pclose(fp);
			if (sdSpace > imgSpace)
			{
				nan_ctime = time(0);
				strcpy(tw_image,tw_image_base);
				strcat(tw_image,tw_nan_cache);
				sprintf(exe,"cd /%s && tar -%s %s ./*", cac.mnt, tar_arg, tw_image);
				ui_print("...Backing up cache partition.\n");
				ui_show_progress(1,progTime);
				fp = __popen(exe, "r");
				while (fscanf(fp,"%s",tmpOutput) != EOF)
				{
					if(is_true(tw_show_spam_val))
					{
						ui_print("%s\n",tmpOutput);
					} else {
						ui_print_overwrite("%s",tmpOutput);
					}
				}
				ui_print_overwrite("....Done.\n");
				__pclose(fp);
				ui_print("...Generating %s md5...\n", cac.mnt);
				makeMD5(tw_image_base,tw_nan_cache);
				ui_print("....Done.\n");
				ui_print("...Verifying %s md5...\n", cac.mnt);
				checkMD5(tw_image_base,tw_nan_cache);
				ui_print("...Done.\n");
				ui_reset_progress();
				ui_print("Backed up in %d Seconds\n\n", time(0) - nan_ctime);
			} else {
				ui_print("\nNot enough space left on /sdcard... Aborting.\n\n");
				isContinue = 0;
			}
			ensure_path_unmounted("/cache");
			sdSpace -= imgSpace;
		}
	}
	if (isContinue)
	{
		if (is_true(tw_nan_wimax_val)) {
			if (strcmp(wim.mnt,"wimax") == 0)
			{
				imgSpace = wim.sze / 1024;
				ui_print("[WIMAX (%d MB)]\n",imgSpace/1024);
			} else {
				__system("mount /efs");
				fp = __popen("du -sk /efs", "r");
				LOGI("=> Checking size of /efs.\n");
			    fscanf(fp,"%lu %*s",&imgSpace);
				ui_print("[EFS (%d MB)]\n",imgSpace/1024);
				__pclose(fp);
			}
			if (sdSpace > imgSpace)
			{
				nan_ctime = time(0);
				strcpy(tw_image,tw_image_base);
				strcat(tw_image,tw_nan_wimax);
				ui_print("...Backing up %s partition.\n", wim.mnt);
				ui_show_progress(1,5);
				if (strcmp(wim.mnt,"efs") == 0)
				{
					sprintf(exe,"cd /%s && tar -%s %s ./*", wim.mnt, tar_arg, tw_image);
				} else {
					if (strcmp(wim.fst,"mtd") == 0)
					{
						sprintf(exe,"dump_image %s %s", wim.mnt, tw_image);
						fp = __popen(exe, "r");
						while (fscanf(fp,"%s",tmpOutput) != EOF)
						{
							if(is_true(tw_show_spam_val))
							{
								ui_print("%s\n",tmpOutput);
							} else {
								ui_print_overwrite("%s",tmpOutput);
							}
						}
						__pclose(fp);
					} else {
						sprintf(exe,"dd bs=%s if=%s of=%s", bs_size, wim.dev, tw_image);
						__system(exe);
						LOGI("=> %s\n", exe);
					}
				}
				ui_print_overwrite("....Done.\n");
				ui_print("...Generating %s md5...\n", wim.mnt);
				makeMD5(tw_image_base,tw_nan_wimax);
				ui_print("....Done.\n");
				ui_print("...Verifying %s md5...\n", wim.mnt);
				checkMD5(tw_image_base,tw_nan_wimax);
				ui_print("...Done.\n");
				ui_reset_progress();
				ui_print("Backed up in %d Seconds\n\n", time(0) - nan_ctime);
			} else {
				ui_print("\nNot enough space left on /sdcard... Aborting.\n\n");
				isContinue = 0;
			}
			if (strcmp(wim.mnt,"efs") == 0)
			{
				__system("umount /efs");
			}
			sdSpace -= imgSpace;
		}
	}
	if (isContinue)
	{
		if (is_true(tw_nan_andsec_val)) {
			ensure_path_mounted(ase.dev);
			fp = __popen("du -sk /sdcard/.android_secure", "r");
			LOGI("=> Checking size of .android_secure.\n");
		    fscanf(fp,"%lu %*s",&imgSpace);
			progTime = imgSpace / div;
			ui_print("[ANDROID_SECURE (%d MB)]\n",imgSpace/1024);
			__pclose(fp);
			if (sdSpace > imgSpace)
			{
				nan_ctime = time(0);
				strcpy(tw_image,tw_image_base);
				strcat(tw_image,tw_nan_andsec);
				sprintf(exe,"cd %s && tar -%s %s ./*", ase.dev, tar_arg, tw_image);
				ui_print("...Backing up .android_secure.\n");
				ui_show_progress(1,progTime);
				fp = __popen(exe, "r");
				while (fscanf(fp,"%s",tmpOutput) != EOF)
				{
					if(is_true(tw_show_spam_val))
					{
						ui_print("%s\n",tmpOutput);
					} else {
						ui_print_overwrite("%s",tmpOutput);
					}
				}
				__pclose(fp);
				ui_print_overwrite("....Done.\n");
				ui_print("...Generating %s md5...\n", ase.mnt);
				makeMD5(tw_image_base,tw_nan_andsec);
				ui_print("....Done.\n");
				ui_print("...Verifying %s md5...\n", ase.mnt);
				checkMD5(tw_image_base,tw_nan_andsec);
				ui_print("...Done.\n");
				ui_reset_progress();
				ui_print("Backed up in %d Seconds\n\n", time(0) - nan_ctime);
			} else {
				ui_print("\nNot enough space left on /sdcard... Aborting.\n\n");
				isContinue = 0;
			}
			sdSpace -= imgSpace;
		}
	}
	if (isContinue)
	{
		if (is_true(tw_nan_sdext_val)) {
			__system("mount /sd-ext");
			fp = __popen("du -sk /sd-ext", "r");
			LOGI("=> Checking size of /sd-ext.\n");
		    fscanf(fp,"%lu %*s",&imgSpace);
			progTime = imgSpace / div;
			ui_print("[SD-EXT (%d MB)]\n",imgSpace/1024);
			__pclose(fp);
			if (sdSpace > imgSpace)
			{
				nan_ctime = time(0);
				strcpy(tw_image,tw_image_base);
				strcat(tw_image,tw_nan_sdext);
				sprintf(exe,"cd %s && tar -%s %s ./*", sde.mnt, tar_arg, tw_image);
				ui_print("...Backing up sd-ext partition.\n");
				ui_show_progress(1,progTime);
				fp = __popen(exe, "r");
				while (fscanf(fp,"%s",tmpOutput) != EOF)
				{
					if(is_true(tw_show_spam_val))
					{
						ui_print("%s\n",tmpOutput);
					} else {
						ui_print_overwrite("%s",tmpOutput);
					}
				}
				__pclose(fp);
				ui_print_overwrite("....Done.\n");
				ui_print("...Generating %s md5...\n", sde.mnt);
				makeMD5(tw_image_base,tw_nan_sdext);
				ui_print("....Done.\n");
				ui_print("...Verifying %s md5...\n", sde.mnt);
				checkMD5(tw_image_base,tw_nan_sdext);
				ui_print("...Done.\n");
				ui_reset_progress();
				ui_print("Backed up in %d Seconds\n\n", time(0) - nan_ctime);
			} else {
				ui_print("\nNot enough space left on /sdcard... Aborting.\n\n");
				isContinue = 0;
			}
			__system("umount /sd-ext");
			sdSpace -= imgSpace;
		}
	}
	fp = __popen("df -k /sdcard| grep sdcard | awk '{ print $4 }'", "r");
	while (fgets(tmpString,15,fp) != NULL)
	{
		tmpChar = tmpString[strlen(tmpString)-2];
	}
	__pclose(fp);
	if(tmpChar == '%')
	{
		fp = __popen("df -k /sdcard| grep sdcard | awk '{ print $3 }'", "r");
		LOGI("=> Not the response we were looking for, trying again.\n");
		fgets(tmpString,15,fp);
		__pclose(fp);
	}
	sscanf(tmpString,"%lu",&sdSpace);
    int totalBackedUp = (int)(sdSpaceFinal - sdSpace) / 1024;
	ui_print("[ %d MB TOTAL BACKED UP TO SDCARD ]\n[ BACKUP COMPLETED IN %d SECONDS ]\n\n", totalBackedUp, time(0) - nan_ttime);
}

void 
nandroid_rest_exe()
{
	ensure_path_mounted(SDCARD_ROOT);
	FILE *fp;
	char tmpBuffer[1024];
	char *tmpOutput;
	int tmpSize;
	int numErrors = 0;
	char exe[255];
	char* tmp_file = (char*)malloc(255);
    time_t nan_ttime;
    time_t nan_ctime;
	ui_print("\nStarting Restore...\n\n");
	nan_ttime = time(0);
	if (tw_nan_system_x == 1) {
		ui_print("...Verifying md5 hash for %s.\n",tw_nan_system);
		ui_show_progress(1,150);
		nan_ctime = time(0);
		if(checkMD5(nan_dir,tw_nan_system))
		{
			ensure_path_mounted("/system");
			strcpy(tmp_file,nan_dir);
			strcat(tmp_file,tw_nan_system);
			ui_print("...Wiping %s.\n",sys.mnt);
			sprintf(exe,"rm -rf /%s/* && rm -rf /%s/.* ", sys.mnt, sys.mnt);
			__system(exe);
			ui_print("....Done.\n");
			sprintf(exe,"cd /%s && tar xzvpf %s", sys.mnt, tmp_file);
			ui_print("...Restoring system partition.\n");
			fp = __popen(exe, "r");
			while (fgets(tmpBuffer,sizeof(tmpBuffer),fp) != NULL)
			{
				tmpBuffer[strlen(tmpBuffer)-1] = '\0';
				tmpOutput = tmpBuffer;
				if(is_true(tw_show_spam_val))
				{
					ui_print("%s\n",tmpOutput);
				} else {
					ui_print_overwrite("%s",tmpOutput);
				}
			}
			__pclose(fp);
			ui_print_overwrite("....Done.\n");
			ui_print("Restored in %d Seconds\n\n", time(0) - nan_ctime);
			ensure_path_unmounted("/system");
		} else {
			ui_print("...Failed md5 check. Aborted.\n\n");
			numErrors++;
		}
		ui_reset_progress();
	}
	if (tw_nan_data_x == 1) {
		ui_print("...Verifying md5 hash for %s.\n",tw_nan_data);
		ui_show_progress(1,150);
		nan_ctime = time(0);
		if(checkMD5(nan_dir,tw_nan_data))
		{
			ensure_path_mounted("/data");
			strcpy(tmp_file,nan_dir);
			strcat(tmp_file,tw_nan_data);
			ui_print("...Wiping %s.\n",dat.mnt);
			sprintf(exe,"rm -rf /%s/* && rm -rf /%s/.* ", dat.mnt, dat.mnt);
			__system(exe);
			ui_print("....Done.\n");
			sprintf(exe,"cd /%s && tar xzvpf %s", dat.mnt, tmp_file);
			ui_print("...Restoring data partition.\n");
			fp = __popen(exe, "r");
			while (fgets(tmpBuffer,sizeof(tmpBuffer),fp) != NULL)
			{
				tmpBuffer[strlen(tmpBuffer)-1] = '\0';
				tmpOutput = tmpBuffer;
				if(is_true(tw_show_spam_val))
				{
					ui_print("%s\n",tmpOutput);
				} else {
					ui_print_overwrite("%s",tmpOutput);
				}
			}
			__pclose(fp);
			ui_print_overwrite("....Done.\n");
			ui_print("Restored in %d Seconds\n\n", time(0) - nan_ctime);
			ensure_path_unmounted("/data");
		} else {
			ui_print("...Failed md5 check. Aborted.\n\n");
			numErrors++;
		}
		ui_reset_progress();
	}
	if (tw_nan_boot_x == 1) {
		ui_print("...Verifying md5 hash for %s.\n",tw_nan_boot);
		ui_show_progress(1,5);
		nan_ctime = time(0);
		if(checkMD5(nan_dir,tw_nan_boot))
		{
			strcpy(tmp_file,nan_dir);
			strcat(tmp_file,tw_nan_boot);
			ui_print("...Double checking, by checking file size.\n");
			sprintf(exe,"ls -l %s",tmp_file);
			fp = __popen(exe, "r");
			fscanf(fp,"%*s %*i %*s %*s %i",&tmpSize);
			if (tmpSize == boo.sze)
			{
				ui_print("....File size matched partition size.\n");
				if (strcmp(boo.fst,"mtd") == 0)
				{
					sprintf(exe,"flash_image %s %s", boo.mnt, tmp_file);
				} else {
					sprintf(exe,"dd bs=%s if=%s of=%s", bs_size, tmp_file, boo.dev);
				}
				LOGI("=> %s\n", exe);
				ui_print("...Restoring boot partition.\n");
				__system(exe);
				ui_print("...Done.\n");
				ui_print("Restored in %d Seconds\n\n", time(0) - nan_ctime);
			} else {
				ui_print("...Failed file size check. Aborted.\n\n");
				numErrors++;
			}
			__pclose(fp);
		} else {
			ui_print("...Failed md5 check. Aborted.\n\n");
			numErrors++;
		}
		ui_reset_progress();
	}
	if (tw_nan_recovery_x == 1) {
		ui_print("...Verifying md5 hash for %s.\n",tw_nan_recovery);
		ui_show_progress(1,5);
		nan_ctime = time(0);
		if(checkMD5(nan_dir,tw_nan_recovery))
		{
			strcpy(tmp_file,nan_dir);
			strcat(tmp_file,tw_nan_recovery);
			ui_print("...Double checking, by checking file size.\n");
			sprintf(exe,"ls -l %s",tmp_file);
			fp = __popen(exe, "r");
			fscanf(fp,"%*s %*i %*s %*s %i",&tmpSize);
			if (tmpSize == rec.sze)
			{
				ui_print("....File size matched partition size.\n");
				if (strcmp(rec.fst,"mtd") == 0)
				{
					sprintf(exe,"flash_image %s %s", rec.mnt, tmp_file);
				} else {
					sprintf(exe,"dd bs=%s if=%s of=%s", bs_size, tmp_file, rec.dev);
				}
				LOGI("=> %s\n", exe);
				ui_print("...Restoring recovery partition.\n");
				__system(exe);
				ui_print("...Done.\n");
				ui_print("Restored in %d Seconds\n\n", time(0) - nan_ctime);
			} else {
				ui_print("...Failed file size check. Aborted.\n\n");
				numErrors++;
			}
			__pclose(fp);
		} else {
			ui_print("...Failed md5 check. Aborted.\n\n");
			numErrors++;
		}
		ui_reset_progress();
	}
	if (tw_nan_cache_x == 1) {
		ui_print("...Verifying md5 hash for %s.\n",tw_nan_cache);
		ui_show_progress(1,100);
		nan_ctime = time(0);
		if(checkMD5(nan_dir,tw_nan_cache))
		{
			ensure_path_mounted("/cache");
			strcpy(tmp_file,nan_dir);
			strcat(tmp_file,tw_nan_cache);
			ui_print("...Wiping %s.\n",cac.mnt);
			sprintf(exe,"rm -rf /%s/* && rm -rf /%s/.* ", cac.mnt, cac.mnt);
			__system(exe);
			ui_print("....Done.\n");
			sprintf(exe,"cd /%s && tar xzvpf %s", cac.mnt, tmp_file);
			ui_print("...Restoring cache partition.\n");
			fp = __popen(exe, "r");
			while (fgets(tmpBuffer,sizeof(tmpBuffer),fp) != NULL)
			{
				tmpBuffer[strlen(tmpBuffer)-1] = '\0';
				tmpOutput = tmpBuffer;
				if(is_true(tw_show_spam_val))
				{
					ui_print("%s\n",tmpOutput);
				} else {
					ui_print_overwrite("%s",tmpOutput);
				}
			}
			__pclose(fp);
			ui_print_overwrite("....Done.\n");
			ui_print("Restored in %d Seconds\n\n", time(0) - nan_ctime);
			ensure_path_unmounted("/cache");
		} else {
			ui_print("...Failed md5 check. Aborted.\n\n");
			numErrors++;
		}
		ui_reset_progress();
	}
	if (tw_nan_wimax_x == 1) {
		ui_print("...Verifying md5 hash for %s.\n",tw_nan_wimax);
		ui_show_progress(1,5);
		nan_ctime = time(0);
		if(checkMD5(nan_dir,tw_nan_wimax))
		{
			strcpy(tmp_file,nan_dir);
			strcat(tmp_file,tw_nan_wimax);
			if (strcmp(wim.mnt,"efs") == 0)
			{
				__system("mount /efs");
				ui_print("...Wiping %s.\n",wim.mnt);
				sprintf(exe,"rm -rf /%s/* && rm -rf /%s/.*", wim.mnt);
				__system(exe);
				ui_print("....Done.\n");
				sprintf(exe,"cd /%s && tar xzvpf %s", wim.mnt, tmp_file);
				ui_print("...Restoring efs partition.\n");
				fp = __popen(exe, "r");
				while (fgets(tmpBuffer,sizeof(tmpBuffer),fp) != NULL)
				{
					tmpBuffer[strlen(tmpBuffer)-1] = '\0';
					tmpOutput = tmpBuffer;
					if(is_true(tw_show_spam_val))
					{
						ui_print("%s\n",tmpOutput);
					} else {
						ui_print_overwrite("%s",tmpOutput);
					}
				}
				__pclose(fp);
				ui_print_overwrite("....Done.\n");
				ui_print("Restored in %d Seconds\n\n", time(0) - nan_ctime);
				__system("umount /efs");
			} else {
				ui_print("...Double checking, by checking file size.\n");
				sprintf(exe,"ls -l %s",tmp_file);
				fp = __popen(exe, "r");
				fscanf(fp,"%*s %*i %*s %*s %i",&tmpSize);
				if (tmpSize == wim.sze)
				{
					ui_print("....File size matched partition size.\n");
					if (strcmp(rec.fst,"mtd") == 0)
					{
						sprintf(exe,"flash_image %s %s", wim.mnt, tmp_file);
					} else {
						sprintf(exe,"dd bs=%s if=%s of=%s", bs_size, tmp_file, wim.dev);
					}
					LOGI("=> %s\n", exe);
					ui_print("...Restoring wimax partition.\n");
					__system(exe);
					ui_print("...Done.\n");
					ui_print("Restored in %d Seconds\n\n", time(0) - nan_ctime);
				} else {
					ui_print("...Failed file size check. Aborted.\n\n");
					numErrors++;
				}
				__pclose(fp);
			}
		} else {
			ui_print("...Failed md5 check. Aborted.\n\n");
			numErrors++;
		}
		ui_reset_progress();
	}
	if (tw_nan_andsec_x == 1) {
		ui_print("...Verifying md5 hash for %s.\n",tw_nan_andsec);
		ui_show_progress(1,25);
		nan_ctime = time(0);
		if(checkMD5(nan_dir,tw_nan_andsec))
		{
			ensure_path_mounted(ase.dev);
			strcpy(tmp_file,nan_dir);
			strcat(tmp_file,tw_nan_andsec);
			ui_print("...Wiping %s.\n",ase.dev);
			sprintf(exe,"rm -rf %s/* && rm -rf %s/.* ", ase.dev, ase.dev);
			__system(exe);
			ui_print("....Done.\n");
			sprintf(exe,"cd %s && tar xzvpf %s", ase.dev, tmp_file);
			ui_print("...Restoring .android-secure.\n");
			fp = __popen(exe, "r");
			while (fgets(tmpBuffer,sizeof(tmpBuffer),fp) != NULL)
			{
				tmpBuffer[strlen(tmpBuffer)-1] = '\0';
				tmpOutput = tmpBuffer;
				if(is_true(tw_show_spam_val))
				{
					ui_print("%s\n",tmpOutput);
				} else {
					ui_print_overwrite("%s",tmpOutput);
				}
			}
			__pclose(fp);
			ui_print_overwrite("....Done.\n");
			ui_print("Restored in %d Seconds\n\n", time(0) - nan_ctime);
		} else {
			ui_print("...Failed md5 check. Aborted.\n\n");
			numErrors++;
		}
		ui_reset_progress();
	}
	if (tw_nan_sdext_x == 1) {
		ui_print("...Verifying md5 hash for %s.\n",tw_nan_sdext);
		ui_show_progress(1,25);
		nan_ctime = time(0);
		if(checkMD5(nan_dir,tw_nan_sdext))
		{
			__system("mount /sd-ext");
			strcpy(tmp_file,nan_dir);
			strcat(tmp_file,tw_nan_sdext);
			ui_print("...Wiping %s.\n",sde.mnt);
			sprintf(exe,"rm -rf /%s/* && rm -rf /%s/.* ", sde.mnt, sde.mnt);
			__system(exe);
			ui_print("....Done.\n");
			sprintf(exe,"cd /%s && tar xzvpf %s", sde.mnt, tmp_file);
			ui_print("...Restoring sd-ext partition.\n");
			fp = __popen(exe, "r");
			while (fgets(tmpBuffer,sizeof(tmpBuffer),fp) != NULL)
			{
				tmpBuffer[strlen(tmpBuffer)-1] = '\0';
				tmpOutput = tmpBuffer;
				if(is_true(tw_show_spam_val))
				{
					ui_print("%s\n",tmpOutput);
				} else {
					ui_print_overwrite("%s",tmpOutput);
				}
			}
			__pclose(fp);
			ui_print_overwrite("....Done.\n");
			ui_print("Restored in %d Seconds\n\n", time(0) - nan_ctime);
			__system("umount /sd-ext");
		} else {
			ui_print("...Failed md5 check. Aborted.\n\n");
			numErrors++;
		}
		ui_reset_progress();
	}
	if (numErrors > 0)
	{
		ui_print("[ %d ERROR(S), PLEASE CHECK LOGS ]\n", numErrors);
	}
	ui_print("[ RESTORE COMPLETED IN %d SECONDS ]\n\n", time(0) - nan_ttime);
	__system("sync");
	free(tmp_file);
}

static int compare_string(const void* a, const void* b) {
    return strcmp(*(const char**)a, *(const char**)b);
}

void choose_backup_folder() 
{
    ensure_path_mounted(SDCARD_ROOT);

	struct stat st;
    char tw_dir[255];
    sprintf(tw_dir, "%s/%s/", backup_folder, device_id);
    if (stat(backup_folder,&st) != 0)
    {
    	if (mkdir(backup_folder, 0777) != 0)
    	{
    		LOGI("=> Can not create %s\n", backup_folder);
    	} else {
    		LOGI("=> Created folder %s\n", backup_folder);
    	}
    }
    if (stat(tw_dir,&st) != 0)
    {
    	if (mkdir(tw_dir, 0777) != 0)
    	{
    		LOGI("=> Can not create %s\n", tw_dir);
    	} else {
    		LOGI("=> Created folder %s\n", tw_dir);
    	}
    }
    
    const char* MENU_HEADERS[] = { "Choose a Nandroid Folder:",
    							   tw_dir,
                                   NULL };
    
    char** headers = prepend_title(MENU_HEADERS);
    
    DIR* d;
    struct dirent* de;
    d = opendir(tw_dir);
    if (d == NULL) {
        LOGE("error opening %s: %s\n", tw_dir, strerror(errno));
        ensure_path_unmounted(SDCARD_ROOT);
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
    for (;;) 
    {
        chosen_item = get_menu_selection(headers, zips, 1, chosen_item);

        char* item = zips[chosen_item];
        int item_len = strlen(item);

        if (chosen_item == 0) {
            break;
        } else if (item[item_len-1] == '/') {
            strcpy(nan_dir, tw_dir);
            strcat(nan_dir, item);
            set_restore_files();
            nan_restore_menu(0);
            break;
        }
    }

    int i;
    for (i = 0; i < z_size; ++i) free(zips[i]);
    free(zips);

    ensure_path_unmounted(SDCARD_ROOT);
    dec_menu_loc();
}

int makeMD5(char *imgDir, const char *imgFile)
{
	int bool = 0;
	if (ensure_path_mounted("/sdcard") != 0) {
		LOGI("=> Can not mount sdcard.\n");
	} else {
		char tmpString[255];
		sprintf(tmpString,"cd %s && md5sum %s > %s.md5",imgDir,imgFile,imgFile);
		__system(tmpString);
		bool = 1;
	}
	return bool;
}

int checkMD5(char *imgDir, const char *imgFile)
{
	int bool = 0;
	if (ensure_path_mounted("/sdcard") != 0) {
		LOGI("=> Can not mount sdcard.\n");
	} else {
		FILE *fp;
		char tmpString[255];
		char tmpAnswer[10];
		sprintf(tmpString,"cd %s && md5sum -c %s.md5",imgDir,imgFile);
		fp = __popen(tmpString, "r");
		if (fp == NULL)
		{
			LOGI("=> Can not open pipe.\n");
		} else {
			fgets(tmpString, 255, fp);
			sscanf(tmpString,"%*s %s",tmpAnswer);
			if (strcmp(tmpAnswer,"OK") == 0)
			{
				bool = 1;
				ui_print("....MD5 Check: %s\n", tmpAnswer);
			} else {
				ui_print("....MD5 Error: %s\n", tmpString);
			}
		}
		__pclose(fp);
	}
	return bool;
}
