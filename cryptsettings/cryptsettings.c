#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "cutils/properties.h"

int main(void)
{
	char prop[255];

	printf("This tool will gather the build flags needed for decryption support for TWRP.\n");
	printf("This tool comes with no warranties whatsoever.\n");
	printf("http://teamw.in\n\n");
	property_get("ro.crypto.state", prop, "encrypted");
	if (strcmp(prop, "encrypted") != 0)
		printf("Your device is not encrypted, continuing anyway.\n\n");
	printf("TW_INCLUDE_CRYPTO := true\n");
	property_get("ro.crypto.fs_type", prop, "ERROR");
	printf("TW_CRYPTO_FS_TYPE := \"%s\"\n", prop);
	property_get("ro.crypto.fs_real_blkdev", prop, "ERROR");
	printf("TW_CRYPTO_REAL_BLKDEV := \"%s\"\n", prop);
	property_get("ro.crypto.fs_mnt_point", prop, "ERROR");
	printf("TW_CRYPTO_MNT_POINT := \"%s\"\n", prop);
	property_get("ro.crypto.fs_options", prop, "ERROR");
	printf("TW_CRYPTO_FS_OPTIONS := \"%s\"\n", prop);
	property_get("ro.crypto.fs_flags", prop, "ERROR");
	printf("TW_CRYPTO_FS_FLAGS := \"%s\"\n", prop);
	property_get("ro.crypto.keyfile.userdata", prop, "footer");
	printf("TW_CRYPTO_KEY_LOC := \"%s\"\n", prop);

	return 0;
}
