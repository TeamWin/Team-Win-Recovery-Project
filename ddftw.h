// Header goes here

#ifndef _DDFTW_HEADER
#define _DDFTW_HEADER

static const char tw_dinfo_file[] = "/etc/device.info";
static const char tw_block[] = "/dev/block/";
static const char tw_mtd[] = "/dev/mtd/";
static int has_datadata = 0;

enum backup_method {
    unknown = 0, 
    none, 
    image, 
    files,
};

enum flash_memory_type {
    unknown_flash_memory_type = 0, 
    mtd, 
    emmc, 
};

struct dInfo {
	char mnt[20];
	char blk[100];
	char dev[100];
	char fst[10];
	char fnm[256];
	char format_location[256];
	unsigned long long sze;
    unsigned long long used;
    int mountable;
    enum backup_method backup;
	enum flash_memory_type memory_type;
};

struct dInfo* findDeviceByLabel(const char* label);
struct dInfo* findDeviceByBlockDevice(const char* blockDevice);

void readRecFstab();
void verifyFst();
void createFstab();
void listMntInfo(struct dInfo* mMnt, char* variable_name);
unsigned long long getUsedSizeViaDu(const char* path);
int getLocations();
void updateUsedSized();

extern struct dInfo tmp, sys, dat, datadata, boo, rec, cac, sdcext, sdcint, ase, sde, sp1, sp2, sp3;
extern char tw_device_name[20];

#endif  // _DDFTW_HEADER

