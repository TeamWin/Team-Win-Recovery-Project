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
#include <fcntl.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>


extern "C" {
#include "../common.h"
#include "../roots.h"
#include "../minui/minui.h"
#include "../recovery_ui.h"
#include "../minzip/Zip.h"
#include <pixelflinger/pixelflinger.h>
}

#include "rapidxml.hpp"
#include "objects.hpp"


#include "curtain.h"
#include "watermark.h"


const static int CURTAIN_RATE = 16;
const static int CURTAIN_FADE = 32;

using namespace rapidxml;

// Global values
static gr_surface gCurtain = NULL;
static gr_surface gWatermark = NULL;
static int gGuiInitialized = 0;
static int gForceRender = 0;
static int gNoAnimation = 0;

static int gRecorder = -1;

extern "C" void gr_write_frame_to_file(int fd);
extern "C" void gr_watermark(gr_surface source, int sx, int sy, int w, int h, int dx, int dy);

void flip(void)
{
    if (gRecorder != -1)
    {
        timespec time;
        clock_gettime(CLOCK_MONOTONIC, &time);
        write(gRecorder, &time, sizeof(timespec));
        gr_write_frame_to_file(gRecorder);
    }
    if (gWatermark)
        gr_watermark(gWatermark, 0, 0, gr_get_width(gWatermark), gr_get_height(gWatermark), 0, 0);
    gr_flip();
    return;
}

void rapidxml::parse_error_handler(const char *what, void *where)
{
    fprintf(stderr, "Parser error: %s\n", what);
    fprintf(stderr, "  Start of string: %s\n", (char*) where);
    abort();
}

static void curtainSet()
{
    gr_color(0, 0, 0, 255);
    gr_fill(0, 0, gr_fb_width(), gr_fb_height());
    gr_blit(gCurtain, 0, 0, gr_get_width(gCurtain), gr_get_height(gCurtain), 0, 0);
    gr_flip();
    return;
}

static void curtainRaise(gr_surface surface)
{
    int sy = 0;
    int h = gr_get_height(gCurtain) - 1;
    int w = gr_get_width(gCurtain);
    int fy = 1;

    int msw = gr_get_width(surface);
    int msh = gr_get_height(surface);

    if (gNoAnimation == 0)
    {
        for (; h > 0; h -= CURTAIN_RATE, sy += CURTAIN_RATE, fy += CURTAIN_RATE)
        {
            gr_blit(surface, 0, 0, msw, msh, 0, 0);
            gr_blit(gCurtain, 0, sy, w, h, 0, 0);
            gr_flip();
        }
    }
    gr_blit(surface, 0, 0, msw, msh, 0, 0);
    flip();
    return;
}

void curtainClose()
{
    int w = gr_get_width(gCurtain);
    int h = 1;
    int sy = gr_get_height(gCurtain) - 1;
    int fbh = gr_fb_height();

    if (gNoAnimation == 0)
    {
        for (; h < fbh; h += CURTAIN_RATE, sy -= CURTAIN_RATE)
        {
            gr_blit(gCurtain, 0, sy, w, h, 0, 0);
            gr_flip();
        }
        gr_blit(gCurtain, 0, 0, gr_get_width(gCurtain), gr_get_height(gCurtain), 0, 0);
        gr_flip();
    
        close(gRecorder);
    
        int fade;
        for (fade = 16; fade < 255; fade += CURTAIN_FADE)
        {
            gr_blit(gCurtain, 0, 0, gr_get_width(gCurtain), gr_get_height(gCurtain), 0, 0);
            gr_color(0, 0, 0, fade);
            gr_fill(0, 0, gr_fb_width(), gr_fb_height());
            gr_flip();
        }
        gr_color(0, 0, 0, 255);
        gr_fill(0, 0, gr_fb_width(), gr_fb_height());
        gr_flip();
    }
    return;
}

static void *input_thread(void *cookie)
{
    int drag = 0;

    for (;;) {

        // wait for the next event
        struct input_event ev;
        int state = 0;

        ev_get(&ev, 0);

        if (ev.type == EV_ABS)
        {
            int x, y;

            x = ev.value >> 16;
            y = ev.value & 0xFFFF;

            if (ev.code == 0)
            {
                if (state == 0)
                {
//                    LOGE("TOUCH_RELEASE: %d,%d\n", x, y);
                    PageManager::NotifyTouch(TOUCH_RELEASE, x, y);
                }
                state = 0;
                drag = 0;
            }
            else
            {
                if (!drag)
                {
//                    LOGE("TOUCH_START: %d,%d\n", x, y);
                    if (PageManager::NotifyTouch(TOUCH_START, x, y) > 0)
                        state = 1;
                    drag = 1;
                }
                else
                {
                    if (state == 0)
                    {
//                        LOGE("TOUCH_DRAG: %d,%d\n", x, y);
                        if (PageManager::NotifyTouch(TOUCH_DRAG, x, y) > 0)
                            state = 1;
                    }
                }
            }
        }
        else if (ev.type == EV_KEY)
        {
            // Handle key-press here
//            LOGE("TOUCH_KEY: %d\n", ev.code);
            PageManager::NotifyKey(ev.code);
        }
    }
    return NULL;
}

