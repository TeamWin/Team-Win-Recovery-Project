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
#include <sys/stat.h>

#include "ddftw.h"
#include "common.h"
#include "extra-functions.h"
#include "bootloader.h"
#include "backstore.h"

static int isMTDdevice = 0;
static int isBootMountable = 0;

struct dInfo* findDeviceByLabel(const char* label)
{
    if (!label)                             return NULL;
    if (strcmp(label, "system") == 0)       return &sys;
    if (strcmp(label, "userdata") == 0)     return &dat;
    if (strcmp(label, "data") == 0)         return &dat;
    if (strcmp(label, "boot") == 0)         return &boo;
    if (strcmp(label, "recovery") == 0)     return &rec;
    if (strcmp(label, "cache") == 0)        return &cac;
    if (strcmp(label, "wimax") == 0)        return &wim;
    if (strcmp(label, "efs") == 0)          return &wim;
    if (strcmp(label, "sdcard") == 0)       return &sdc;
    if (strcmp(label, "sd-ext") == 0)       return &sde;
    return NULL;
}

struct dInfo* findDeviceByBlockDevice(const char* blockDevice)
{
    if (!blockDevice)                       return NULL;
    if (strcmp(blockDevice, sys.blk) == 0)  return &sys;
    if (strcmp(blockDevice, dat.blk) == 0)  return &dat;
    if (strcmp(blockDevice, boo.blk) == 0)  return &boo;
    if (strcmp(blockDevice, rec.blk) == 0)  return &rec;
    if (strcmp(blockDevice, cac.blk) == 0)  return &cac;
    if (strcmp(blockDevice, wim.blk) == 0)  return &wim;
    if (strcmp(blockDevice, sdc.blk) == 0)  return &sdc;
    if (strcmp(blockDevice, sde.blk) == 0)  return &sde;
    return NULL;
}

// This routine handles the case where we can't open either /proc/mtd or /proc/emmc
int setLocationData(const char* label, const char* blockDevice, const char* mtdDevice, const char* fstype, int size)
{
    struct dInfo* loc = NULL;

    if (label)                  loc = findDeviceByLabel(label);
    if (!loc && blockDevice)    loc = findDeviceByBlockDevice(blockDevice);

    if (!loc)                   return -1;

    if (label)                  strcpy(loc->mnt, label);
    if (blockDevice)            strcpy(loc->blk, blockDevice);
    if (mtdDevice)              strcpy(loc->dev, mtdDevice);

    // This is a simple 
    if (strcmp(loc->mnt, "boot") == 0 && strlen(loc->blk) > 1)
    {
        // We use this flag to mark the boot partition as mountable
        if (fstype && strcmp(fstype, "vfat") == 0)
            isBootMountable = 1;

        if (strcmp(loc->blk, loc->dev) == 0)
            fstype = "emmc";
        else
            fstype = "mtd";
    }

    if (fstype)                 strcpy(loc->fst, fstype);
    if (size)                   loc->sze = size;

    return 0;
}

int getLocationsViafstab()
{
    FILE* fp;
    char line[512];
    int ret = 0;

    // In this case, we'll first get the partitions we care about (with labels)
    fp = fopen("/etc/recovery.fstab", "rt");
    if (fp == NULL)
    {
        LOGE("=> Unable to open /etc/recovery.fstab\n");
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char mount[32], fstype[32], device[64];
        char* pDevice = device;

        if (line[0] != '/')     continue;
        sscanf(line + 1, "%s %s %s", mount, fstype, device);

        if (device[0] != '/')   pDevice = NULL;
        setLocationData(mount, pDevice, pDevice, fstype, 0);
    }
    fclose(fp);

    // Now, let's retrieve base partition sizes
    if (!isMTDdevice)
    {
        fp = __popen("fdisk -l /dev/block/mmcblk0","r");
        if (fp == NULL)
        {
            LOGE("=> Unable to retrieve partition sizes via fdisk\n");
            return -1;
        }
    
        while (fgets(line, sizeof(line), fp) != NULL)
        {
            char isBoot[64], device[64], blocks[2][16];
            int size = 0;
    
            if (line[0] != '/')     continue;
            sscanf(line, "%s %s %*s %s %s", device, isBoot, blocks[0], blocks[1]);
    
            if (isBoot[0] == '*')       size = atoi(blocks[1]) * 1024;
            else                        size = atoi(blocks[0]) * 1024;

            if (size && (setLocationData(NULL, device, NULL, NULL, size) == 0))
                LOGI("  Mount %s size: %d\n", device, size);
        }
        fclose(fp);
    }
    return ret;
}

// get locations from our device.info
int getLocationsViaProc(const char* fstype)
{
	FILE *fp = NULL;
    char line[255];

    if (fstype == NULL)
    {
        LOGE("Invalid argument to getLocationsViaProc\n");
        return -1;
    }

    sprintf(line, "/proc/%s", fstype);
    fp = fopen(line, "rt");
    if (fp == NULL)
    {
        LOGW("Device does not support %s\n", line);
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char device[32], label[32];
        int size = 0;
        char mtdDevice[32];
        char* fstype = NULL;
        int deviceId;

        sscanf(line, "%s %x %*s %*c%s", device, &size, label);

        // Skip header and blank lines
        if ((strcmp(device, "dev:") == 0) || (strlen(line) < 8))
            continue;

        // Strip off the : and " from the details
        device[strlen(device)-1] = '\0';
        label[strlen(label)-1] = '\0';

        if (sscanf(device,"mtd%d", &deviceId) == 1)
        {
            isMTDdevice = 1;
            sprintf(mtdDevice, "%s%s", tw_mtd, device);
            sprintf(device, "%smtdblock%d", tw_block, deviceId);
            strcpy(tmp.fst,"mtd");
            fstype = "mtd";
        }
        else
        {
            strcpy(mtdDevice, device);
            fstype = "emmc";
        }

        if (strcmp(label, "efs") == 0)
            fstype = "yaffs2";

        setLocationData(label, device, mtdDevice, fstype, size);
    }

    fclose(fp);

    // We still proceed on to use the fstab to query devices as well
    return getLocationsViafstab();
}

