int __system(const char *command);
FILE * __popen(const char *program, const char *type);
int __pclose(FILE *iop);

// Device ID variable / function
char device_id[15];
void get_device_id();

// Menus
void install_zip_menu(int pIdx);
void advanced_menu();

void usb_storage_toggle();
void wipe_dalvik_cache();

void format_menu();
void main_wipe_menu();

// Format Menu Stuff
int erase_volume(const char *volume);
static long tmplog_offset = 0;

// Battery level
char* print_batt_cap();

void confirm_format(char* volume_name, char* volume_path);

int get_menu_selection(char** headers, char** items, int menu_only, int initial_selection);

char* zip_verify();
char* reboot_after_flash();

char* save_reboot_setting();
void all_settings_menu(int pIdx);
void time_zone_menu();
void time_zone_minus();
void time_zone_plus();
void time_zone_offset();
char time_zone_offset_string[5];
char time_zone_dst_string[10];
void time_zone_dst();
void update_tz_environment_variables();

// Menu Stuff
#define GO_HOME 69

int go_home;
int go_menu;
int menu_loc_idx;
int menu_loc[255];

void inc_menu_loc(int bInt);
void dec_menu_loc();

char* isMounted(int mInt);
void mount_menu(int pIdx);
void chkMounts();
int sysIsMounted;
int datIsMounted;
int cacIsMounted;
int sdcIsMounted;
int sdeIsMounted;

int get_new_zip_dir;

char* checkTheme(int tw_theme);
void tw_reboot();

void fix_perms();
char* toggle_spam();