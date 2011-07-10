static const char tw_dinfo_file[] = "/etc/device.info";
char tw_device_name[20];

struct dInfo {
	char mnt[10];
	char blk[100];
	char dev[100];
	char fst[10];
};
struct dInfo tmp, sys, dat, boo, rec, wim, cac, mis, sdc;

void createFstab();
void getLocations();