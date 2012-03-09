void set_theme(const char* tw_theme);
char* checkTheme(int tw_theme);
void twrp_themes_menu();

struct twTheme {
	int r;
	int g;
	int b;
	int a;
};
struct twTheme htc, mtc, upc, mihc, miwhc, mhebc;
//HEADER_TEXT_COLOR
//MENU_ITEM_COLOR
//UI_PRINT_COLOR
//MENU_ITEM_HIGHLIGHT_COLOR
//MENU_ITEM_WHEN_HIGHLIGHTED_COLOR
//MENU_HORIZONTAL_END_BAR_COLOR
