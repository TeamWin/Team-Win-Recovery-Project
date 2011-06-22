// Settings Constants 
static const char* TW_SETTINGS_FILE = "/sdcard/nandroid/.twrs"; // Actual File
static const int TW_MAX_SETTINGS_CHARS = 255; // Max Character Length Per Line
static const int TW_MAX_NUM_SETTINGS = 4; // Total Number of Settings (Change this as we add more settings)
static const int TW_SIGNED_ZIP = 0; // Zip Signed Toggle (Constant number corresponds to line number in file .twrs)
static const int TW_NAN_SYSTEM = 1; // system is backed up during nandroid (Constant number corresponds to line number in file .twrs)
static const int TW_NAN_DATA = 2; // data is backed up during nandroid (Constant number corresponds to line number in file .twrs)
static const int TW_ZIP_LOCATION = 3; // Last location zip flashed from (remembers last folder) (Constant number corresponds to line number in file .twrs)

static char* tw_signed_zip_val; // Variable that holds value, default is defined
static char* tw_nan_system_val; //
static char* tw_nan_data_val; //
static char* tw_zip_location_val; //

// Read Settings Function
int is_true(char* tw_setting);
void tw_set_defaults();
void tw_set_show();
void write_s_file();
void read_s_file();

// Menus
void install_zip_menu();
void advanced_menu();

void usb_storage_toggle();
void wipe_dalvik_cache();
