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

#include "ddftw.h"
#include "common.h"
#include "bootloader.h"
#include "extra-functions.h"

void getLocations()
{
	char chkString[255];
	char locBase[15];
	sprintf(tw_fstab_file, "/tmp/%s.fstab", get_fstype());
	sprintf(chkString,"cat /proc/%s >> %s", get_fstype(), tw_fstab_file);
	__system(chkString);
	
	if (strcmp(get_fstype(), "mtd") == 0)
	{
		strcpy(locBase, "/dev/mtd/");
	} else if (strcmp(get_fstype(), "emmc") == 0)
	{
		strcpy(locBase, "/dev/block/");
	} else {
		ui_print("File type not supported");
	}
	
	FILE *fp;
	fp = fopen(tw_fstab_file, "r");
	if (fp == NULL)
	{
		ui_print("=> Can't open fstab");
	} else 
	{
		int len;
		char fs_dev[25];
		char fs_size[25];
		char fs_ersize[25];
		char fs_name[25];
		
		while (fscanf(fp, "%s %s %s %s", fs_dev, fs_size, fs_ersize, fs_name) != EOF) 
		{
			if (strcmp(fs_name,"name") != 0) {
				len = strlen(fs_dev); // get length of line
				if (fs_dev[len-1] == ':') { // if last char is carriage return
					fs_dev[len-1] = 0; // remove it by setting it to 0
				}
				if (strcmp(fs_name,"\"system\"") == 0) 
				{
					strcpy(tw_system_loc, locBase);
					strcat(tw_system_loc, fs_dev);
					LOGI("=> %s = %s\n", fs_name, tw_system_loc);
				} else if (strcmp(fs_name,"\"userdata\"") == 0) 
				{
					strcpy(tw_data_loc, locBase);
					strcat(tw_data_loc, fs_dev);
					LOGI("=> %s = %s\n", fs_name, tw_data_loc);
				} else if (strcmp(fs_name,"\"cache\"") == 0) 
				{
					strcpy(tw_cache_loc, locBase);
					strcat(tw_cache_loc, fs_dev);
					LOGI("=> %s = %s\n", fs_name, tw_cache_loc);
				} else if (strcmp(fs_name,"\"boot\"") == 0) 
				{
					strcpy(tw_boot_loc, locBase);
					strcat(tw_boot_loc, fs_dev);
					LOGI("=> %s = %s\n", fs_name, tw_boot_loc);
				} else if (strcmp(fs_name,"\"wimax\"") == 0) 
				{
					strcpy(tw_wimax_loc, locBase);
					strcat(tw_wimax_loc, fs_dev);
					LOGI("=> %s = %s\n", fs_name, tw_wimax_loc);
				} else if (strcmp(fs_name,"\"recovery\"") == 0) 
				{
					strcpy(tw_recovery_loc, locBase);
					strcat(tw_recovery_loc, fs_dev);
					LOGI("=> %s = %s\n", fs_name, tw_recovery_loc);
				}
			}
		}
	}
	get_device_id();
}