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
#include <sys/types.h>
#include <dirent.h>

#include "format.h"
#include "extra-functions.h"
#include "common.h"
#include "mtdutils/mtdutils.h"
#include "mtdutils/mounts.h"
#include "ddftw.h"
#include "roots.h"
#include "backstore.h"
#include "data.h"

static int file_exists(const char* file)
{
    struct stat st;
    if (stat(file, &st) == 0)   return 1;
    return 0;
}

static int tw_format_mtd(const char* device)
{
    const char* location = device;

    Volume* v = volume_for_device(device);
    if (v)
    {
        // We may not have the right "name" for it... Let's flip it
        location = v->device;
    }

    LOGI("%s: Formatting \"%s\"\n", __FUNCTION__, location);

    mtd_scan_partitions();
    const MtdPartition* mtd = mtd_find_partition_by_name(location);
    if (mtd == NULL) {
        LOGE("%s: no mtd partition named \"%s\"", __FUNCTION__, location);
        return -1;
    }

    MtdWriteContext* ctx = mtd_write_partition(mtd);
    if (ctx == NULL) {
        LOGE("%s: can't write \"%s\"", __FUNCTION__, location);
        return -1;
    }
    if (mtd_erase_blocks(ctx, -1) == -1) {
        mtd_write_close(ctx);
        LOGE("%s: failed to erase \"%s\"", __FUNCTION__, location);
        return -1;
    }
    if (mtd_write_close(ctx) != 0) {
        LOGE("%s: failed to close \"%s\"", __FUNCTION__, location);
        return -1;
    }
    return 0;
}

static int tw_format_rmfr(const char* device)
{
    char cmd[256];

    ui_print("Using rm -rf on '%s'\n", device);

	Volume* v = volume_for_device(device);
    if (!v)
    {
        LOGE("%s: unable to locate volume for device \"%s\"\n", __FUNCTION__, device);
        return -1;
    }

    if (ensure_path_mounted(v->mount_point) != 0)
    {
        LOGE("%s: failed to mount \"%s\"\n", __FUNCTION__, v->mount_point);
        return -1;
    }
    
    sprintf(cmd, "rm -rf %s/* && rm -rf %s/.*", v->mount_point, v->mount_point);
    __system(cmd);

    if (ensure_path_unmounted(v->mount_point) != 0)
    {
        // This isn't a normal error, since some partitions like cache may not be unmountable
        LOGW("%s: failed to umount \"%s\"\n", __FUNCTION__, v->mount_point);
    }

    return 0;
}

static int tw_format_vfat(const char* device)
{
    char exe[512];

    // if it's android secure, we shouldn't format sdcard so...
    Volume* v = volume_for_device(device);
    if (v)
    {
        if (strcmp(device, "/sdcard/.android_secure") == 0)
            return tw_format_rmfr(device);
    }

    if (file_exists("/sbin/mkdosfs"))
    {
        sprintf(exe,"mkdosfs %s", device); // use mkdosfs to format it
        __system(exe);
    }
    else
        return tw_format_rmfr(device);

    return 0;
}

static int tw_format_ext23(const char* fstype, const char* device)
{
    if (file_exists("/sbin/mke2fs"))
    {
        char exe[512];

        sprintf(exe, "mke2fs -t %s -m 0 %s", fstype, device);
        LOGI("mke2fs command: %s\n", exe);
        __system(exe);
    }
    else
        return tw_format_rmfr(device);

    return 0;
}

static int tw_format_ext4(const char* device)
{
    return tw_format_ext23("ext4", device);
}

void wipe_data_without_wiping_media(void) {
	// This handles wiping data on devices with "sdcard" in /data/media
    ui_print("Wiping data without wiping /data/media\n");
	tw_mount(dat);
    __system("rm -f /data/*");
    __system("rm -f /data/.*");

    DIR* d;
    d = opendir("/data");
    if (d != NULL)
    {
        struct dirent* de;
        while ((de = readdir(d)) != NULL)
        {
            if (strcmp(de->d_name, "media") == 0)   continue;

            char cmd[256];
            sprintf(cmd, "rm -fr /data/%s", de->d_name);
            __system(cmd);
        }
        closedir(d);
    }
    tw_unmount(dat);
}

int tw_format(const char *fstype, const char *fsblock)
{
    int result = -1;

    if (DataManager_GetIntValue(TW_RM_RF_VAR) == 1) {
		tw_format_rmfr(fsblock);
		return 0;
	}

	LOGI("%s: Formatting \"%s\" as \"%s\"\n", __FUNCTION__, fsblock, fstype);
	if (DataManager_GetIntValue(TW_HAS_DATA_MEDIA) == 1 && strcmp(dat.blk, fsblock) == 0) {
		wipe_data_without_wiping_media();
		return 0;
	}
    Volume* v = volume_for_device(fsblock);
    if (v)
    {
        if (ensure_path_unmounted(v->mount_point) != 0)
        {
            LOGE("%s: failed to unmount \"%s\"\n", __FUNCTION__, v->mount_point);
            return -1;
        }
    }

    // Verify the block exists
    if (!file_exists(fsblock))
    {
        if (strcmp(fsblock, "userdata") == 0 && strcmp(fstype, "yaffs2") == 0) {
			// userdata doesn't exist in the file system, but it's the name used for fsblock MTD devices.
			// there is handling in tw_format_mtd if the fsblock doesn't exist, so we'll just keep going.
		} else {
			LOGE("%s: failed to locate device \"%s\"\n", __FUNCTION__, fsblock);
			return -1;
		}
    }

    // Let's handle the different types
    if (strcmp(fstype, "yaffs2") == 0 && v && strcmp(v->mount_point, "/efs") == 0)
        result = tw_format_rmfr(fsblock);

    else if (strcmp(fstype, "yaffs2") == 0 || strcmp(fstype, "mtd") == 0)
        result = tw_format_mtd(fsblock);

    else if (strcmp(fstype, "ext4") == 0)
        result = tw_format_ext4(fsblock);

    else if (strcmp(fstype, "vfat") == 0)
        result = tw_format_vfat(fsblock);

    else if (memcmp(fstype, "ext", 3) == 0)
        result = tw_format_ext23(fstype, fsblock);

    // Now, let's make sure we've set the +x for the mount point
    if (result == 0 && v)
    {
        if (ensure_path_mounted(v->mount_point) == 0)
        {
            struct stat st;

            if (stat(v->mount_point, &st) == 0)
            {
                chmod(v->mount_point, st.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
            }

            ensure_path_unmounted(v->mount_point);
        }
    }

    return result;
}
