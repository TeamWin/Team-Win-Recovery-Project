// Header goes here

#ifndef _DDFTW_HEADER
#define _DDFTW_HEADER

static const char tw_dinfo_file[] = "/etc/device.info";
static const char tw_block[] = "/dev/block/";
static const char tw_mtd[] = "/dev/mtd/";

char tw_device_name[20];

enum backup_method {
    unknown = 0, 
    none, 
    image, 
    files,
};

struct dInfo {
	char mnt[20];
	char blk[100];
	char dev[100];
	char fst[10];
	char fnm[256];
	unsigned long long sze;
    unsigned long long used;
    int mountable;
    enum backup_method backup;
};

struct dInfo tmp, sys, dat, boo, rec, cac, sdcext, sdcint, ase, sde, sp1, sp2, sp3;

struct dInfo* findDeviceByLabel(const char* label);

void readRecFstab();
void verifyFst();
void createFstab();
int getLocations();
void updateUsedSized();

#endif  // _DDFTW_HEADER