int getLocations()
{
    if (getLocationsViaProc("emmc") != 0 && getLocationsViaProc("mtd") != 0 && getLocationsViafstab() != 0)
    {
        LOGE("E: Unable to get device locations.\n");
        return -1;
    }

    // Handle adding .android_secure and sd-ext
    strcpy(ase.dev,"/sdcard/.android_secure"); // android secure stuff
    strcpy(ase.blk,ase.dev);
    strcpy(ase.mnt,".android_secure");
    strcpy(ase.fst,"vfat");

    if (strcmp(sde.mnt,"sd-ext") != 0) // sd-ext stuff
	{
		int tmpInt;
		char tmpBase[50];
		char tmpWildCard[50];

		strcpy(tmpBase,sdc.blk);
		tmpBase[strlen(tmpBase)-1] = '\0';
		sprintf(tmpWildCard,"%s%%d",tmpBase);
		sscanf(sdc.blk, tmpWildCard, &tmpInt); // sdcard block used as sd-ext base
		sprintf(sde.blk, "%s%d",tmpBase, tmpInt+1);
	}

    get_device_id();

	LOGI("=> Let's update filesystem types.\n");
	verifyFst(); // use blkid to REALLY REALLY determine partition filesystem type
	LOGI("=> And update our fstab also.\n\n");
	createFstab(); // used for our busybox mount command

    return 0;
}

// write fstab so we can mount in adb shell
void createFstab()
{
	FILE *fp;
	struct stat st;
	fp = fopen("/etc/fstab", "w");
	if (fp == NULL) {
		LOGI("=> Can not open /etc/fstab.\n");
	} else 
	{
		char tmpString[255];
		sprintf(tmpString,"%s /%s %s rw\n",sys.blk,sys.mnt,sys.fst);
		if (tmpString[0] != ' ')    fputs(tmpString, fp);
		sprintf(tmpString,"%s /%s %s rw\n",dat.blk,dat.mnt,dat.fst);
        if (tmpString[0] != ' ')    fputs(tmpString, fp);
		sprintf(tmpString,"%s /%s %s rw\n",cac.blk,cac.mnt,cac.fst);
        if (tmpString[0] != ' ')    fputs(tmpString, fp);
		sprintf(tmpString,"%s /%s %s rw\n",sdc.blk,sdc.mnt,sdc.fst);
        if (tmpString[0] != ' ')    fputs(tmpString, fp);
		if (stat(sde.blk,&st) == 0) {
			strcpy(sde.mnt,"sd-ext");
			strcpy(sde.dev,sde.blk);
			sprintf(tmpString,"%s /%s %s rw\n",sde.blk,sde.mnt,sde.fst);
            if (tmpString[0] != ' ')    fputs(tmpString, fp);
			if (stat("/sd-ext",&st) != 0) {
				if(mkdir("/sd-ext",0777) == -1) {
					LOGI("=> Can not create /sd-ext folder.\n");
				} else {
					LOGI("=> Created /sd-ext folder.\n");
				}
			}
		}
		if (strcmp(wim.mnt,"efs") == 0) {
			sprintf(tmpString,"%s /%s %s rw\n",wim.blk,wim.mnt,wim.fst);
            if (tmpString[0] != ' ')    fputs(tmpString, fp);
			if (stat("/efs",&st) != 0) {
				if(mkdir("/efs",0777) == -1) {
					LOGI("=> Can not create /efs folder.\n");
				} else {
					LOGI("=> Created /efs folder.\n");
				}
			}
		}
        sprintf(tmpString,"%s /%s vfat rw\n",boo.blk,boo.mnt);
        if (tmpString[0] != ' ')    fputs(tmpString, fp);
	}
	fclose(fp);
}

void verifyFst()
{
	FILE *fp;
	char blkOutput[100];
	char* blk;
    char* arg;
    char* ptr;
    struct dInfo* dat;

    // This has a tendency to hang on MTD devices.
    if (isMTDdevice)    return;

	fp = __popen("blkid","r");
	while (fgets(blkOutput,sizeof(blkOutput),fp) != NULL)
    {
        blk = blkOutput;
        ptr = blkOutput;
        while (*ptr > 32 && *ptr != ':')        ptr++;
        if (*ptr == 0)                          continue;
        *ptr = 0;

        // Increment by two, but verify that we don't hit a NULL
        ptr++;
        if (*ptr != 0)      ptr++;

        // Now, find the TYPE field
        while (1)
        {
            arg = ptr;
            while (*ptr > 32)       ptr++;
            if (*ptr != 0)
            {
                *ptr = 0;
                ptr++;
            }

            if (strlen(arg) > 6)
            {
                if (memcmp(arg, "TYPE=\"", 6) == 0)  break;
                if (memcmp(arg, "TYPE=\"", 6) == 0)  break;
                if (memcmp(arg, "TYPE=\"", 6) == 0)  break;
            }

            if (*ptr == 0)
            {
                arg = NULL;
                break;
            }
        }

        if (arg && strlen(arg) > 7)
        {
            arg += 6;   // Skip the TYPE=" portion
            arg[strlen(arg)-1] = '\0';  // Drop the tail quote
        }
        else
            continue;

        dat = findDeviceByBlockDevice(blk);
        if (dat)
			strcpy(sys.fst,arg);
	}
	__pclose(fp);
}

