int __system(const char *command);

// Menus
void install_zip_menu();
void advanced_menu();

void usb_storage_toggle();
void wipe_dalvik_cache();

void format_menu();

void save_up_a_level_menu_location(int up_location);

// Format Menu Stuff
int erase_volume(const char *volume);
static long tmplog_offset = 0;

// Battery level
char* print_batt_cap();

void mark_menu_location();
void decrement_menu_location();
void clear_menu_marker();

void confirm_format(char* volume_name, char* volume_path);

int get_menu_selection(char** headers, char** items, int menu_only, int initial_selection);

char* zip_verify();
