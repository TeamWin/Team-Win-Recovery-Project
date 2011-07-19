int __system(const char *command);
FILE * __popen(const char *program, const char *type);
int __pclose(FILE *iop);

static int tw_gapps_auto_backup_restore_option ; // option to backup / restore gapps before and after a zip flash - not saved in settings, defaults to off

// Device ID variable / function
char device_id[15];
void get_device_id();

// Menus
void install_zip_menu();
void advanced_menu();

void usb_storage_toggle();
void wipe_dalvik_cache();

void format_menu();

// Format Menu Stuff
int erase_volume(const char *volume);
static long tmplog_offset = 0;

// Battery level
char* print_batt_cap();

void confirm_format(char* volume_name, char* volume_path);

int get_menu_selection(char** headers, char** items, int menu_only, int initial_selection);

char* zip_verify();
char* reboot_after_flash();
char* backup_restore_gapps();

char* save_reboot_setting();
void all_settings_menu();
void time_zone_menu();
void update_tz_environment_variables();

// Menu Stuff
#define GO_HOME 69

int go_home;
int go_menu;
int menu_loc_idx;
int menu_loc[255];

void inc_menu_loc(int bInt);
void dec_menu_loc();

