// Settings Constants 
#define TW_MIN_SETTINGS_CHARS 1 // Max Character Length Per Line
#define TW_MAX_SETTINGS_CHARS 255 // Max Character Length Per Line
#define TW_MAX_NUM_SETTINGS 10 // Total Number of Settings (Change this as we add more settings)

static const char* TW_SETTINGS_FILE = "/sdcard/nandroid/.twrs"; // Actual File
static const int TW_SIGNED_ZIP = 0; // Zip Signed Toggle (Constant number corresponds to line number in file .twrs)
static const int TW_NAN_SYSTEM = 1; // system is backed up during nandroid (Constant number corresponds to line number in file .twrs)
static const int TW_NAN_DATA = 2; // data is backed up during nandroid (Constant number corresponds to line number in file .twrs)
static const int TW_NAN_CACHE = 3;
static const int TW_NAN_BOOT = 4;
static const int TW_NAN_WIMAX = 5;
static const int TW_NAN_RECOVERY = 6;
static const int TW_NAN_SDEXT = 7;
static const int TW_NAN_ANDSEC = 8;
static const int TW_ZIP_LOCATION = 9; // Last location zip flashed from (remembers last folder) (Constant number corresponds to line number in file .twrs)

static char tw_signed_zip_val[TW_MIN_SETTINGS_CHARS]; // Variable that holds value, default is defined
static char tw_nan_system_val[TW_MIN_SETTINGS_CHARS]; //
static char tw_nan_data_val[TW_MIN_SETTINGS_CHARS]; //
static char tw_nan_cache_val[TW_MIN_SETTINGS_CHARS];
static char tw_nan_boot_val[TW_MIN_SETTINGS_CHARS];
static char tw_nan_wimax_val[TW_MIN_SETTINGS_CHARS];
static char tw_nan_recovery_val[TW_MIN_SETTINGS_CHARS];
static char tw_nan_sdext_val[TW_MIN_SETTINGS_CHARS];
static char tw_nan_andsec_val[TW_MIN_SETTINGS_CHARS];
static char tw_zip_location_val[TW_MAX_SETTINGS_CHARS]; //

// Read Settings Function
int is_true(char* tw_setting);
void tw_set_defaults();
void write_s_file();
void read_s_file();

// Menus
void install_zip_menu();
void advanced_menu();

void usb_storage_toggle();
void wipe_dalvik_cache();

void save_up_a_level_menu_location(int up_location);

// Nandroid Menu Stuff
void nandroid_menu();
void nan_backup_menu();
void nan_restore_menu();
char* nan_img_set(int tw_setting);
void nandroid_string();