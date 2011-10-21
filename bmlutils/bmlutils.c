#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <sys/wait.h>
#include <sys/limits.h>
#include <dirent.h>
#include <sys/stat.h>

#include <signal.h>
#include <sys/wait.h>

extern int __system(const char *command);
#define BML_UNLOCK_ALL 0x8A29 ///< unlock all partition RO -> RW

#ifndef BOARD_BML_BOOT
#define BOARD_BML_BOOT "/dev/block/bml7"
#endif

#ifndef BOARD_BML_RECOVERY
#define BOARD_BML_RECOVERY "/dev/block/bml8"
#endif

static int restore_internal(const char* bml, const char* filename)
{
    char buf[4096];
    int dstfd, srcfd, bytes_read, bytes_written, total_read = 0;
    if (filename == NULL)
        srcfd = 0;
    else {
        srcfd = open(filename, O_RDONLY | O_LARGEFILE);
        if (srcfd < 0)
            return 2;
    }
    dstfd = open(bml, O_RDWR | O_LARGEFILE);
    if (dstfd < 0)
        return 3;
    if (ioctl(dstfd, BML_UNLOCK_ALL, 0))
        return 4;
    do {
        total_read += bytes_read = read(srcfd, buf, 4096);
        if (!bytes_read)
            break;
        if (bytes_read < 4096)
            memset(&buf[bytes_read], 0, 4096 - bytes_read);
        if (write(dstfd, buf, 4096) < 4096)
            return 5;
    } while(bytes_read == 4096);
    
    close(dstfd);
    close(srcfd);
    
    return 0;
}

int cmd_bml_restore_raw_partition(const char *partition, const char *filename)
{
    if (strcmp(partition, "boot") != 0 && strcmp(partition, "recovery") != 0 && strcmp(partition, "recoveryonly") != 0)
        return 6;

    int ret = -1;
    if (strcmp(partition, "recoveryonly") != 0) {
        // always restore boot, regardless of whether recovery or boot is flashed.
        // this is because boot and recovery are the same on some samsung phones.
        // unless of course, recoveryonly is explictly chosen (bml8)
        ret = restore_internal(BOARD_BML_BOOT, filename);
        if (ret != 0)
            return ret;
    }

    if (strcmp(partition, "recovery") == 0 || strcmp(partition, "recoveryonly") == 0)
        ret = restore_internal(BOARD_BML_RECOVERY, filename);
    return ret;
}

int cmd_bml_backup_raw_partition(const char *partition, const char *out_file)
{
    char* bml;
    if (strcmp("boot", partition) == 0)
        bml = BOARD_BML_BOOT;
    else if (strcmp("recovery", partition) == 0)
        bml = BOARD_BML_RECOVERY;
    else {
        printf("Invalid partition.\n");
        return -1;
    }

    int ch;
    FILE *in;
    FILE *out;
    int val = 0;
    char buf[512];
    unsigned sz = 0;
    unsigned i;
    int ret = -1;
    char *in_file = bml;

    in = fopen ( in_file, "r" );
    if (in == NULL)
        goto ERROR3;

    out = fopen ( out_file, "w" );
    if (out == NULL)
        goto ERROR2;

    fseek(in, 0L, SEEK_END);
    sz = ftell(in);
    fseek(in, 0L, SEEK_SET);

    if (sz % 512)
    {
        while ( ( ch = fgetc ( in ) ) != EOF )
            fputc ( ch, out );
    }
    else
    {
        for (i=0; i< (sz/512); i++)
        {
            if ((fread(buf, 512, 1, in)) != 1)
                goto ERROR1;
            if ((fwrite(buf, 512, 1, out)) != 1)
                goto ERROR1;
        }
    }

    fsync(out);
    ret = 0;
ERROR1:
    fclose ( out );
ERROR2:
    fclose ( in );
ERROR3:
    return ret;
}

int cmd_bml_erase_raw_partition(const char *partition)
{
    // TODO: implement raw wipe
    return 0;
}

int cmd_bml_erase_partition(const char *partition, const char *filesystem)
{
    return -1;
}

int cmd_bml_mount_partition(const char *partition, const char *mount_point, const char *filesystem, int read_only)
{
    return -1;
}

int cmd_bml_get_partition_device(const char *partition, char *device)
{
    return -1;
}