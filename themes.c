/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "themes.h"
#include "extra-functions.h"
#include "data.h"

#define THEME_REBOOT_RECOVERY 0
#define TW_THEME              1
#define CM_THEME              2
#define RED_THEME             3
#define GOOGLE_THEME          4
#define JF_THEME              5
#define HTC_THEME             6
#define FABULOUS_THEME        7
#define PURPLE_SHIFT          8
#define GREYBALLER_THEME      9
#define TRIPPY_THEME	      10
#define SHIFTY_BASTARD	      11
#define MYN_WARM              12
#define THEMES_BACK           13

char* checkTheme(int tw_theme)
{
	int isVal;
	int isTrue = 0;
	char* tmp_set = (char*)malloc(40);
	switch (tw_theme)
	{
		case TW_THEME:
			strcpy(tmp_set, "[ ] Team Win Theme (Default)");
			break;
		case CM_THEME:
			strcpy(tmp_set, "[ ] CyanogenMod Theme");
			break;
		case RED_THEME:
			strcpy(tmp_set, "[ ] Red Rum Theme");
			break;
		case GOOGLE_THEME:
			strcpy(tmp_set, "[ ] Google Theme");
			break;
		case JF_THEME:
			strcpy(tmp_set, "[ ] JesusFreke Theme (oldschool)");
			break;
		case HTC_THEME:
			strcpy(tmp_set, "[ ] HTC Theme");
			break;
		case FABULOUS_THEME:
			strcpy(tmp_set, "[ ] Fabulous Theme");
			break;
		case PURPLE_SHIFT:
			strcpy(tmp_set, "[ ] Purple Shift");
			break;
		case GREYBALLER_THEME:
			strcpy(tmp_set, "[ ] Greyballer");
			break;
		case TRIPPY_THEME:
			strcpy(tmp_set, "[ ] Trippy");
			break;
		case SHIFTY_BASTARD:
			strcpy(tmp_set, "[ ] Shifty Bastard");
			break;
		case MYN_WARM:
			strcpy(tmp_set, "[ ] Myn's Warm");
			break;
	}
	isVal = DataManager_GetIntValue(TW_COLOR_THEME_VAR);
	if (isVal == tw_theme - 1)
	{
		tmp_set[1] = 'x';
	}
	return tmp_set;
}

void twrp_themes_menu()
{
    const char* MENU_THEMES_HEADERS[] = {  "twrp Theme Chooser",
    								   	   "Taste tEh Rainbow:",
                                           NULL };
    
	char* MENU_THEMES[] =       { 	"[RESTART MENU AND APPLY THEME]",
									checkTheme(TW_THEME),
									checkTheme(CM_THEME),
									checkTheme(RED_THEME),
									checkTheme(GOOGLE_THEME),
									checkTheme(JF_THEME),
									checkTheme(HTC_THEME),
									checkTheme(FABULOUS_THEME),
									checkTheme(PURPLE_SHIFT),
									checkTheme(GREYBALLER_THEME),
									checkTheme(TRIPPY_THEME),
									checkTheme(SHIFTY_BASTARD),
									checkTheme(MYN_WARM),
									"<-- Back To twrp Settings",
									NULL };

    char** headers = prepend_title(MENU_THEMES_HEADERS);
    
    inc_menu_loc(THEMES_BACK);
    for (;;)
    {
        int chosen_item = get_menu_selection(headers, MENU_THEMES, 0, 0);
        switch (chosen_item)
        {
            case THEME_REBOOT_RECOVERY:
				set_theme(DataManager_GetStrValue(TW_COLOR_THEME_VAR));
				go_home = 1;
				go_restart = 1;
                break;
            case TW_THEME:
            	DataManager_SetIntValue(TW_COLOR_THEME_VAR, 0);
                break;
            case CM_THEME:
                DataManager_SetIntValue(TW_COLOR_THEME_VAR, 1);
                break;
            case RED_THEME:
                DataManager_SetIntValue(TW_COLOR_THEME_VAR, 2);
                break;
            case GOOGLE_THEME:
                DataManager_SetIntValue(TW_COLOR_THEME_VAR, 3);
                break;
            case JF_THEME:
                DataManager_SetIntValue(TW_COLOR_THEME_VAR, 4);
                break;
            case HTC_THEME:
                DataManager_SetIntValue(TW_COLOR_THEME_VAR, 5);
                break;
            case FABULOUS_THEME:
                DataManager_SetIntValue(TW_COLOR_THEME_VAR, 6);
                break;
			case PURPLE_SHIFT:
                DataManager_SetIntValue(TW_COLOR_THEME_VAR, 7);
                break;
			case GREYBALLER_THEME:
                DataManager_SetIntValue(TW_COLOR_THEME_VAR, 8);
                break;
			case TRIPPY_THEME:
                DataManager_SetIntValue(TW_COLOR_THEME_VAR, 9);
                break;
			case SHIFTY_BASTARD:
                DataManager_SetIntValue(TW_COLOR_THEME_VAR, 10);
                break;
			case MYN_WARM:
                DataManager_SetIntValue(TW_COLOR_THEME_VAR, 11);
                break;
            case THEMES_BACK:
            	dec_menu_loc();
            	return;
        }
        if (go_home) {
        	dec_menu_loc();
	        return;
	    }
        break;
    }
	ui_end_menu();
    dec_menu_loc();
    twrp_themes_menu();
}

