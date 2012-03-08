// Header goes here

#include "ddftw.h"

#ifndef _BACKSTORE_HEADER
#define _BACKSTORE_HEADER

static const char backup_folder[] = "/sdcard/TWRP/BACKUPS";
static const char bs_size[] = "4096";

char* nan_img_set(int tw_setting, int tw_backstore);
char tw_nandroid_string[1024];

void nandroid_menu();
void nan_backup_menu(int pIdx);
int nandroid_back_exe();
int nandroid_rest_exe();

int recursive_mkdir(const char* path);

void choose_backup_folder();
void set_restore_files();
void nan_restore_menu(int pIdx);
char* nan_compress();

int sdSpace;

int makeMD5(const char *imgDir, const char *imgFile);
int checkMD5(const char *imgDir, const char *imgFile);

int tw_isMounted(struct dInfo mMnt);
int tw_mount(struct dInfo mMnt);
int tw_unmount(struct dInfo uMnt);

#endif  // _BACKSTORE_HEADER
