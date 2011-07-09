int __system(const char *command);

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

// Menu Stuff
#define GO_HOME 69

int go_home;
int go_menu;
int menu_loc_idx;
int menu_loc[255];

void inc_menu_loc(int bInt);
