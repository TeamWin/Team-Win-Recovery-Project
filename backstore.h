static const char nandroid_folder[] = "/sdcard/nandroid";
static const char tw_nan_system[] = "system.win";
static const char tw_nan_data[] = "data.win";
static const char tw_nan_cache[] = "cache.win";
static const char tw_nan_boot[] = "boot.win";
static const char tw_nan_wimax[] = "wimax.win";
static const char tw_nan_recovery[] = "recovery.win";
static const char tw_nan_andsec[] = "and-sec.win";
static const char tw_nan_sdext[] = "sd-ext.win";

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
void nandroid_back_exe();

void choose_nandroid_folder();
void set_restore_files();
void nan_restore_menu(int pIdx);
void nandroid_rest_exe();

int makeMD5(char *imgDir, const char *imgFile);
int checkMD5(char *imgDir, const char *imgFile);

