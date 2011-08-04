// Settings Constants 
#define TW_MIN_SETTINGS_CHARS 1 // Max Character Length Per Line
#define TW_MAX_SETTINGS_CHARS 255 // Max Character Length Per Line
#define TW_MAX_NUM_SETTINGS 16 // Total Number of Settings (Change this as we add more settings)

static const char* TW_SETTINGS_FILE = "/sdcard/TWRP/.twrps"; // Actual File
static const int TW_VERSION = 0; // Used to make sure of settings compatability
static const int TW_NAN_SYSTEM = 1; // system is backed up during nandroid (Constant number corresponds to line number in file .twrs)
static const int TW_NAN_DATA = 2; // data is backed up during nandroid (Constant number corresponds to line number in file .twrs)
static const int TW_NAN_BOOT = 3;
static const int TW_NAN_RECOVERY = 4;
static const int TW_NAN_CACHE = 5;
static const int TW_NAN_WIMAX = 6;
static const int TW_NAN_ANDSEC = 7;
static const int TW_NAN_SDEXT = 8;
static const int TW_REBOOT_AFTER_FLASH = 9;
static const int TW_SIGNED_ZIP = 10; // Zip Signed Toggle (Constant number corresponds to line number in file .twrs)
static const int TW_COLOR_THEME = 11;
static const int TW_USE_COMPRESSION = 12;
static const int TW_SHOW_SPAM = 13;
static const int TW_TIME_ZONE = 14;
static const int TW_ZIP_LOCATION = 15; // Last location zip flashed from (remembers last folder) (Constant number corresponds to line number in file .twrs)

static const char tw_version_val[] = "1.0.3"; // Variable Holding Version
char tw_nan_system_val[TW_MIN_SETTINGS_CHARS]; //
char tw_nan_data_val[TW_MIN_SETTINGS_CHARS]; //
char tw_nan_boot_val[TW_MIN_SETTINGS_CHARS];
char tw_nan_recovery_val[TW_MIN_SETTINGS_CHARS];
char tw_nan_cache_val[TW_MIN_SETTINGS_CHARS];
char tw_nan_wimax_val[TW_MIN_SETTINGS_CHARS];
char tw_nan_andsec_val[TW_MIN_SETTINGS_CHARS];
char tw_nan_sdext_val[TW_MIN_SETTINGS_CHARS];
char tw_reboot_after_flash_option[TW_MIN_SETTINGS_CHARS];
char tw_signed_zip_val[TW_MIN_SETTINGS_CHARS];
char tw_color_theme_val[TW_MIN_SETTINGS_CHARS];
char tw_use_compression_val[20];
char tw_show_spam_val[20];
char tw_time_zone_val[20];
char tw_zip_location_val[TW_MAX_SETTINGS_CHARS];

// Read Settings Function
int is_true(char* tw_setting);
void tw_set_defaults();
void write_s_file();
void read_s_file();
