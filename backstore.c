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
#include <sys/reboot.h>

#include "backstore.h"
#include "ddftw.h"
#include "extra-functions.h"
#include "roots.h"
#include "format.h"

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
	strcpy(tmp_set, "[ ] Compress Backup (slow but saves space)");
	if (DataManager_GetIntValue(TW_USE_COMPRESSION_VAR) == 1) {
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
	for (;;) {
		int chosen_item = get_menu_selection(headers, nan_b_items, 0, pIdx); // get key presses
		pIdx = chosen_item; // remember last selection location
		switch (chosen_item) {
			case ITEM_NAN_BACKUP:
				if (tw_total > 0) {
					nandroid_back_exe();
					dec_menu_loc();
					return;
				}
				break;
			case ITEM_NAN_SYSTEM:
            	if (DataManager_GetIntValue(TW_NANDROID_SYSTEM_VAR)) {
            		DataManager_SetIntValue(TW_NANDROID_SYSTEM_VAR, 0); // toggle's value
            		tw_total--; // keeps count of how many selected
            	} else {
            		DataManager_SetIntValue(TW_NANDROID_SYSTEM_VAR, 1);
            		tw_total++;
            	}
                break;
			case ITEM_NAN_DATA:
            	if (DataManager_GetIntValue(TW_NANDROID_DATA_VAR)) {
            		DataManager_SetIntValue(TW_NANDROID_DATA_VAR, 0);
            		tw_total--;
            	} else {
            		DataManager_SetIntValue(TW_NANDROID_DATA_VAR, 1);
            		tw_total++;
            	}
				break;
			case ITEM_NAN_BOOT:
            	if (DataManager_GetIntValue(TW_NANDROID_BOOT_VAR)) {
            		DataManager_SetIntValue(TW_NANDROID_BOOT_VAR, 0);
            		tw_total--;
            	} else {
            		DataManager_SetIntValue(TW_NANDROID_BOOT_VAR, 1);
            		tw_total++;
            	}
				break;
			case ITEM_NAN_RECOVERY:
            	if (DataManager_GetIntValue(TW_NANDROID_RECOVERY_VAR)) {
            		DataManager_SetIntValue(TW_NANDROID_RECOVERY_VAR, 0);
            		tw_total--;
            	} else {
            		DataManager_SetIntValue(TW_NANDROID_RECOVERY_VAR, 1);
            		tw_total++;
            	}
				break;
			case ITEM_NAN_CACHE:
            	if (DataManager_GetIntValue(TW_NANDROID_CACHE_VAR)) {
            		DataManager_SetIntValue(TW_NANDROID_CACHE_VAR, 0);
            		tw_total--;
            	} else {
            		DataManager_SetIntValue(TW_NANDROID_CACHE_VAR, 1);
            		tw_total++;
            	}
				break;
			case ITEM_NAN_WIMAX:
				if (tw_nan_wimax_x != -1) {
	            	if (DataManager_GetIntValue(TW_NANDROID_WIMAX_VAR)) {
	            		DataManager_SetIntValue(TW_NANDROID_WIMAX_VAR, 0);
	            		tw_total--;
	            	} else {
	            		DataManager_SetIntValue(TW_NANDROID_WIMAX_VAR, 1);
	            		tw_total++;
	            	}
				}
				break;
			case ITEM_NAN_ANDSEC:
				if (tw_nan_andsec_x != -1) {
	            	if (DataManager_GetIntValue(TW_NANDROID_ANDSEC_VAR)) {
	            		DataManager_SetIntValue(TW_NANDROID_ANDSEC_VAR, 0);
	            		tw_total--;
	            	} else {
	            		DataManager_SetIntValue(TW_NANDROID_ANDSEC_VAR, 1);
	            		tw_total++;
	            	}
				}
				break;
			case ITEM_NAN_SDEXT:
				if (tw_nan_sdext_x != -1) {
	            	if (DataManager_GetIntValue(TW_NANDROID_SDEXT_VAR)) {
	            		DataManager_SetIntValue(TW_NANDROID_SDEXT_VAR, 0);
	            		tw_total--;
	            	} else {
	            		DataManager_SetIntValue(TW_NANDROID_SDEXT_VAR, 1);
	            		tw_total++;
	           		}
				}
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
	FILE *rfFp;
	char rfCommand[255];
	char rfOutput[20];
	tw_nan_system_x = -1; // set restore files as not there (default)
	tw_nan_data_x = -1;
	tw_nan_cache_x = -1;
	tw_nan_recovery_x = -1;
	tw_nan_wimax_x = -1;
	tw_nan_boot_x = -1;
	tw_nan_andsec_x = -1;
	tw_nan_sdext_x = -1;
	sprintf(rfCommand,"cd %s && ls -l *.win | awk '{ print $9 }'",nan_dir); // form scan for *.win files command
	rfFp = __popen(rfCommand, "r"); // open pipe with scan command
	while (fscanf(rfFp,"%s",rfOutput) == 1) { // while there is output
		if (strcmp(rfOutput,"system.yaffs2.win") == 0 ||
			strcmp(rfOutput,"system.ext2.win") == 0 ||
			strcmp(rfOutput,"system.ext3.win") == 0 ||
			strcmp(rfOutput,"system.ext4.win") == 0) { // if output matches any of these filenames
			strcpy(sys.fnm,rfOutput); // copy the filename and store in struct .fnm
			tw_nan_system_x = 1; // set restore option for system as visible
			tw_total++; // add to restore count
		}
		if (strcmp(rfOutput,"data.yaffs2.win") == 0 ||
			strcmp(rfOutput,"data.ext2.win") == 0 ||
			strcmp(rfOutput,"data.ext3.win") == 0 ||
			strcmp(rfOutput,"data.ext4.win") == 0) {
			strcpy(dat.fnm,rfOutput);
			tw_nan_data_x = 1;
			tw_total++;
		}
		if (strcmp(rfOutput,"cache.yaffs2.win") == 0 ||
			strcmp(rfOutput,"cache.ext2.win") == 0 ||
			strcmp(rfOutput,"cache.ext3.win") == 0 ||
			strcmp(rfOutput,"cache.ext4.win") == 0) {
			strcpy(cac.fnm,rfOutput);
			tw_nan_cache_x = 1;
			tw_total++;
		}
		if (strcmp(rfOutput,"sd-ext.ext2.win") == 0 ||
			strcmp(rfOutput,"sd-ext.ext3.win") == 0 ||
			strcmp(rfOutput,"sd-ext.ext4.win") == 0) {
			strcpy(sde.fnm,rfOutput);
			tw_nan_sdext_x = 1;
			tw_total++;
		}
		if (strcmp(rfOutput,"and-sec.vfat.win") == 0) {
			strcpy(ase.fnm,rfOutput);
			tw_nan_andsec_x = 1;
			tw_total++;
		}
		if (strcmp(rfOutput,"boot.mtd.win") == 0 ||
			strcmp(rfOutput,"boot.emmc.win") == 0) {
			strcpy(boo.fnm,rfOutput);
			tw_nan_boot_x = 1;
			tw_total++;
		}
		if (strcmp(rfOutput,"recovery.mtd.win") == 0 ||
			strcmp(rfOutput,"recovery.emmc.win") == 0) {
			strcpy(rec.fnm,rfOutput);
			tw_nan_recovery_x = 1;
			tw_total++;
		}
		if (strcmp(wim.mnt,"wimax") == 0 || strcmp(wim.mnt,"efs") == 0) {
			if (strcmp(rfOutput,"wimax.mtd.win") == 0 ||
				strcmp(rfOutput,"wimax.emmc.win") == 0) {
				strcpy(wim.fnm,rfOutput);
				tw_nan_wimax_x = 1;
				tw_total++;
			}
			if (strcmp(rfOutput,"efs.yaffs2.win") == 0) {
				strcpy(wim.fnm,rfOutput);
				tw_nan_wimax_x = 1;
				tw_total++;
			}
		}
	}
	__pclose(rfFp);
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
	for (;;) {
		int chosen_item = get_menu_selection(headers, nan_r_items, 0, pIdx);
		pIdx = chosen_item; // remember last selection location
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
			if (tw_nan_system_x == 0) {
				tw_nan_system_x = 1;
        		tw_total++;
			} else if (tw_nan_system_x == 1) {
				tw_nan_system_x = 0;
        		tw_total--;
			}
            break;
		case ITEM_NAN_DATA:
			if (tw_nan_data_x == 0) {
				tw_nan_data_x = 1;
        		tw_total++;
			} else if (tw_nan_data_x == 1) {
				tw_nan_data_x = 0;
        		tw_total--;
			}
			break;
		case ITEM_NAN_BOOT:
			if (tw_nan_boot_x == 0) {
				tw_nan_boot_x = 1;
        		tw_total++;
			} else if (tw_nan_boot_x == 1) {
				tw_nan_boot_x = 0;
        		tw_total--;
			}
			break;
		case ITEM_NAN_RECOVERY:
			if (tw_nan_recovery_x == 0) {
				tw_nan_recovery_x = 1;
        		tw_total++;
			} else if (tw_nan_recovery_x == 1) {
				tw_nan_recovery_x = 0;
        		tw_total--;
			}
			break;
		case ITEM_NAN_CACHE:
			if (tw_nan_cache_x == 0) {
				tw_nan_cache_x = 1;
        		tw_total++;
			} else if (tw_nan_cache_x == 1) {
				tw_nan_cache_x = 0;
        		tw_total--;
			}
			break;
		case ITEM_NAN_WIMAX:
			if (tw_nan_wimax_x == 0) {
				tw_nan_wimax_x = 1;
        		tw_total++;
			} else if (tw_nan_wimax_x == 1) {
				tw_nan_wimax_x = 0;
        		tw_total--;
			}
			break;
		case ITEM_NAN_ANDSEC:
			if (tw_nan_andsec_x == 0) {
				tw_nan_andsec_x = 1;
        		tw_total++;
			} else if (tw_nan_andsec_x == 1) {
				tw_nan_andsec_x = 0;
        		tw_total--;
			}
			break;
		case ITEM_NAN_SDEXT:
			if (tw_nan_sdext_x == 0) {
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
	switch (tw_setting) {
		case ITEM_NAN_SYSTEM:
			strcpy(tmp_set, "[ ] system");
			if (tw_backstore) {
				isTrue = tw_nan_system_x;
			} else {
				isTrue = DataManager_GetIntValue(TW_NANDROID_SYSTEM_VAR);
			}
			break;
		case ITEM_NAN_DATA:
			strcpy(tmp_set, "[ ] data");
			if (tw_backstore) {
				isTrue = tw_nan_data_x;
			} else {
				isTrue = DataManager_GetIntValue(TW_NANDROID_DATA_VAR);
			}
			break;
		case ITEM_NAN_BOOT:
			strcpy(tmp_set, "[ ] boot");
			if (tw_backstore) {
				isTrue = tw_nan_boot_x;
			} else {
				isTrue = DataManager_GetIntValue(TW_NANDROID_BOOT_VAR);
			}
			break;
		case ITEM_NAN_RECOVERY:
			strcpy(tmp_set, "[ ] recovery");
			if (tw_backstore) {
				isTrue = tw_nan_recovery_x;
			} else {
				isTrue = DataManager_GetIntValue(TW_NANDROID_RECOVERY_VAR);
			}
			break;
		case ITEM_NAN_CACHE:
			strcpy(tmp_set, "[ ] cache");
			if (tw_backstore) {
				isTrue = tw_nan_cache_x;
			} else {
				isTrue = DataManager_GetIntValue(TW_NANDROID_CACHE_VAR);
			}
			break;
		case ITEM_NAN_WIMAX:
			strcpy(tmp_set, "[ ] ");
			if (strcmp(wim.mnt,"efs") == 0) {
				strcat(tmp_set,"efs");
			} else {
				strcat(tmp_set,"wimax");
			}
			if (tw_backstore) {
				isTrue = tw_nan_wimax_x;
			} else {
				if (strcmp(wim.mnt,"wimax") == 0 || strcmp(wim.mnt,"efs") == 0) {
					isTrue = DataManager_GetIntValue(TW_NANDROID_WIMAX_VAR);
				} else {
					tw_nan_wimax_x = -1;
					isTrue = -1;
				}
			}
			break;
		case ITEM_NAN_ANDSEC:
			strcpy(tmp_set, "[ ] .android_secure");
			if (tw_backstore) {
				isTrue = tw_nan_andsec_x;
			} else {
				if (stat(ase.dev,&st) != 0) {
					tw_nan_andsec_x = -1;
					isTrue = -1;
				} else {
					isTrue = DataManager_GetIntValue(TW_NANDROID_ANDSEC_VAR);
				}
			}
			break;
		case ITEM_NAN_SDEXT:
			strcpy(tmp_set, "[ ] sd-ext");
			if (tw_backstore) {
				isTrue = tw_nan_sdext_x;
			} else {
				if (stat(sde.blk,&st) != 0) {
					tw_nan_sdext_x = -1;
					isTrue = -1;
				} else {
					isTrue = DataManager_GetIntValue(TW_NANDROID_SDEXT_VAR);
				}
			}
			break;
	}
	if (isTrue == 1) {
		tmp_set[1] = 'x';
		tw_total++;
	} 
	if (isTrue == -1) {
		tmp_set[0] = ' ';
		tmp_set[1] = ' ';
		tmp_set[2] = ' ';
	}
	return tmp_set;
}

/* Made my own mount, because aosp api "ensure_path_mounted" relies on recovery.fstab
** and we needed one that can mount based on what the file system really is (from blkid), 
** and not what's in fstab or recovery.fstab
*/
int tw_mount(struct dInfo mMnt)
{
	if (strcmp(mMnt.mnt,"system") == 0 || strcmp(mMnt.mnt,"data") == 0 ||
		strcmp(mMnt.mnt,"cache") == 0 || strcmp(mMnt.mnt,"sd-ext") == 0 || 
		strcmp(mMnt.mnt,"efs") == 0) { // if any of the mount points match these
		FILE *fp;
		char mCommand[255];
		char mOutput[50];
		LOGI("=> Checking if /%s is mounted.\n",mMnt.mnt); // check it mounted
		sprintf(mCommand,"cat /proc/mounts | grep %s | awk '{ print $1 }'",mMnt.blk); // by checking for block in proc mounts
		fp = __popen(mCommand, "r"); // run above command
		if (fscanf(fp,"%s",mOutput) != 1) { // if we get a match
			__pclose(fp);
			LOGI("=> /%s is not mounted. Mounting...\n",mMnt.mnt);
			sprintf(mCommand,"mount -t %s %s /%s",mMnt.fst,mMnt.blk,mMnt.mnt); // mount using filesystem stored in struct mMnt.fst
			fp = __popen(mCommand, "r");
			fgets(mOutput,sizeof(mOutput),fp); // get output
			__pclose(fp);
			if (mOutput[0] == 'm') { // if output starts with m, it's an error
				ui_print("-- Error: %s",mOutput);
				return 1;
			} else {
				LOGI("=> Mounted /%s.\n",mMnt.mnt); // output should be nothing, so it's succesful
			}
		} else {
			__pclose(fp);
			LOGI("=> /%s is already mounted.\n",mMnt.mnt);
		}
	}
	return 0;
}

int tw_unmount(struct dInfo uMnt)
{
	if (strcmp(uMnt.mnt,"system") == 0 || strcmp(uMnt.mnt,"data") == 0 ||
		strcmp(uMnt.mnt,"cache") == 0 || strcmp(uMnt.mnt,"sd-ext") == 0 || 
		strcmp(uMnt.mnt,"efs") == 0) {
		FILE *fp;
		char uCommand[255];
		char uOutput[50];
		char exe[50];
		LOGI("=> Checking if /%s is mounted.\n",uMnt.mnt);
		sprintf(uCommand,"cat /proc/mounts | grep %s | awk '{ print $1 }'",uMnt.blk);
		fp = __popen(uCommand, "r");
		if (fscanf(fp,"%s",uOutput) == 1) {
			__pclose(fp);
			sprintf(exe,"umount /%s",uMnt.mnt);
			fp = __popen(exe, "r");
			fgets(uOutput,sizeof(uOutput),fp);
			__pclose(fp);
			if (uOutput[0] == 'u') {
				ui_print("-- Error: %s",uOutput);
				return 1;
			} else {
				LOGI("=> Unmounted /%s\n",uMnt.mnt);
			}
		} else {
			__pclose(fp);
			LOGI("=> /%s is not mounted.\n\n",uMnt.mnt);
		}
	}
	return 0;
}

/* New backup function
** Condensed all partitions into one function
*/
int tw_backup(struct dInfo bMnt, char *bDir)
{
	if (ensure_path_mounted(SDCARD_ROOT) != 0) {
		ui_print("-- Could not mount: %s.\n-- Aborting.\n",SDCARD_ROOT);
		return 1;
	}
	int bDiv;
	char bTarArg[10];
	if (DataManager_GetIntValue(TW_USE_COMPRESSION_VAR)) { // set compression or not
		strcpy(bTarArg,"-czvf");
		bDiv = 512;
	} else {
		strcpy(bTarArg,"-cvf");
		bDiv = 2048;
	}
	FILE *bFp;
	int bPartSize;
	char *bImage = malloc(sizeof(char)*50);
	char *bMount = malloc(sizeof(char)*50);
	char *bCommand = malloc(sizeof(char)*255);
	if (strcmp(bMnt.mnt,"system") == 0 || strcmp(bMnt.mnt,"data") == 0 || 
			strcmp(bMnt.mnt,"cache") == 0 || strcmp(bMnt.mnt,"sd-ext") == 0 || 
			strcmp(bMnt.mnt,"efs") == 0 || strcmp(bMnt.mnt,".android_secure") == 0) { // detect mountable partitions
		if (strcmp(bMnt.mnt,".android_secure") == 0) { // if it's android secure, add sdcard to prefix
			strcpy(bMount,"/sdcard/");
			strcat(bMount,bMnt.mnt);
			sprintf(bImage,"and-sec.%s.win",bMnt.fst); // set image name based on filesystem, should always be vfat for android_secure
		} else {
			strcpy(bMount,"/");
			strcat(bMount,bMnt.mnt);
			sprintf(bImage,"%s.%s.win",bMnt.mnt,bMnt.fst); // anything else that is mountable, will be partition.filesystem.win
			ui_print("\n-- Mounting %s, please wait...\n",bMount);
			if (tw_mount(bMnt)) {
				ui_print("-- Could not mount: %s\n-- Aborting.\n",bMount);
				free(bCommand);
				free(bMount);
				free(bImage);
				return 1;
			}
			ui_print("-- Done.\n\n",bMount);
		}
		sprintf(bCommand,"du -sk %s | awk '{ print $1 }'",bMount); // check for partition/folder size
		bFp = __popen(bCommand, "r");
		fscanf(bFp,"%d",&bPartSize);
		__pclose(bFp);
		sprintf(bCommand,"cd %s && tar %s %s%s ./*",bMount,bTarArg,bDir,bImage); // form backup command
	} else {
		strcpy(bMount,bMnt.mnt);
		bPartSize = bMnt.sze / 1024;
		sprintf(bImage,"%s.%s.win",bMnt.mnt,bMnt.fst); // non-mountable partitions such as boot/wimax/recovery
		if (strcmp(bMnt.fst,"mtd") == 0) {
			sprintf(bCommand,"dump_image %s %s%s",bMnt.mnt,bDir,bImage); // if it's mtd, we use dump image
		} else if (strcmp(bMnt.fst,"emmc") == 0) {
			sprintf(bCommand,"dd bs=%s if=%s of=%s%s",bs_size,bMnt.blk,bDir,bImage); // if it's emmc, use dd
		}
		ui_print("\n");
	}
	LOGI("=> Filename: %s\n",bImage);
	LOGI("=> Size of %s is %d KB.\n\n",bMount,bPartSize);
	int i;
	char bUppr[20];
	strcpy(bUppr,bMnt.mnt);
	for (i = 0; i < strlen(bUppr); i++) { // make uppercase of mount name
		bUppr[i] = toupper(bUppr[i]);
	}
	ui_print("[%s (%d MB)]\n",bUppr,bPartSize/1024); // show size in MB
	int bProgTime;
	time_t bStart, bStop;
	char bOutput[512];
	if (sdSpace > bPartSize) { // Do we have enough space on sdcard?
		time(&bStart); // start timer
		bProgTime = bPartSize / bDiv; // not very accurate but better than nothing progress time for progress bar
		ui_show_progress(1,bProgTime);
		ui_print("...Backing up %s partition.\n",bMount);
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
		ui_print_overwrite("....Done.\n");
		__pclose(bFp);
		int pFileSize;
		ui_print("...Double checking backup file size.\n");
		sprintf(bCommand,"ls -l %s%s | awk '{ print $5 }'",bDir,bImage); // double checking to make sure we backed up something
		bFp = __popen(bCommand, "r");
		fscanf(bFp,"%d",&pFileSize);
		__pclose(bFp);
		ui_print("....File size: %d bytes.\n",pFileSize); // file size
		if (pFileSize > 0) { // larger than 0 bytes?
			if (strcmp(bMnt.fst,"mtd") == 0 || strcmp(bMnt.fst,"emmc") == 0) { // if it's an unmountable partition, we can make sure
				LOGI("=> Expected size: %d Got: %d\n",bMnt.sze,pFileSize); // partition size matches file image size (they should match!)
				if (pFileSize != bMnt.sze) {
					ui_print("....File size is incorrect. Aborting...\n\n"); // they dont :(
					free(bCommand);
					free(bMount);
					free(bImage);
					return 1;
				} else {
					ui_print("....File size matches partition size.\n"); // they do, yay!
				}
			}
			ui_print("...Generating %s md5...\n", bMnt.mnt);
			makeMD5(bDir,bImage); // make md5 file
			ui_print("....Done.\n");
			ui_print("...Verifying %s md5...\n", bMnt.mnt);
			checkMD5(bDir,bImage); // test the md5 we just made, just in case
			ui_print("....Done.\n");
			time(&bStop); // stop timer
			ui_reset_progress(); // stop progress bar
			ui_print("[%s DONE (%d SECONDS)]\n\n",bUppr,(int)difftime(bStop,bStart)); // done, finally. How long did it take?
			tw_unmount(bMnt); // unmount partition we just restored to (if it's not a mountable partition, it will just bypass)
			sdSpace -= bPartSize; // minus from sd space number (not accurate but, it's more than the real count so it will suffice)
		} else {
			ui_print("...File size is zero bytes. Aborting...\n\n"); // oh noes! file size is 0, abort! abort!
			tw_unmount(bMnt);
			free(bCommand);
			free(bMount);
			free(bImage);
			return 1;
		}
	} else {
		ui_print("...Not enough space on /sdcard. Aborting.\n"); // oh noes! no space left on sdcard, abort! abort!
		tw_unmount(bMnt);
		free(bCommand);
		free(bMount);
		free(bImage);
		return 1;
	}
	free(bCommand);
	free(bMount);
	free(bImage);
	return 0;
}

int
nandroid_back_exe()
{
	if (ensure_path_mounted(SDCARD_ROOT) != 0) {
		ui_print("-- Could not mount: %s.\n-- Aborting.\n",SDCARD_ROOT);
		return 1;
	}
    struct tm *t;
    char timestamp[15];
	char tw_image_dir[255];
	char exe[255];
	time_t start, stop;
	time_t seconds;
	seconds = time(0);
    t = localtime(&seconds);
    sprintf(timestamp,"%02d%02d%d%02d%02d%02d",t->tm_mon+1,t->tm_mday,t->tm_year+1900,t->tm_hour,t->tm_min,t->tm_sec); // make time stamp
	sprintf(tw_image_dir,"%s/%s/%s/",backup_folder,device_id,timestamp); // for backup folder
	sprintf(exe,"mkdir -p %s",tw_image_dir); // make the folder with timestamp
	if (__system(exe) != 0) {
		ui_print("-- Could not create: %s.\n-- Aborting.",tw_image_dir);
		return 1;
	} else {
		LOGI("=> Created folder: %s\n",tw_image_dir);
	}
	FILE *fp;
	char pOutput[25];
	int sdSpaceFinal;
	LOGI("=> Checking space on %s.\n",SDCARD_ROOT);
	fp = __popen("df -k /sdcard | grep sdcard | awk '{ print $4 \" \" $3 }'", "r"); // how much space left on sdcard?
	fgets(pOutput,25,fp);
	__pclose(fp);
	if(pOutput[2] == '%') { // oh o, crespo devices report diskspace on the 3rd argument.
		if (sscanf(pOutput,"%*s %d",&sdSpaceFinal) != 1) { // is it a number?
			ui_print("-- Could not determine free space on %s.",SDCARD_ROOT); // oh noes! Can't find sdcard's free space.
			return 1;
		}
	} else {
		if (sscanf(pOutput,"%d %*s",&sdSpaceFinal) != 1) { // is it a number?
			ui_print("-- Could not determine free space on %s.",SDCARD_ROOT); // oh noes! Can't find sdcard's free space.
			return 1;
		}
	}
	LOGI("=> %s",pOutput);
	sdSpace = sdSpaceFinal; // set starting and running count of sd space
	LOGI("=> /sdcard has %d MB free.\n",sdSpace/1024);
	time(&start);
	ui_print("\n[BACKUP STARTED]\n\n");
	ui_print("-- Verifying filesystems, please wait...\n");
	verifyFst();
	ui_print("-- Updating fstab.\n");
	createFstab();
	ui_print("-- Done.\n");
	// SYSTEM
	if (DataManager_GetIntValue(TW_NANDROID_SYSTEM_VAR)) { // was system backup enabled?
		if (tw_backup(sys,tw_image_dir) == 1) { // did the backup process return an error ? 0 = no error
			ui_print("-- Error occured, check recovery.log. Aborting.\n"); //oh noes! abort abort!
			return 1;
		}
	}
	// DATA
	if (DataManager_GetIntValue(TW_NANDROID_DATA_VAR)) {
		if (tw_backup(dat,tw_image_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
			return 1;
		}
	}
	// BOOT
	if (DataManager_GetIntValue(TW_NANDROID_BOOT_VAR)) {
		if (tw_backup(boo,tw_image_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
			return 1;
		}
	}
	// RECOVERY
	if (DataManager_GetIntValue(TW_NANDROID_RECOVERY_VAR)) {
		if (tw_backup(rec,tw_image_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
			return 1;
		}
	}
	// CACHE
	if (DataManager_GetIntValue(TW_NANDROID_CACHE_VAR)) {
		if (tw_backup(cac,tw_image_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
			return 1;
		}
	}
	// WIMAX
	if (DataManager_GetIntValue(TW_NANDROID_WIMAX_VAR)) {
		if (tw_backup(wim,tw_image_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
			return 1;
		}
	}
	// ANDROID-SECURE
	if (DataManager_GetIntValue(TW_NANDROID_ANDSEC_VAR)) {
		if (tw_backup(ase,tw_image_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
			return 1;
		}
	}
	// SD-EXT
	if (DataManager_GetIntValue(TW_NANDROID_SDEXT_VAR)) {
		if (tw_backup(sde,tw_image_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
			return 1;
		}
	}
	LOGI("=> Checking /sdcard space again.\n\n");
	fp = __popen("df -k /sdcard| grep sdcard | awk '{ print $4 \" \" $3 }'", "r");
	fgets(pOutput,25,fp);
	__pclose(fp);
	if(pOutput[2] == '%') {
		if (sscanf(pOutput,"%*s %d",&sdSpace) != 1) { // is it a number?
			ui_print("-- Could not determine free space on %s.\n",SDCARD_ROOT); // oh noes! Can't find sdcard's free space.
			return 1;
		}
	} else {
		if (sscanf(pOutput,"%d %*s",&sdSpace) != 1) { // is it a number?
			ui_print("-- Could not determine free space on %s.\n",SDCARD_ROOT); // oh noes! Can't find sdcard's free space.
			return 1;
		}
	}
	time(&stop);
	ui_print("[%d MB TOTAL BACKED UP TO SDCARD]\n",(int)(sdSpaceFinal - sdSpace) / 1024);
	ui_print("[BACKUP COMPLETED IN %d SECONDS]\n\n",(int)difftime(stop, start)); // the end
	return 0;
}

int tw_restore(struct dInfo rMnt, char *rDir)
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
	for (i = 0; i < strlen(rUppr); i++) {
		rUppr[i] = toupper(rUppr[i]);
	}
	ui_print("[%s]\n",rUppr);
	ui_show_progress(1,150);
	time(&rStart);
	ui_print("...Verifying md5 hash for %s.\n",rMnt.mnt);
	if(checkMD5(rDir,rMnt.fnm) == 0) // verify md5, check if no error; 0 = no error.
	{
		strcpy(rFilename,rDir);
		strcat(rFilename,rMnt.fnm);
		sprintf(rCommand,"ls -l %s | awk -F'.' '{ print $2 }'",rFilename); // let's get the filesystem type from filename
		reFp = __popen(rCommand, "r");
		LOGI("=> Filename is: %s\n",rMnt.fnm);
		while (fscanf(reFp,"%s",rFilesystem) == 1) { // if we get a match, store filesystem type
			LOGI("=> Filesystem is: %s\n",rFilesystem); // show it off to the world!
		}
		__pclose(reFp);
		if (DataManager_GetIntValue(TW_RM_RF_VAR) == 1 && (strcmp(rMnt.mnt,"system") == 0 || strcmp(rMnt.mnt,"data") == 0 || strcmp(rMnt.mnt,"cache") == 0)) { // we'll use rm -rf instead of formatting for system, data and cache if the option is set
			ui_print("...using rm -rf to wipe %s\n", rMnt.mnt);
			tw_mount(rMnt); // mount the partition first
			sprintf(rCommand,"rm -rf %s%s/*", "/", rMnt.mnt);
			LOGI("rm rf commad: %s\n", rCommand);
			reFp = __popen(rCommand, "r");
			while (fscanf(reFp,"%s",rOutput) == 1) {
				ui_print_overwrite("%s",rOutput);
			}
			__pclose(reFp);
		} else {
			ui_print("...Formatting %s\n",rMnt.mnt);
			tw_format(rFilesystem,rMnt.blk); // let's format block, based on filesystem from filename above
		}
		ui_print("....Done.\n");
		if (strcmp(rFilesystem,"mtd") == 0) { // if filesystem is mtd, we use flash image
			sprintf(rCommand,"flash_image %s %s",rMnt.mnt,rFilename);
			strcpy(rMount,rMnt.mnt);
		} else if (strcmp(rFilesystem,"emmc") == 0) { // if filesystem is emmc, we use dd
			sprintf(rCommand,"dd bs=%s if=%s of=%s",bs_size,rFilename,rMnt.dev);
			strcpy(rMount,rMnt.mnt);
		} else {
			tw_mount(rMnt);
			strcpy(rMount,"/");
			if (strcmp(rMnt.mnt,".android_secure") == 0) { // if it's android_secure, we have add prefix
				strcat(rMount,"sdcard/");
			}
			strcat(rMount,rMnt.mnt);
			sprintf(rCommand,"cd %s && tar -xvf %s",rMount,rFilename); // formulate shell command to restore
		}
		ui_print("...Restoring %s\n",rMount);
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
		ui_print_overwrite("....Done.\n");
		if (strcmp(rMnt.mnt,".android_secure") != 0) { // any partition other than android secure,
			tw_unmount(rMnt); // let's unmount (unmountable partitions won't matter)
		}
	} else {
		ui_print("...Failed md5 check. Aborted.\n\n");
		return 1;
	}
	ui_reset_progress();
	time(&rStop);
	ui_print("[%s DONE (%d SECONDS)]\n\n",rUppr,(int)difftime(rStop,rStart));
	return 0;
}

int 
nandroid_rest_exe()
{
	if (ensure_path_mounted(SDCARD_ROOT) != 0) {
		ui_print("-- Could not mount: %s.\n-- Aborting.\n",SDCARD_ROOT);
		return 1;
	}
	time_t rStart, rStop;
	time(&rStart);
	ui_print("\n[RESTORE STARTED]\n\n");
	if (tw_nan_system_x == 1) {
		if (tw_restore(sys,nan_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
			return 1;
		}
	}
	if (tw_nan_data_x == 1) {
		if (tw_restore(dat,nan_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
			return 1;
		}
	}
	if (tw_nan_boot_x == 1) {
		if (tw_restore(boo,nan_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
			return 1;
		}
	}
	if (tw_nan_recovery_x == 1) {
		if (tw_restore(rec,nan_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
			return 1;
		}
	}
	if (tw_nan_cache_x == 1) {
		if (tw_restore(cac,nan_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
			return 1;
		}
	}
	if (tw_nan_wimax_x == 1) {
		if (tw_restore(wim,nan_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
			return 1;
		}
	}
	if (tw_nan_andsec_x == 1) {
		if (tw_restore(ase,nan_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
			return 1;
		}
	}
	if (tw_nan_sdext_x == 1) {
		if (tw_restore(sde,nan_dir) == 1) {
			ui_print("-- Error occured, check recovery.log. Aborting.\n");
			return 1;
		}
	}
	time(&rStop);
	ui_print("[RESTORE COMPLETED IN %d SECONDS]\n\n",(int)difftime(rStop,rStart));
	__system("sync");
	LOGI("=> Let's update filesystem types.\n");
	verifyFst();
	LOGI("=> And update our fstab also.\n");
	createFstab();
	return 0;
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
    
    char** headers = prepend_title(MENU_HEADERS);
    
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

    dec_menu_loc();
}

int makeMD5(char *imgDir, const char *imgFile)
{
	int bool = 1;
	if (ensure_path_mounted("/sdcard") != 0) {
		LOGI("=> Can not mount sdcard.\n");
		return bool;
	} else {
		FILE *fp;
		char tmpString[255];
		char tmpAnswer[10];
		sprintf(tmpString,"cd %s && md5sum %s > %s.md5",imgDir,imgFile,imgFile);
		fp = __popen(tmpString, "r");
		fgets(tmpString, 255, fp);
		sscanf(tmpString,"%s %*s",tmpAnswer);
		if (strcmp(tmpAnswer,"md5sum:") == 0) {
			ui_print("....MD5 Error: %s", tmpString);
		} else {
			ui_print("....MD5 Created.\n");
			bool = 0;
		}
		__pclose(fp);
	}
	return bool;
}

int checkMD5(char *imgDir, const char *imgFile)
{
	int bool = 1;
	if (ensure_path_mounted("/sdcard") != 0) {
		LOGI("=> Can not mount sdcard.\n");
		return bool;
	} else {
		FILE *fp;
		char tmpString[255];
		char tmpAnswer[10];
		sprintf(tmpString,"cd %s && md5sum -c %s.md5",imgDir,imgFile);
		fp = __popen(tmpString, "r");
		fgets(tmpString, 255, fp);
		sscanf(tmpString,"%*s %s",tmpAnswer);
		if (strcmp(tmpAnswer,"OK") == 0) {
			ui_print("....MD5 Check: %s\n", tmpAnswer);
			bool = 0;
		} else {
			ui_print("....MD5 Error: %s", tmpString);
		}
		__pclose(fp);
	}
	return bool;
}
