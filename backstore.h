static const char backup_folder[] = "/sdcard/TWRP/BACKUPS";
static const char bs_size[] = "4096";

int tw_nan_system_x;
int tw_nan_data_x;
int tw_nan_cache_x;
int tw_nan_boot_x;
int tw_nan_wimax_x;
int tw_nan_recovery_x;
int tw_nan_andsec_x;
int tw_nan_sdext_x;

int tw_total;

char* nan_img_set(int tw_setting, int tw_backstore);
char tw_nandroid_string[1024];
char nan_dir[255];

void nandroid_menu();
void nan_backup_menu(int pIdx);
int nandroid_back_exe();
int nandroid_rest_exe();

void choose_backup_folder();
void set_restore_files();
void nan_restore_menu(int pIdx);
char* nan_compress();

int sdSpace;

int makeMD5(char *imgDir, const char *imgFile);
int checkMD5(char *imgDir, const char *imgFile);