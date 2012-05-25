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

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>

#include "mtdutils/mtdutils.h"
#include "mtdutils/mounts.h"
#include "roots.h"
#include "common.h"
#include "ddftw.h"
#include "format.h"
#include "data.h"
#include "variables.h"
#include "extra-functions.h"

static int num_volumes = 0;
static Volume* device_volumes = NULL;

void load_volume_table() {
    int alloc = 10;
    device_volumes = malloc(alloc * sizeof(Volume));

    // Insert an entry for /tmp, which is the ramdisk and is always mounted.
    device_volumes[0].mount_point = "/tmp";
    device_volumes[0].fs_type = "ramdisk";
    device_volumes[0].device = NULL;
    device_volumes[0].device2 = NULL;
    num_volumes = 1;

    FILE* fstab = fopen("/etc/recovery.fstab", "r");
    if (fstab == NULL) {
        LOGE("failed to open /etc/recovery.fstab (%s)\n", strerror(errno));
        return;
    }

    char buffer[1024];
    int i;
    while (fgets(buffer, sizeof(buffer)-1, fstab)) {
        for (i = 0; buffer[i] && isspace(buffer[i]); ++i);
        if (buffer[i] == '\0' || buffer[i] == '#') continue;

        char* original = strdup(buffer);

        char* mount_point = strtok(buffer+i, " \t\n");
        char* fs_type = strtok(NULL, " \t\n");
        char* device = strtok(NULL, " \t\n");
        // lines may optionally have a second device, to use if
        // mounting the first one fails.
        char* device2 = strtok(NULL, " \t\n");

        if (mount_point && fs_type && device) {
            while (num_volumes >= alloc) {
                alloc += 5;
                device_volumes = realloc(device_volumes, alloc*sizeof(Volume));
            }
            device_volumes[num_volumes].mount_point = strdup(mount_point);
            device_volumes[num_volumes].fs_type = strdup(fs_type);
            device_volumes[num_volumes].device = strdup(device);
            device_volumes[num_volumes].device2 =
                device2 ? strdup(device2) : NULL;
            ++num_volumes;
        } else {
            LOGE("skipping malformed recovery.fstab line: %s\n", original);
        }
        free(original);
    }

    fclose(fstab);

    printf("recovery filesystem table\n");
    printf("=========================\n");
    for (i = 0; i < num_volumes; ++i) {
        Volume* v = &device_volumes[i];
        printf("  %d %s %s %s %s\n", i, v->mount_point, v->fs_type,
               v->device, v->device2);
    }
    printf("\n");
}

Volume* volume_for_path(const char* path) {
    char search_path[255];

	if (strcmp(path, "/sdcard") == 0) {
		if (DataManager_GetIntValue(TW_HAS_INTERNAL) == 1 && DataManager_GetIntValue(TW_HAS_DATA_MEDIA) == 1) {
			if (DataManager_GetIntValue(TW_HAS_EXTERNAL) == 0)
				ui_print("Unable to mount USB storage due to internal storage being in /data/media and no external storage available.\n");
			else
				strcpy(search_path, DataManager_GetStrValue(TW_EXTERNAL_MOUNT));
		} else
			strcpy(search_path, DataManager_GetCurrentStorageMount());
	} else
		strcpy(search_path, path);

	int i;
    for (i = 0; i < num_volumes; ++i) {
        Volume* v = device_volumes+i;
        int len = strlen(v->mount_point);
        if (strncmp(search_path, v->mount_point, len) == 0 &&
            (search_path[len] == '\0' || search_path[len] == '/')) {
            return v;
        }
    }
    return NULL;
}

Volume* volume_for_device(const char* device)
{
    int i;
    struct dInfo* loc = findDeviceByBlockDevice(device);

    for (i = 0; i < num_volumes; ++i)
    {
        Volume* v = &device_volumes[i];
        if (v->device && strcmp(device, v->device) == 0)
            return v;
        if (v->device && loc && strcmp(loc->mnt, v->device) == 0)
            return v;
    }
    return NULL;
}

