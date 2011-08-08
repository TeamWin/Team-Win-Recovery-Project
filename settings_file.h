// Settings Constants 
#define TW_MIN_SETTINGS_CHARS 1 // Max Character Length Per Line
#define TW_MAX_SETTINGS_CHARS 255 // Max Character Length Per Line
#define TW_MAX_NUM_SETTINGS 16 // Total Number of Settings (Change this as we add more settings)

static const char* TW_SETTINGS_FILE = "/sdcard/TWRP/.twrps"; // Actual File
static const char* TW_VERSION = "VERSION"; // Used to make sure of settings compatability
static const char* TW_NAN_SYSTEM = "NAN_SYSTEM"; // system is backed up during nandroid (Constant number corresponds to line number in file .twrs)
static const char* TW_NAN_DATA = "NAN_DATA"; // data is backed up during nandroid (Constant number corresponds to line number in file .twrs)
static const char* TW_NAN_BOOT = "NAN_BOOT";
static const char* TW_NAN_RECOVERY = "NAN_RECOVERY";
static const char* TW_NAN_CACHE = "NAN_CACHE";
static const char* TW_NAN_WIMAX = "NAN_WIMAX";
static const char* TW_NAN_ANDSEC = "NAN_ANDSEC";
static const char* TW_NAN_SDEXT = "NAND_SDEXT";
static const char* TW_REBOOT_AFTER_FLASH = "REBOOT_AFTER_FLASH";
static const char* TW_SIGNED_ZIP = "SIGNED_ZIP"; // Zip Signed Toggle (Constant number corresponds to line number in file .twrs)
static const char* TW_COLOR_THEME = "COLOR_THEME";
static const char* TW_USE_COMPRESSION = "NAN_USE_COMPRESSION";
static const char* TW_SHOW_SPAM = "SHOW_SPAM";
static const char* TW_TIME_ZONE = "TIME_ZONE";
static const char* TW_ZIP_LOCATION = "ZIP_LOCATION"; // Last location zip flashed from (remembers last folder) (Constant number corresponds to line number in file .twrs)

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
