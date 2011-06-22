#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/reboot.h>
#include <reboot/reboot.h>
#include <unistd.h>

int reboot_main(int argc, char *argv[])
{
    int ret;
    int nosync = 0;
    int poweroff = 0;

    opterr = 0;
    do {
        int c;

        c = getopt(argc, argv, "np");
        
        if (c == EOF) {
            break;
        }
        
        switch (c) {
        case 'n':
            nosync = 1;
            break;
        case 'p':
            poweroff = 1;
            break;
        case '?':
            fprintf(stderr, "usage: %s [-n] [-p] [rebootcommand]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    } while (1);

    if(argc > optind + 1) {
        fprintf(stderr, "%s: too many arguments\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if(!nosync)
        sync();

    if(poweroff)
        ret = __reboot(LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2, LINUX_REBOOT_CMD_POWER_OFF, NULL);
    else if(argc > optind) {
        ret = reboot_wrapper(argv[optind]);
    } else
        ret = reboot_wrapper(NULL);
    if(ret < 0) {
        perror("reboot");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "reboot returned\n");
    return 0;
}