int ensure_path_mounted(const char* path) {

    if ((DataManager_GetIntValue(TW_HAS_DATA_MEDIA) == 1 || DataManager_GetIntValue(TW_HAS_DUAL_STORAGE) == 0) && strncmp(path, "/sdcard", 7) == 0)
		return 0; // sdcard is just a symlink

	Volume* v = volume_for_path(path);
    if (v == NULL) {
        LOGE("unknown volume for path [%s]\n", path);
        return -1;
    }
    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // the ramdisk is always mounted.
        return 0;
    }

    int result;
    result = scan_mounted_volumes();
    if (result < 0) {
        LOGE("failed to scan mounted volumes\n");
        return -1;
    }

    const MountedVolume* mv =
        find_mounted_volume_by_mount_point(v->mount_point);
    if (mv) {
        // volume is already mounted
        return 0;
    }

    mkdir(v->mount_point, 0755);  // in case it doesn't already exist

    if (strcmp(v->fs_type, "yaffs2") == 0) {
        // mount an MTD partition as a YAFFS2 filesystem.
        mtd_scan_partitions();
        const MtdPartition* partition;
        partition = mtd_find_partition_by_name(v->device);
        if (partition == NULL) {
            LOGE("failed to find \"%s\" partition to mount at \"%s\"\n",
                 v->device, v->mount_point);
            return -1;
        }
        return mtd_mount_partition(partition, v->mount_point, v->fs_type, 0);
    } else if (strcmp(v->fs_type, "ext4") == 0 ||
               strcmp(v->fs_type, "ext3") == 0 ||
               strcmp(v->fs_type, "ext2") == 0 ||
               strcmp(v->fs_type, "vfat") == 0) {
        result = mount(v->device, v->mount_point, v->fs_type,
                       MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
        if (result == 0) return 0;

        if (v->device2) {
            LOGW("failed to mount %s (%s); trying %s\n",
                 v->device, strerror(errno), v->device2);
            result = mount(v->device2, v->mount_point, v->fs_type,
                           MS_NOATIME | MS_NODEV | MS_NODIRATIME, "");
            if (result == 0) return 0;
        }

        LOGE("failed to mount %s (%s)\n", v->mount_point, strerror(errno));
        return -1;
    }

    LOGE("unknown fs_type \"%s\" for %s\n", v->fs_type, v->mount_point);
    return -1;
}

int ensure_path_unmounted(const char* path) {
	if ((DataManager_GetIntValue(TW_HAS_DATA_MEDIA) == 1 || DataManager_GetIntValue(TW_HAS_DUAL_STORAGE) == 0) && strncmp(path, "/sdcard", 7) == 0)
		return 0; // sdcard is just a symlink

    // This is an old, common method for ensuring a flush
    sync();
    sync();

	if (DataManager_GetIntValue(TW_DONT_UNMOUNT_SYSTEM) == 1 && strncmp(path, "/system", 7) == 0)
		return 0;

	Volume* v = volume_for_path(path);
    if (v == NULL) {
        LOGE("unknown volume for path [%s]\n", path);
        return -1;
    }
    if (strcmp(v->fs_type, "ramdisk") == 0) {
        // the ramdisk is always mounted; you can't unmount it.
        return -1;
    }

    int result;
    result = scan_mounted_volumes();
    if (result < 0) {
        LOGE("failed to scan mounted volumes\n");
        return -1;
    }

    const MountedVolume* mv =
        find_mounted_volume_by_mount_point(v->mount_point);
    if (mv == NULL) {
        // volume is already unmounted
        return 0;
    }

    return unmount_mounted_volume(mv);
}

int mount_current_storage(void) {
	char mount_point[255];

	strcpy(mount_point, DataManager_GetCurrentStorageMount());

	return ensure_path_mounted(mount_point);
}

int unmount_current_storage(void) {
	char mount_point[255];

	strcpy(mount_point, DataManager_GetCurrentStorageMount());

	return ensure_path_unmounted(mount_point);
}

int mount_internal_storage(void) {
	char mount_point[255];

	strcpy(mount_point, DataManager_GetSettingsStorageMount());
	return ensure_path_mounted(mount_point);
}

int unmount_internal_storage(void) {
	char mount_point[255];

	strcpy(mount_point, DataManager_GetSettingsStorageMount());
	return ensure_path_unmounted(mount_point);
}

int format_volume(const char* volume) {
    Volume* v = volume_for_path(volume);
    if (v == NULL) {
        LOGE("unknown volume \"%s\"\n", volume);
        return -1;
    }

    if (strcmp(v->mount_point, volume) != 0) {
        LOGE("can't give path \"%s\" to format_volume\n", volume);
        return -1;
    }

    // Retrieve the fs_type
    const char* fs_type = v->fs_type;
    if (memcmp(fs_type, "ext", 3) == 0)
    {
        if (strcmp(v->mount_point,"/system") == 0)      fs_type = sys.fst;
        else if (strcmp(v->mount_point,"/data") == 0)   fs_type = dat.fst;
        else if (strcmp(v->mount_point,"/cache") == 0)  fs_type = cac.fst;
    }

    return tw_format(fs_type, v->device);
}
