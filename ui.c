/*
 * Copyright (C) 2007 The Android Open Source Project
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

#include <linux/input.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "tw_reboot.h"
#include "minuitwrp/minui.h"
#include "recovery_ui.h"
#include "themes.h"
#include "data.h"

#define MAX_COLS 96
#define MAX_ROWS 50

#define MENU_MAX_COLS 50
#define MENU_MAX_ROWS 500

#define CHAR_WIDTH 10
#define CHAR_HEIGHT 18

#define PROGRESSBAR_INDETERMINATE_STATES 6
#define PROGRESSBAR_INDETERMINATE_FPS 15

void gui_print(const char *fmt, ...);
void gui_print_overwrite(const char *fmt, ...);

static pthread_mutex_t gUpdateMutex = PTHREAD_MUTEX_INITIALIZER;
static gr_surface gBackgroundIcon[NUM_BACKGROUND_ICONS];
static gr_surface gProgressBarIndeterminate[PROGRESSBAR_INDETERMINATE_STATES];
static gr_surface gProgressBarEmpty;
static gr_surface gProgressBarFill;
static int gUiInitialized = 0;

static const struct { gr_surface* surface; const char *name; } BITMAPS[] = {
    { &gBackgroundIcon[BACKGROUND_ICON_INSTALLING], "icon_installing" },
    { &gBackgroundIcon[BACKGROUND_ICON_ERROR],      "icon_error" },
    { &gBackgroundIcon[BACKGROUND_ICON_MAIN],       "icon_main" },
    { &gBackgroundIcon[BACKGROUND_ICON_WIPE],       "icon_wipe" },
    { &gBackgroundIcon[BACKGROUND_ICON_WIPE_CHOOSE],"icon_wipe_choose" },
    { &gBackgroundIcon[BACKGROUND_ICON_FLASH_ZIP],  "icon_flash_zip" },
    { &gBackgroundIcon[BACKGROUND_ICON_NANDROID],  "icon_nandroid" },
    { &gProgressBarIndeterminate[0],    "indeterminate1" },
    { &gProgressBarIndeterminate[1],    "indeterminate2" },
    { &gProgressBarIndeterminate[2],    "indeterminate3" },
    { &gProgressBarIndeterminate[3],    "indeterminate4" },
    { &gProgressBarIndeterminate[4],    "indeterminate5" },
    { &gProgressBarIndeterminate[5],    "indeterminate6" },
    { &gProgressBarEmpty,               "progress_empty" },
    { &gProgressBarFill,                "progress_fill" },
    { NULL,                             NULL },
};

static gr_surface gCurrentIcon = NULL;

static enum ProgressBarType {
    PROGRESSBAR_TYPE_NONE,
    PROGRESSBAR_TYPE_INDETERMINATE,
    PROGRESSBAR_TYPE_NORMAL,
} gProgressBarType = PROGRESSBAR_TYPE_NONE;

// Progress bar scope of current operation
static float gProgressScopeStart = 0, gProgressScopeSize = 0, gProgress = 0;
static time_t gProgressScopeTime, gProgressScopeDuration;

// Set to 1 when both graphics pages are the same (except for the progress bar)
static int gPagesIdentical = 0;

// Log text overlay, displayed when a magic key is pressed
static char text[MAX_ROWS][MAX_COLS];
static int text_cols = 0, text_rows = 0;
static int text_col = 0, text_row = 0, text_top = 0;
static int show_text = 1;

static char menu[MENU_MAX_ROWS][MENU_MAX_COLS]; //allows menu to hold more than stock default 32 rows
static int show_menu = 0;
static int menu_top = 0, menu_items = 0, menu_sel = 0;
static int menu_show_start = 0; ////line count of where the menu starts to be drawn

// Key event input queue
static pthread_mutex_t key_queue_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t key_queue_cond = PTHREAD_COND_INITIALIZER;
static int key_queue[256], key_queue_len = 0;
static volatile char key_pressed[KEY_MAX + 1];

// Clear the screen and draw the currently selected background icon (if any).
// Should only be called with gUpdateMutex locked.
static void draw_background_locked(gr_surface icon)
{
    gPagesIdentical = 0;
    gr_color(0, 0, 0, 255);
    gr_fill(0, 0, gr_fb_width(), gr_fb_height());

    if (icon) {
        int iconWidth = gr_get_width(icon);
        int iconHeight = gr_get_height(icon);
        int iconX = (gr_fb_width() - iconWidth) / 2;
        int iconY = (gr_fb_height() - iconHeight) / 2;
        gr_blit(icon, 0, 0, iconWidth, iconHeight, iconX, iconY);
    }
}

// Draw the progress bar (if any) on the screen.  Does not flip pages.
// Should only be called with gUpdateMutex locked.
static void draw_progress_locked()
{
    if (gProgressBarType == PROGRESSBAR_TYPE_NONE) return;

    int iconHeight = gr_get_height(gBackgroundIcon[BACKGROUND_ICON_INSTALLING]);
    int width = gr_get_width(gProgressBarEmpty);
    int height = gr_get_height(gProgressBarEmpty);

    int dx = (gr_fb_width() - width)/2;
    int dy = (3*gr_fb_height() + iconHeight + 425 - 2*height)/4;

    // Erase behind the progress bar (in case this was a progress-only update)
    gr_color(0, 0, 0, 255);
    gr_fill(dx, dy, width, height);

    if (gProgressBarType == PROGRESSBAR_TYPE_NORMAL) {
        float progress = gProgressScopeStart + gProgress * gProgressScopeSize;
        int pos = (int) (progress * width);

        if (pos > 0) {
          gr_blit(gProgressBarFill, 0, 0, pos, height, dx, dy);
        }
        if (pos < width-1) {
          gr_blit(gProgressBarEmpty, pos, 0, width-pos, height, dx+pos, dy);
        }
    }

    if (gProgressBarType == PROGRESSBAR_TYPE_INDETERMINATE) {
        static int frame = 0;
        gr_blit(gProgressBarIndeterminate[frame], 0, 0, width, height, dx, dy);
        frame = (frame + 1) % PROGRESSBAR_INDETERMINATE_STATES;
    }
}

static void draw_text_line(int row, const char* t) {
  if (t[0] != '\0') {
    gr_text(0, row*CHAR_HEIGHT+1, t);
  }
}

//setup up all our fancy colors in one convenient location
//#define HEADER_TEXT_COLOR 255, 255, 255, 255 //white
//#define MENU_ITEM_COLOR 0, 255, 0, 255 //teamwin green
//#define UI_PRINT_COLOR 0, 255, 0, 255 //teamwin green
//#define MENU_ITEM_HIGHLIGHT_COLOR 0, 255, 0, 255 //teamwin green
//#define MENU_ITEM_WHEN_HIGHLIGHTED_COLOR 0, 0, 0, 0 //black
//#define MENU_HORIZONTAL_END_BAR_COLOR 0, 255, 0, 255 //teamwin green

// Redraw everything on the screen.  Does not flip pages.
// Should only be called with gUpdateMutex locked.
// joeykrim-all the LOGI help me keep my sanity
static void draw_screen_locked(void)
{
    draw_background_locked(gCurrentIcon);
    draw_progress_locked();

    if (show_text) {
        gr_color(0, 0, 0, 160);
        gr_fill(0, 0, gr_fb_width(), gr_fb_height());

        int i = 0, j = 0;
        int k = menu_top + 1; //counter for bottom horizontal text line location
        if (show_menu) {

        //LOGI("text_rows: %i\n", text_rows);
        //LOGI("menu_items: %i\n", menu_items);
        //LOGI("menu_top: %i\n", menu_top);
        //LOGI("menu_show_start: %i\n", menu_show_start);
            //menu line item selection highlight draws
            gr_color(mihc.r, mihc.g, mihc.b, mihc.a);
            gr_fill(0, (menu_top + menu_sel - menu_show_start+1) * CHAR_HEIGHT,
                    gr_fb_width(), CHAR_HEIGHT+1);

            //draw semi-static headers
            for (i = 0; i < menu_top; ++i) {
                gr_color(htc.r, htc.g, htc.b, htc.a);
                draw_text_line(i, menu[i]);
                //LOGI("Semi-static headers internal counter i: %i\n", i);
            }

            //LOGI("Drawing horizontal start bar at k and k = %i\n", k);
            gr_color(mhebc.r, mhebc.g, mhebc.b, mhebc.a);
            //draws horizontal line at bottom of the menu
            gr_fill(0, (k-1)*CHAR_HEIGHT+CHAR_HEIGHT/2-1,
                    gr_fb_width(), 2);

            //adjust counter for current position of selection and menu display starting point
            if (menu_items - menu_show_start + menu_top >= text_rows){
                j = text_rows - menu_top;
                //LOGI("j = text_rows - mneu_top and j = %i\n", j);
           } else {
                j = menu_items - menu_show_start;
                //LOGI("j = mneu_items - menu_show_start and j = %i\n", j);
           }
            //LOGI("outside draw menu items for loop and i goes until limit. limit-menu_show_start + menu_top + j = %i\n", menu_show_start + menu_top + j);
            //draw menu items dynamically based on current menu starting position, menu selection point and headers
             for (i = menu_show_start + menu_top; i < (menu_show_start + menu_top + j); ++i) {
                //LOGI("inside draw menu items for loop and i = %i\n", i);
                if (i == menu_top + menu_sel) {
                    gr_color(miwhc.r, miwhc.g, miwhc.b, miwhc.a);
                    //LOGI("draw_text_line -menu_item_when_highlighted_color- at i + 1= %i\n", i+1);
                    draw_text_line(i - menu_show_start +1, menu[i]);
                } else {
                    gr_color(mtc.r, mtc.g, mtc.b, mtc.a);
                    //LOGI("draw_text_line -menu_item_color- at i + 1= %i\n", i+1);
                    draw_text_line(i - menu_show_start +1, menu[i]);
                }
                //LOGI("inside draw menu items for loop and k = %i\n", k);
                k++;
            }
            //LOGI("drawing horizontal end bar at k and k = %i\n", k);
            gr_color(mhebc.r, mhebc.g, mhebc.b, mhebc.a);
            //draws horizontal line at bottom of the menu
            gr_fill(0, k*CHAR_HEIGHT+CHAR_HEIGHT/2-1,
                    gr_fb_width(), 2);
        }

        k++; //keep ui_print below menu items display
        gr_color(upc.r, upc.g, upc.b, upc.a); //called by at least ui_print
        for (; k < text_rows; ++k) {
            draw_text_line(k, text[(k+text_top) % text_rows]);
        }
    }
}

// Redraw everything on the screen and flip the screen (make it visible).
// Should only be called with gUpdateMutex locked.
static void update_screen_locked(void)
{
    if (!gUiInitialized)    return;
    draw_screen_locked();
    gr_flip();
}

// Updates only the progress bar, if possible, otherwise redraws the screen.
// Should only be called with gUpdateMutex locked.
static void update_progress_locked(void)
{
    if (!gUiInitialized)    return;

    if (show_text || !gPagesIdentical) {
        draw_screen_locked();    // Must redraw the whole screen
        gPagesIdentical = 1;
    }
#ifndef BOARD_HAS_FLIPPED_SCREEN
    else {
        draw_progress_locked();  // Draw only the progress bar
    }
#endif
    gr_flip();
}

// Keeps the progress bar updated, even when the process is otherwise busy.
static void *progress_thread(void *cookie)
{
    for (;;) {
        usleep(1000000 / PROGRESSBAR_INDETERMINATE_FPS);
        pthread_mutex_lock(&gUpdateMutex);

        // update the progress bar animation, if active
        // skip this if we have a text overlay (too expensive to update)
        if (gProgressBarType == PROGRESSBAR_TYPE_INDETERMINATE && !show_text) {
            update_progress_locked();
        }

        // move the progress bar forward on timed intervals, if configured
        int duration = gProgressScopeDuration;
        if (gProgressBarType == PROGRESSBAR_TYPE_NORMAL && duration > 0) {
            int elapsed = time(NULL) - gProgressScopeTime;
            float progress = 1.0 * elapsed / duration;
            if (progress > 1.0) progress = 1.0;
            if (progress > gProgress) {
                gProgress = progress;
                update_progress_locked();
            }
        }

        pthread_mutex_unlock(&gUpdateMutex);
    }
    return NULL;
}

// Reads input events, handles special hot keys, and adds to the key queue.
static void *input_thread(void *cookie)
{
    int rel_sum = 0;
    int fake_key = 0;
    for (;;) {
        // wait for the next key event
        struct input_event ev;
        do {
            ev_get(&ev, 0);

            if (ev.type == EV_SYN) {
                continue;
            } else if (ev.type == EV_REL) {
                if (ev.code == REL_Y) {
                    // accumulate the up or down motion reported by
                    // the trackball.  When it exceeds a threshold
                    // (positive or negative), fake an up/down
                    // key event.
                    rel_sum += ev.value;

                    if (rel_sum > 3) {
                        fake_key = 1;
                        ev.type = EV_KEY;
                        ev.code = KEY_DOWN;
                        ev.value = 1;
                        rel_sum = 0;
                    } else if (rel_sum < -3) {
                        fake_key = 1;
                        ev.type = EV_KEY;
                        ev.code = KEY_UP;
                        ev.value = 1;
                        rel_sum = 0;
                    }
                }
            } else {
                rel_sum = 0;
            }
        } while (ev.type != EV_KEY || ev.code > KEY_MAX);

        pthread_mutex_lock(&key_queue_mutex);
        if (!fake_key) {
            // our "fake" keys only report a key-down event (no
            // key-up), so don't record them in the key_pressed
            // table.
            key_pressed[ev.code] = ev.value;
        }
        fake_key = 0;
        const int queue_max = sizeof(key_queue) / sizeof(key_queue[0]);
        if (ev.value > 0 && key_queue_len < queue_max) {
            key_queue[key_queue_len++] = ev.code;
            pthread_cond_signal(&key_queue_cond);
        }
        pthread_mutex_unlock(&key_queue_mutex);

        if (ev.value > 0 && device_toggle_display(key_pressed, ev.code)) {
            pthread_mutex_lock(&gUpdateMutex);
            show_text = !show_text;
            update_screen_locked();
            pthread_mutex_unlock(&gUpdateMutex);
        }

        if (ev.value > 0 && device_reboot_now(key_pressed, ev.code)) {
            tw_reboot(rb_system);
        }
    }
    return NULL;
}

void ui_init(void)
{
    gr_init();
    ev_init();

    text_col = text_row = 0;
    text_rows = gr_fb_height() / CHAR_HEIGHT;
    if (text_rows > MAX_ROWS) text_rows = MAX_ROWS;
    text_top = 1;

    text_cols = gr_fb_width() / CHAR_WIDTH;
    if (text_cols > MAX_COLS - 1) text_cols = MAX_COLS - 1;

    int i;
    for (i = 0; BITMAPS[i].name != NULL; ++i) {
        int result = res_create_surface(BITMAPS[i].name, BITMAPS[i].surface);
        if (result < 0) {
            if (result == -2) {
                LOGI("Bitmap %s missing header\n", BITMAPS[i].name);
            } else {
                LOGE("Missing bitmap %s\n(Code %d)\n", BITMAPS[i].name, result);
            }
            *BITMAPS[i].surface = NULL;
        }
    }

    pthread_t t;
    pthread_create(&t, NULL, progress_thread, NULL);
    pthread_create(&t, NULL, input_thread, NULL);

    gUiInitialized = 1;
}

void ui_set_background(int icon)
{
    pthread_mutex_lock(&gUpdateMutex);
    gCurrentIcon = gBackgroundIcon[icon];
    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_show_indeterminate_progress()
{
    pthread_mutex_lock(&gUpdateMutex);
    if (gProgressBarType != PROGRESSBAR_TYPE_INDETERMINATE) {
        gProgressBarType = PROGRESSBAR_TYPE_INDETERMINATE;
        update_progress_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_show_progress(float portion, int seconds)
{
    DataManager_SetFloatValue("ui_progress_portion", portion * 100.0);
    DataManager_SetIntValue("ui_progress_frames", seconds * 30);

    pthread_mutex_lock(&gUpdateMutex);
    gProgressBarType = PROGRESSBAR_TYPE_NORMAL;
    gProgressScopeStart += gProgressScopeSize;
    gProgressScopeSize = portion;
    gProgressScopeTime = time(NULL);
    gProgressScopeDuration = seconds;
    gProgress = 0;
    update_progress_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_set_progress(float fraction)
{
    DataManager_SetFloatValue("ui_progress", (float) (fraction * 100.0));

    pthread_mutex_lock(&gUpdateMutex);
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;
    if (gProgressBarType == PROGRESSBAR_TYPE_NORMAL && fraction > gProgress) {
        // Skip updates that aren't visibly different.
        int width = gr_get_width(gProgressBarIndeterminate[0]);
        float scale = width * gProgressScopeSize;
        if ((int) (gProgress * scale) != (int) (fraction * scale)) {
            gProgress = fraction;
            update_progress_locked();
        }
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_reset_progress()
{
    pthread_mutex_lock(&gUpdateMutex);
    gProgressBarType = PROGRESSBAR_TYPE_NONE;
    gProgressScopeStart = gProgressScopeSize = 0;
    gProgressScopeTime = gProgressScopeDuration = 0;
    gProgress = 0;
    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_print(const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, 512, fmt, ap);
    va_end(ap);

    fputs(buf, stdout);

    gui_print("%s", buf);

    // This can get called before ui_init(), so be careful.
    pthread_mutex_lock(&gUpdateMutex);
    if (text_rows > 0 && text_cols > 0) {
        char *ptr;
        for (ptr = buf; *ptr != '\0'; ++ptr) {
            if (*ptr == '\n' || text_col >= text_cols) {
                text[text_row][text_col] = '\0';
                text_col = 0;
                text_row = (text_row + 1) % text_rows;
                if (text_row == text_top) text_top = (text_top + 1) % text_rows;
            }
            if (*ptr != '\n') text[text_row][text_col++] = *ptr;
        }
        text[text_row][text_col] = '\0';
        update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_print_overwrite(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, (text_cols - 1), fmt, ap);
    va_end(ap);
    //LOGI("ui_print_overwrite - starting text row %i\n", text_row);
    fputs(buf, stdout);

    gui_print_overwrite("%s", buf);

    text_col = 0;
    // This can get called before ui_init(), so be careful.
    pthread_mutex_lock(&gUpdateMutex);
    if (text_rows > 0 && text_cols > 0) {
        char *ptr;
        for (ptr = buf; *ptr != '\0'; ++ptr) {
            if (*ptr == '\n' || text_col >= text_cols) {
                text[text_row][text_col] = '\0';
                text_col = 0;
                text_row = (text_row + 1) % text_rows;
                if (text_row == text_top) text_top = (text_top + 1) % text_rows;
            }
            if (*ptr != '\n') text[text_row][text_col++] = *ptr;
        }
        text[text_row][text_col] = '\0';
        // had to comment out as it was being thrown into the output
		//LOGI("ui_print_overwrite - ending text row %i    ending text col%i\n", text_row, text_col); 
        update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

void ui_start_menu(char** headers, char** items, int initial_selection) {
    int i;
    pthread_mutex_lock(&gUpdateMutex);
    if (text_rows > 0 && text_cols > 0) {
        for (i = 0; i < text_rows; ++i) {
            if (headers[i] == NULL) break;
            strncpy(menu[i], headers[i], text_cols-1); //copy in all headers[i] to menu[i]
            menu[i][text_cols-1] = '\0';
        }
        menu_top = i; // from this value and previous of i are headers - this item and greater will be menu items[i]
        for (; i < MENU_MAX_ROWS; ++i) {
            if (items[i-menu_top] == NULL) break;
            strncpy(menu[i], items[i-menu_top], text_cols-1);
            menu[i][text_cols-1] = '\0';
        }
        menu_items = i - menu_top; //grand count of how many values are actual menu items to exclude header items
        show_menu = 1; //display the menu
        menu_sel = initial_selection; // set menu_sel to initial_selection for proper display
        if (menu_items <= (text_rows - menu_top)) { // this block of if statements sets menu_show_start to display the right section of the menu based on what item was selected - primarily needed when going back up a level in the menus 
            menu_show_start = 0;
        } else {
            menu_show_start = initial_selection - ((text_rows - menu_top) / 2); // this should place the selected item around the middle of the menu selection area
            if (menu_show_start > (menu_items - (text_rows - menu_top))) {
                menu_show_start = (menu_items - (text_rows - menu_top)); // makes sure that we are displaying as many menu items as possible in case the selected item is at the bottom of a large list of menu items
            }
            if (menu_show_start < 0) {
                menu_show_start = 0; // prevents menu_show_start from being <0 in case we're close to the top of the menu
            }
        }
        update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

int ui_menu_select(int sel) {
    int old_sel;
    pthread_mutex_lock(&gUpdateMutex);
    if (show_menu > 0) {
        old_sel = menu_sel;
        menu_sel = sel;
        if (menu_sel < 0) menu_sel = menu_items + menu_sel;
        if (menu_sel >= menu_items) menu_sel = menu_sel - menu_items;

        //move the display starting point up the screen as the selection moves up
        if (menu_show_start > 0 && menu_sel < menu_show_start) menu_show_start = menu_sel;

        //move display starting point down one past the end of the current screen as the selection moves back end of screen
        if (menu_sel - menu_show_start + menu_top >= text_rows) menu_show_start = menu_sel + menu_top - text_rows + 1;

        sel = menu_sel;
        if (menu_sel != old_sel) update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
    return sel;
}

void ui_end_menu() {
    int i;
    pthread_mutex_lock(&gUpdateMutex);
    if (show_menu > 0 && text_rows > 0 && text_cols > 0) {
        show_menu = 0;
        update_screen_locked();
    }
    pthread_mutex_unlock(&gUpdateMutex);
}

int ui_text_visible()
{
    pthread_mutex_lock(&gUpdateMutex);
    int visible = show_text;
    pthread_mutex_unlock(&gUpdateMutex);
    return visible;
}

void ui_show_text(int visible)
{
    pthread_mutex_lock(&gUpdateMutex);
    show_text = visible;
    update_screen_locked();
    pthread_mutex_unlock(&gUpdateMutex);
}

int ui_wait_key()
{
    pthread_mutex_lock(&key_queue_mutex);
    while (key_queue_len == 0) {
        pthread_cond_wait(&key_queue_cond, &key_queue_mutex);
    }

    int key = key_queue[0];
    memcpy(&key_queue[0], &key_queue[1], sizeof(int) * --key_queue_len);
    pthread_mutex_unlock(&key_queue_mutex);
    return key;
}

int ui_key_pressed(int key)
{
    // This is a volatile static array, don't bother locking
    return key_pressed[key];
}

void ui_clear_key_queue() {
    pthread_mutex_lock(&key_queue_mutex);
    key_queue_len = 0;
    pthread_mutex_unlock(&key_queue_mutex);
}
