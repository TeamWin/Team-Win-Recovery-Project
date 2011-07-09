static const char nandroid_folder[] = "/sdcard/nandroid";
static const char tw_nan_system[] = "system.img";
static const char tw_nan_data[] = "data.img";
static const char tw_nan_cache[] = "cache.img";
static const char tw_nan_boot[] = "boot.img";
static const char tw_nan_wimax[] = "wimax.img";
static const char tw_nan_recovery[] = "recovery.img";
static const char tw_nan_sdext[] = "sd-ext.img";
static const char tw_nan_andsec[] = "and-sec.img";

int tw_nan_system_x;
int tw_nan_data_x;
int tw_nan_cache_x;
int tw_nan_boot_x;
int tw_nan_wimax_x;
int tw_nan_recovery_x;
int tw_nan_sdext_x;
int tw_nan_andsec_x;

char* nan_img_set(int tw_setting, int tw_backstore);
char tw_nandroid_string[1024];
char nan_dir[255];

void nandroid_menu();
void nan_backup_menu();
void nandroid_back_exe();

void choose_nandroid_folder();
void set_restore_files();
void nan_restore_menu();
void nandroid_rest_exe();