void set_theme(const char* tw_theme)
{
	if (strcmp(tw_theme,"0") == 0) // TEAMWIN THEME (default)
	{
		//HEADER_TEXT_COLOR
		htc.r = 255;
		htc.g = 255;
		htc.b = 255;
		htc.a = 255;

		//MENU_ITEM_COLOR
		mtc.r = 0;
		mtc.g = 255;
		mtc.b = 0;
		mtc.a = 255;

		//UI_PRINT_COLOR
		upc.r = 0;
		upc.g = 255;
		upc.b = 0;
		upc.a = 255;

		//MENU_ITEM_HIGHLIGHT_COLOR
		mihc.r = 0;
		mihc.g = 255;
		mihc.b = 0;
		mihc.a = 255;

		//MENU_ITEM_WHEN_HIGHLIGHTED_COLOR
		miwhc.r = 0;
		miwhc.g = 0;
		miwhc.b = 0;
		miwhc.a = 0;

		//MENU_HORIZONTAL_END_BAR_COLOR
		mhebc.r = 0;
		mhebc.g = 255;
		mhebc.b = 0;
		mhebc.a = 255;
	}
	if (strcmp(tw_theme,"1") == 0) // CM theme
	{
		htc.r = 193;
		htc.g = 193;
		htc.b = 193;
		htc.a = 255;

		mtc.r = 61;
		mtc.g = 233;
		mtc.b = 255;
		mtc.a = 255;

		upc.r = 193;
		upc.g = 193;
		upc.b = 193;
		upc.a = 255;

		mihc.r = 61;
		mihc.g = 233;
		mihc.b = 255;
		mihc.a = 255;

		miwhc.r = 0;
		miwhc.g = 0;
		miwhc.b = 0;
		miwhc.a = 0;
		
		mhebc.r = 61;
		mhebc.g = 233;
		mhebc.b = 255;
		mhebc.a = 255;
	}
	if (strcmp(tw_theme,"2") == 0) // RED
	{
		htc.r = 128;
		htc.g = 0;
		htc.b = 0;
		htc.a = 255;

		mtc.r = 255;
		mtc.g = 0;
		mtc.b = 0;
		mtc.a = 255;

		upc.r = 255;
		upc.g = 0;
		upc.b = 0;
		upc.a = 255;

		mihc.r = 255;
		mihc.g = 0;
		mihc.b = 0;
		mihc.a = 255;

		miwhc.r = 255;
		miwhc.g = 255;
		miwhc.b = 255;
		miwhc.a = 255;
		
		mhebc.r = 255;
		mhebc.g = 0;
		mhebc.b = 0;
		mhebc.a = 255;
	}
	if (strcmp(tw_theme,"3") == 0) // GOOGLE
	{
		//HEADER_TEXT_COLOR
		htc.r = 255;
		htc.g = 255;
		htc.b = 255;
		htc.a = 255;
		//MENU_ITEM_COLOR
		mtc.r = 64;
		mtc.g = 96;
		mtc.b = 255;
		mtc.a = 255;
		//UI_PRINT_COLOR
		upc.r = 255;
		upc.g = 255;
		upc.b = 0;
		upc.a = 255;
		//MENU_ITEM_HIGHLIGHT_COLOR
		mihc.r = 64;
		mihc.g = 96;
		mihc.b = 255;
		mihc.a = 255;
		//MENU_ITEM_WHEN_HIGHLIGHTED_COLOR
		miwhc.r = 0;
		miwhc.g = 0;
		miwhc.b = 0;
		miwhc.a = 0;
		//MENU_HORIZONTAL_END_BAR_COLOR
		mhebc.r = 0;
		mhebc.g = 0;
		mhebc.b = 0;
		mhebc.a = 0;
	}
	if (strcmp(tw_theme,"4") == 0) // JesusFreke
	{
		//HEADER_TEXT_COLOR
		htc.r = 255;
		htc.g = 255;
		htc.b = 255;
		htc.a = 255;
		//MENU_ITEM_COLOR
		mtc.r = 61;
		mtc.g = 96;
		mtc.b = 255;
		mtc.a = 255;
		//UI_PRINT_COLOR
		upc.r = 255;
		upc.g = 255;
		upc.b = 0;
		upc.a = 255;
		//MENU_ITEM_HIGHLIGHT_COLOR
		mihc.r = 61;
		mihc.g = 96;
		mihc.b = 255;
		mihc.a = 255;
		//MENU_ITEM_WHEN_HIGHLIGHTED_COLOR
		miwhc.r = 0;
		miwhc.g = 0;
		miwhc.b = 0;
		miwhc.a = 0;
		//MENU_HORIZONTAL_END_BAR_COLOR
		mhebc.r = 61;
		mhebc.g = 96;
		mhebc.b = 255;
		mhebc.a = 255;
	}
	if (strcmp(tw_theme,"5") == 0) // HTC
	{
		//HEADER_TEXT_COLOR
		htc.r = 255;
		htc.g = 255;
		htc.b = 255;
		htc.a = 255;
		//MENU_ITEM_COLOR
		mtc.r = 153;
		mtc.g = 207;
		mtc.b = 23;
		mtc.a = 255;
		//UI_PRINT_COLOR
		upc.r = 153;
		upc.g = 153;
		upc.b = 153;
		upc.a = 255;
		//MENU_ITEM_HIGHLIGHT_COLOR
		mihc.r = 153;
		mihc.g = 153;
		mihc.b = 153;
		mihc.a = 255;
		//MENU_ITEM_WHEN_HIGHLIGHTED_COLOR
		miwhc.r = 153;
		miwhc.g = 207;
		miwhc.b = 23;
		miwhc.a = 255;
		//MENU_HORIZONTAL_END_BAR_COLOR
		mhebc.r = 153;
		mhebc.g = 207;
		mhebc.b = 23;
		mhebc.a = 255;
	}
	if (strcmp(tw_theme,"6") == 0) // FABULOUS
	{
		//HEADER_TEXT_COLOR
		htc.r = 195;
		htc.g = 77;
		htc.b = 255;
		htc.a = 255;

		//MENU_ITEM_COLOR
		mtc.r = 77;
		mtc.g = 106;
		mtc.b = 255;
		mtc.a = 255;

		//UI_PRINT_COLOR
		upc.r = 106;
		upc.g = 255;
		upc.b = 77;
		upc.a = 255;

		//MENU_ITEM_HIGHLIGHT_COLOR
		mihc.r = 255;
		mihc.g = 77;
		mihc.b = 136;
		mihc.a = 125;

		//MENU_ITEM_WHEN_HIGHLIGHTED_COLOR
		miwhc.r = 255;
		miwhc.g = 136;
		miwhc.b = 77;
		miwhc.a = 255;

		//MENU_HORIZONTAL_END_BAR_COLOR
		mhebc.r = 77; //195
		mhebc.g = 255; //255
		mhebc.b = 195; //77
		mhebc.a = 255;
	}
	if (strcmp(tw_theme,"7") == 0) // PURPLE SHIFT
	{
        	//HEADER_TEXT_COLOR
		htc.r = 255;
		htc.g = 0;
		htc.b = 242;
		htc.a = 255;

		//MENU_ITEM_COLOR
		mtc.r = 140;
		mtc.g = 0;
		mtc.b = 255;
		mtc.a = 255;

		//UI_PRINT_COLOR
		upc.r = 195;
		upc.g = 122;
		upc.b = 255;
		upc.a = 255;

		//MENU_ITEM_HIGHLIGHT_COLOR
		mihc.r = 140;
		mihc.g = 0;
		mihc.b = 255;
		mihc.a = 255;

		//MENU_ITEM_WHEN_HIGHLIGHTED_COLOR
		miwhc.r = 0;
		miwhc.g = 0;
		miwhc.b = 0;
		miwhc.a = 0;

		//MENU_HORIZONTAL_END_BAR_COLOR
		mhebc.r = 140;
		mhebc.g = 0;
		mhebc.b = 255;
		mhebc.a = 255;
	}
	if (strcmp(tw_theme,"8") == 0) // GREYBALLER
	{
        	//HEADER_TEXT_COLOR
		htc.r = 138;
		htc.g = 138;
		htc.b = 138;
		htc.a = 255;

		//MENU_ITEM_COLOR
		mtc.r = 163;
		mtc.g = 163;
		mtc.b = 163;
		mtc.a = 255;

		//UI_PRINT_COLOR
		upc.r = 163;
		upc.g = 163;
		upc.b = 163;
		upc.a = 255;

		//MENU_ITEM_HIGHLIGHT_COLOR
		mihc.r = 138;
		mihc.g = 138;
		mihc.b = 138;
		mihc.a = 255;

		//MENU_ITEM_WHEN_HIGHLIGHTED_COLOR
		miwhc.r = 99;
		miwhc.g = 99;
		miwhc.b = 99;
		miwhc.a = 255;

		//MENU_HORIZONTAL_END_BAR_COLOR
		mhebc.r = 138;
		mhebc.g = 138;
		mhebc.b = 138;
		mhebc.a = 255;
	}
	if (strcmp(tw_theme,"9") == 0) // TRIPPY
	{
		//HEADER_TEXT_COLOR
		htc.r = 255;
		htc.g = 77;
		htc.b = 195;
		htc.a = 255;

		//MENU_ITEM_COLOR
		mtc.r = 255;
		mtc.g = 136;
		mtc.b = 77;
		mtc.a = 255;

		//UI_PRINT_COLOR
		upc.r = 106;
		upc.g = 255;
		upc.b = 77;
		upc.a = 255;

		//MENU_ITEM_HIGHLIGHT_COLOR
		mihc.r = 136;
		mihc.g = 77;
		mihc.b = 255;
		mihc.a = 125;

		//MENU_ITEM_WHEN_HIGHLIGHTED_COLOR
		miwhc.r = 77;
		miwhc.g = 106;
		miwhc.b = 255;
		miwhc.a = 255;

		//MENU_HORIZONTAL_END_BAR_COLOR
		mhebc.r = 195;
		mhebc.g = 255;
		mhebc.b = 77;
		mhebc.a = 255;
	}
	if (strcmp(tw_theme,"10") == 0) // SHIFTY BASTARD
	{
		//HEADER_TEXT_COLOR
		htc.r = 224;
		htc.g = 0;
		htc.b = 0;
		htc.a = 255;

		//MENU_ITEM_COLOR
		mtc.r = 174;
		mtc.g = 0;
		mtc.b = 0;
		mtc.a = 255;

		//UI_PRINT_COLOR
		upc.r = 132;
		upc.g = 0;
		upc.b = 0;
		upc.a = 255;

		//MENU_ITEM_HIGHLIGHT_COLOR
		mihc.r = 207;
		mihc.g = 207;
		mihc.b = 207;
		mihc.a = 125;

		//MENU_ITEM_WHEN_HIGHLIGHTED_COLOR
		miwhc.r = 0;
		miwhc.g = 0;
		miwhc.b = 0;
		miwhc.a = 255;

		//MENU_HORIZONTAL_END_BAR_COLOR
		mhebc.r = 207;
		mhebc.g = 207;
		mhebc.b = 207;
		mhebc.a = 255;
	}
	
	if (strcmp(tw_theme,"11") == 0) // MYN_WARM
	{
		//HEADER_TEXT_COLOR
		htc.r = 250;
		htc.g = 226;
		htc.b = 189;
		htc.a = 255;

		//MENU_ITEM_COLOR
		mtc.r = 255;
		mtc.g = 153;
		mtc.b = 0;
		mtc.a = 255;

		//UI_PRINT_COLOR
		upc.r = 255;
		upc.g = 153;
		upc.b = 0;
		upc.a = 255;

		//MENU_ITEM_HIGHLIGHT_COLOR
		mihc.r = 255;
		mihc.g = 153;
		mihc.b = 0;
		mihc.a = 125;

		//MENU_ITEM_WHEN_HIGHLIGHTED_COLOR
		miwhc.r = 0;
		miwhc.g = 0;
		miwhc.b = 0;
		miwhc.a = 255;

		//MENU_HORIZONTAL_END_BAR_COLOR
		mhebc.r = 238;
		mhebc.g = 192;
		mhebc.b = 125;
		mhebc.a = 255;
	}
}