timespec timespec_diff(timespec& start, timespec& end)
{
	timespec temp;
	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp;
}

// This special function will return immediately the first time, but then
// always returns 1/30th of a second (or immediately if called later) from
// the last time it was called
static void loopTimer(void)
{
    static timespec lastCall;
    static int initialized = 0;

    if (!initialized)
    {
        clock_gettime(CLOCK_MONOTONIC, &lastCall);
        initialized = 1;
        return;
    }

    do
    {
        timespec curTime;
        clock_gettime(CLOCK_MONOTONIC, &curTime);

        timespec diff = timespec_diff(lastCall, curTime);

        // This is really 30 times per second
        if (diff.tv_sec || diff.tv_nsec > 33333333)
        {
            lastCall = curTime;
            return;
        }

        // We need to sleep some period time microseconds
        unsigned int sleepTime = 33333 - (diff.tv_nsec / 1000);
        usleep(sleepTime);
    } while(1);
    return;
}

static int runPages(void)
{
    // Raise the curtain
    if (gCurtain != NULL)
    {
        gr_surface surface;

        PageManager::Render();
        gr_get_surface(&surface);
        curtainRaise(surface);
        gr_free_surface(surface);
    }

    for (;;)
    {
        loopTimer();

        if (!gForceRender)
        {
            int ret;

            ret = PageManager::Update();
            if (ret > 1 || (ret > 0 && gWatermark != NULL))
                PageManager::Render();

            if (ret > 0)
                flip();

            if (ret < 0)
                LOGE("An update has failed.\n");
        }
        else
        {
            gForceRender = 0;
            PageManager::Render();
            flip();
        }
    }
    return 0;
}

int gui_changePage(std::string newPage)
{
    LOGI("Changing page to %s\n", newPage.c_str());
    PageManager::ChangePage(newPage);
    gForceRender = 1;
    return 0;
}

int gui_changePackage(std::string newPackage)
{
    PageManager::SelectPackage(newPackage);
    gForceRender = 1;
    return 0;
}

extern "C" int gui_init()
{
    int fd;

    gr_init();
    ev_init();

    // We need to write out the curtain and watermark blobs
    if (sizeof(gCurtainBlob) > 32)
    {
        fd = open("/tmp/extract.png", O_CREAT | O_WRONLY | O_TRUNC);
        if (fd < 0)
            return 0;
    
        write(fd, gCurtainBlob, sizeof(gCurtainBlob));
        close(fd);
    
        if (res_create_surface("/tmp/extract.png", &gCurtain))
        {
            return -1;
        }
    }
    else
    {
        gNoAnimation = 1;
        if (res_create_surface("bootup", &gCurtain))
            return 0;
    }

    if (sizeof(gWatermarkBlob) > 32)
    {
        fd = open("/tmp/extract.png", O_CREAT | O_WRONLY | O_TRUNC);
        if (fd < 0)
            return 0;

        write(fd, gWatermarkBlob, sizeof(gWatermarkBlob));
        close(fd);
        res_create_surface("/tmp/extract.png", &gWatermark);
    }
    unlink("/tmp/extract.png");

    curtainSet();
    return 0;
}

extern "C" int gui_loadResources()
{
    // Make sure the sdcard is mounted before we continue
    if (ensure_path_mounted("/sdcard") < 0)
    {
        usleep(500000);
        ensure_path_mounted("/sdcard");
    }

//    unlink("/sdcard/video.last");
//    rename("/sdcard/video.bin", "/sdcard/video.last");
//    gRecorder = open("/sdcard/video.bin", O_CREAT | O_WRONLY);

    if (PageManager::LoadPackage("TWRP", "/sdcard/TWRP/theme/ui.zip"))
    {
        if (PageManager::LoadPackage("TWRP", "/res/ui.xml"))
        {
            LOGE("Failed to load base packages.\n");
            goto error;
        }
    }

    // Set the default package
    PageManager::SelectPackage("TWRP");

    gGuiInitialized = 1;
    return 0;

error:
    LOGE("An internal error has occurred.\n");
    gGuiInitialized = 0;
    return -1;
}

extern "C" int gui_start()
{
    if (!gGuiInitialized)   return -1;

    // Start by spinning off an input handler.
    pthread_t t;
    pthread_create(&t, NULL, input_thread, NULL);

    return runPages();
}

