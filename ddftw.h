static const char tw_dinfo_file[] = "/etc/device.info";
static const char tw_block[] = "/dev/block/";
static const char tw_mtd[] = "/dev/mtd/";

char tw_device_name[20];

struct dInfo {
	char mnt[20];
	char blk[100];
	char dev[100];
	char fst[10];
	char fnm[20];
	int sze;
};
struct dInfo tmp, sys, dat, boo, rec, wim, cac, sdc, ase, sde;

void readRecFstab();
void verifyFst();
void createFstab();
void getLocations();