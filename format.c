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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "format.h"
#include "extra-functions.h"
#include "common.h"

void tw_format(char *fstype, char *fsblock)
{
	char frmt[255];
	if (strcmp(fstype,"emmc") != 0 && strcmp(fstype,"mtd") != 0) {
		struct stat st;
		if (stat(fsblock,&st) == 0) {
			FILE *fp;
			char uCommand[255];
			char uOutput[50];
			char exe[255];
			sprintf(uCommand,"cat /proc/mounts | grep %s | awk '{ print $1 }'",fsblock); // form shell command to find block in mounts
			fp = __popen(uCommand, "r");
			LOGI("=> Formatting %s with %s\n",fsblock,fstype);
			if (fscanf(fp,"%s",uOutput) == 1) { // if block is found in mounts
				sprintf(exe,"umount %s",fsblock); // unmount block
				__system(exe);
			}
			__pclose(fp);
			if (fstype[0] == 'e' && fstype[1] == 'x') { // if it's ext
				sprintf(uCommand,"ls /sbin/mke2fs | grep mke2fs | awk '{ print $1 }'"); // form shell command to search for mke2fs
				fp = __popen(uCommand, "r");
				LOGI("=> checking for /sbin/mke2fs\n");
				if (fscanf(fp,"%s",uOutput) == 1) { // if mke2fs is found
					sprintf(exe,"mke2fs -t %s -m 0 %s",fstype,fsblock); // use mke2fs and format block according to fstype
					__system(exe);
				} else {
					sprintf(exe,"cat /etc/fstab | grep %s | awk '{ print $2 }'",fsblock); // find volume name in fstab
					fp = __popen(exe,"r");
					fscanf(fp,"%s",frmt);
					__pclose(fp);
					sprintf(exe, "rm -rf %s/* && rm -rf %s/.*", frmt, frmt);
					LOGI("rm -rf command: %s\n", exe);
					__system(exe); // we just rm -rf everything
				}
				LOGI("uOutput = %s\n", uOutput);
				__pclose(fp);
				
			} else if (fstype[0] == 'v' && fstype[1] == 'f') { // if it's vfat
				if (strcmp(fsblock,"/sdcard/.android_secure") == 0) { // if it's android secure, we shouldn't format sdcard so...
					__system("rm -rf /sdcard/.android_secure/* && rm -rf /sdcard/.android_secure/.*"); // we just rm -rf everything
				} else {
					sprintf(exe,"mkdosfs %s",fsblock); // use mkdosfs to format it
					__system(exe);
				}
			} else if (fstype[0] == 'y' && fstype[1] == 'a') { // if it's yaffs2
				sprintf(exe,"cat /etc/fstab | grep %s | awk '{ print $2 }'",fsblock); // find volume name in fstab
				fp = __popen(exe,"r");
				fscanf(fp,"%s",frmt);
			    if (strcmp(frmt,"/efs") == 0) { // if it's efs, let's not format it and just rm -rf everything
					__system("mount /efs");
			    	__system("rm -rf /efs/* && rm -rf /efs/.*");
			    } else {
					erase_volume(frmt); // it's not efs, so lets format it with aosp api
			    }
				__pclose(fp);
			}
		} else {
			LOGI("=> Device block is invalid.\n"); // oh noes, invalid, abort! Abort!
		}
	} else {
		LOGI("=> Can not format emmc or mmc.\n"); // oh noes, invalid, abort! Abort!
	}
}