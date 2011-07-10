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
#include "extra-functions.h"

// get locations from our device.info
void getLocations()
{	
	FILE *fp;
	fp = fopen(tw_dinfo_file, "r");
	if (fp == NULL)
	{
		LOGI("=> Can't open %s\n", tw_dinfo_file);
	} else {
		fscanf(fp,"# <%s>",tw_device_name); // scan in device name from device.info
		tw_device_name[strlen(tw_device_name)-1] = '\0';
		while (fscanf(fp,"%s %s %s %s",tmp.mnt,tmp.blk,tmp.dev,tmp.fst) != EOF)
		{
			if (strcmp(tmp.mnt,"system") == 0) { // read in system line
				strcpy(sys.mnt,tmp.mnt);
				strcpy(sys.blk,tmp.blk);
				strcpy(sys.dev,tmp.dev);
				strcpy(sys.fst,tmp.fst);
			}
			if (strcmp(tmp.mnt,"data") == 0) {
				strcpy(dat.mnt,tmp.mnt);
				strcpy(dat.blk,tmp.blk);
				strcpy(dat.dev,tmp.dev);
				strcpy(dat.fst,tmp.fst);
			}
			if (strcmp(tmp.mnt,"boot") == 0) {
				strcpy(boo.mnt,tmp.mnt);
				strcpy(boo.blk,tmp.blk);
				strcpy(boo.dev,tmp.dev);
				strcpy(boo.fst,tmp.fst);
			}
			if (strcmp(tmp.mnt,"wimax") == 0) {
				strcpy(wim.mnt,tmp.mnt);
				strcpy(wim.blk,tmp.blk);
				strcpy(wim.dev,tmp.dev);
				strcpy(wim.fst,tmp.fst);
			}
			if (strcmp(tmp.mnt,"recovery") == 0) {
				strcpy(rec.mnt,tmp.mnt);
				strcpy(rec.blk,tmp.blk);
				strcpy(rec.dev,tmp.dev);
				strcpy(rec.fst,tmp.fst);
			}
			if (strcmp(tmp.mnt,"cache") == 0) {
				strcpy(cac.mnt,tmp.mnt);
				strcpy(cac.blk,tmp.blk);
				strcpy(cac.dev,tmp.dev);
				strcpy(cac.fst,tmp.fst);
			}
			if (strcmp(tmp.mnt,"sdcard") == 0) {
				strcpy(sdc.mnt,tmp.mnt);
				strcpy(sdc.blk,tmp.blk);
				strcpy(sdc.dev,tmp.dev);
				strcpy(sdc.fst,tmp.fst);
			}
		}
		fclose(fp);
		createFstab();
		get_device_id();
	}
}

// write fstab so we can mount in adb shell
void createFstab()
{
	FILE *fp;
	fp = fopen("/etc/fstab", "w");
	if (fp == NULL) {
		LOGI("=> Can not open /etc/fstab.\n");
	} else {
		char tmpString[255];
		sprintf(tmpString,"# %s FSTAB\n",tw_device_name);
		fputs(tmpString, fp);
		sprintf(tmpString,"%s /%s %s rw\n",sys.blk,sys.mnt,sys.fst);
		fputs(tmpString, fp);
		sprintf(tmpString,"%s /%s %s rw\n",dat.blk,dat.mnt,dat.fst);
		fputs(tmpString, fp);
		sprintf(tmpString,"%s /%s %s rw\n",sdc.blk,sdc.mnt,sdc.fst);
		fputs(tmpString, fp);
	}
	fclose(fp);
	LOGI("=> /etc/fstab created.\n");
}