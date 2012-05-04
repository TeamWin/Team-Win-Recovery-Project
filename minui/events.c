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

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/poll.h>
#include <limits.h>

#include <linux/input.h>

#include "../common.h"

#include "minui.h"

//#define _EVENT_LOGGING

#define MAX_DEVICES 16

#define VIBRATOR_TIMEOUT_FILE	"/sys/class/timed_output/vibrator/enable"
#define VIBRATOR_TIME_MS	50

#define ABS_MT_POSITION		0x2a	/* Group a set of X and Y */
#define ABS_MT_AMPLITUDE	0x2b	/* Group a set of Z and W */
#define ABS_MT_TOUCH_MAJOR 	0x30
#define ABS_MT_WIDTH_MAJOR 	0x32
#define ABS_MT_POSITION_X       0x35
#define ABS_MT_POSITION_Y       0x36
#define ABS_MT_SLOT		0x39
#define ABS_MT_PRESSURE    	0x3a

#define SYN_REPORT		0x00
#define SYN_MT_REPORT 		0x02

enum {
    DOWN_NOT,
    DOWN_SENT,
    DOWN_RELEASED,
};

struct virtualkey {
    int scancode;
    int centerx, centery;
    int width, height;
};

struct position {
    int x, y;
    int slot;
    int synced;
    struct input_absinfo xi, yi;
};

struct ev {
    struct pollfd *fd;

    struct virtualkey *vks;
    int vk_count;

    char deviceName[64];

    int ignored;

    struct position p, mt_p;
    int down;
};

static struct pollfd ev_fds[MAX_DEVICES];
static struct ev evs[MAX_DEVICES];
static unsigned ev_count = 0;

static inline int ABS(int x) {
    return x<0?-x:x;
}

int vibrate(int timeout_ms)
{
    char str[20];
    int fd;
    int ret;

    fd = open(VIBRATOR_TIMEOUT_FILE, O_WRONLY);
    if (fd < 0)
        return -1;

    ret = snprintf(str, sizeof(str), "%d", timeout_ms);
    ret = write(fd, str, ret);
    close(fd);

    if (ret < 0)
       return -1;

    return 0;
}

/* Returns empty tokens */
static char *vk_strtok_r(char *str, const char *delim, char **save_str)
{
    if(!str)
    {
        if(!*save_str)
            return NULL;

        str = (*save_str) + 1;
    }
    *save_str = strpbrk(str, delim);

    if (*save_str)
        **save_str = '\0';

    return str;
}

static int vk_init(struct ev *e)
{
    char vk_path[PATH_MAX] = "/sys/board_properties/virtualkeys.";
    char vks[2048], *ts = NULL;
    ssize_t len;
    int vk_fd;
    int i;

    e->vk_count = 0;

    len = strlen(vk_path);
    len = ioctl(e->fd->fd, EVIOCGNAME(sizeof(e->deviceName)), e->deviceName);
    if (len <= 0)
    {
        LOGE("Unable to query event object.\n");
        return -1;
    }
#ifdef _EVENT_LOGGING
    LOGI("Event object: %s\n", e->deviceName);
#endif

    // Blacklist these "input" devices
    if (strcmp(e->deviceName, "bma250") == 0)
    {
        e->ignored = 1;
    }

    strcat(vk_path, e->deviceName);

    // Some devices split the keys from the touchscreen
    e->vk_count = 0;
    vk_fd = open(vk_path, O_RDONLY);
    if (vk_fd >= 0)
    {
        len = read(vk_fd, vks, sizeof(vks)-1);
        close(vk_fd);
        if (len <= 0)
            return -1;
    
        vks[len] = '\0';
    
        /* Parse a line like:
            keytype:keycode:centerx:centery:width:height:keytype2:keycode2:centerx2:...
        */
        for (ts = vks, e->vk_count = 1; *ts; ++ts) {
            if (*ts == ':')
                ++e->vk_count;
        }

        if (e->vk_count % 6) {
            LOGW("minui: %s is %d %% 6\n", vk_path, e->vk_count % 6);
        }
        e->vk_count /= 6;
        if (e->vk_count <= 0)
            return -1;

        e->down = DOWN_NOT;
    }

    ioctl(e->fd->fd, EVIOCGABS(ABS_X), &e->p.xi);
    ioctl(e->fd->fd, EVIOCGABS(ABS_Y), &e->p.yi);
    e->p.synced = 0;
#ifdef _EVENT_LOGGING
    LOGI("EV: ST minX: %d  maxX: %d  minY: %d  maxY: %d\n", e->p.xi.minimum, e->p.xi.maximum, e->p.yi.minimum, e->p.yi.maximum);
#endif

    ioctl(e->fd->fd, EVIOCGABS(ABS_MT_POSITION_X), &e->mt_p.xi);
    ioctl(e->fd->fd, EVIOCGABS(ABS_MT_POSITION_Y), &e->mt_p.yi);
    e->mt_p.synced = 0;
#ifdef _EVENT_LOGGING
    LOGI("EV: MT minX: %d  maxX: %d  minY: %d  maxY: %d\n", e->mt_p.xi.minimum, e->mt_p.xi.maximum, e->mt_p.yi.minimum, e->mt_p.yi.maximum);
#endif

    e->vks = malloc(sizeof(*e->vks) * e->vk_count);

    for (i = 0; i < e->vk_count; ++i) {
        char *token[6];
        int j;

        for (j = 0; j < 6; ++j) {
            token[j] = vk_strtok_r((i||j)?NULL:vks, ":", &ts);
        }

        if (strcmp(token[0], "0x01") != 0) {
            /* Java does string compare, so we do too. */
            LOGW("minui: %s: ignoring unknown virtual key type %s\n", vk_path, token[0]);
            continue;
        }

        e->vks[i].scancode = strtol(token[1], NULL, 0);
        e->vks[i].centerx = strtol(token[2], NULL, 0);
        e->vks[i].centery = strtol(token[3], NULL, 0);
        e->vks[i].width = strtol(token[4], NULL, 0);
        e->vks[i].height = strtol(token[5], NULL, 0);
    }

    return 0;
}

int ev_init(void)
{
    DIR *dir;
    struct dirent *de;
    int fd;

	dir = opendir("/dev/input");
    if(dir != 0) {
        while((de = readdir(dir))) {
//            fprintf(stderr,"/dev/input/%s\n", de->d_name);
            if(strncmp(de->d_name,"event",5)) continue;
            fd = openat(dirfd(dir), de->d_name, O_RDONLY);
            if(fd < 0) continue;

			ev_fds[ev_count].fd = fd;
            ev_fds[ev_count].events = POLLIN;
            evs[ev_count].fd = &ev_fds[ev_count];

            /* Load virtualkeys if there are any */
			vk_init(&evs[ev_count]);

            ev_count++;
            if(ev_count == MAX_DEVICES) break;
        }
    }

    return 0;
}

void ev_exit(void)
{
    while (ev_count-- > 0) {
	if (evs[ev_count].vk_count) {
		free(evs[ev_count].vks);
		evs[ev_count].vk_count = 0;
	}
        close(ev_fds[ev_count].fd);
    }
}

static int vk_inside_display(__s32 value, struct input_absinfo *info, int screen_size)
{
    int screen_pos;

    if (info->minimum == info->maximum)
        return 0;

    screen_pos = (value - info->minimum) * (screen_size - 1) / (info->maximum - info->minimum);
    return (screen_pos >= 0 && screen_pos < screen_size);
}

static int vk_tp_to_screen(struct position *p, int *x, int *y)
{
    if (p->xi.minimum == p->xi.maximum || p->yi.minimum == p->yi.maximum)
    {
        // In this case, we assume the screen dimensions are the same.
        *x = p->x;
        *y = p->y;
        return 0;
    }

#ifdef _EVENT_LOGGING
    LOGI("EV: p->x=%d  x-range=%d,%d  fb-width=%d\n", p->x, p->xi.minimum, p->xi.maximum, gr_fb_width());
#endif

#ifndef RECOVERY_TOUCHSCREEN_SWAP_XY
    int fb_width = gr_fb_width();
    int fb_height = gr_fb_height();
#else
    // We need to swap the scaling sizes, too
    int fb_width = gr_fb_height();
    int fb_height = gr_fb_width();
#endif

    *x = (p->x - p->xi.minimum) * (fb_width - 1) / (p->xi.maximum - p->xi.minimum);
    *y = (p->y - p->yi.minimum) * (fb_height - 1) / (p->yi.maximum - p->yi.minimum);

    if (*x >= 0 && *x < fb_width &&
        *y >= 0 && *y < fb_height)
    {
        return 0;
    }

    return 1;
}

/* Translate a virtual key in to a real key event, if needed */
/* Returns non-zero when the event should be consumed */
static int vk_modify(struct ev *e, struct input_event *ev)
{
    static int downX = -1, downY = -1;
    static int discard = 0;
    static int lastWasSynReport = 0;
    static int touchReleaseOnNextSynReport = 0;
    int i;
    int x, y;

    // This is used to ditch useless event handlers, like an accelerometer
    if (e->ignored)     return 1;

    if (ev->type == EV_REL && ev->code == REL_Z)
    {
        // This appears to be an accelerometer or another strange input device. It's not the touchscreen.
#ifdef _EVENT_LOGGING
        LOGI("EV: Device disabled due to non-touchscreen messages.\n");
#endif
        e->ignored = 1;
        return 1;
    }

#ifdef _EVENT_LOGGING
    LOGI("EV: %s => type: %x  code: %x  value: %d\n", e->deviceName, ev->type, ev->code, ev->value);
#endif

    // Discard key-up messages
    if (ev->type == EV_KEY && ev->value == 0)
        return 1;

    if (ev->type == EV_ABS) {
        switch (ev->code) {
        case ABS_X:
#ifdef _EVENT_LOGGING
            LOGI("EV: %s => EV_ABS  ABS_X  %d\n", e->deviceName, ev->value);
#endif
            e->p.synced |= 0x01;
            e->p.x = ev->value;
            break;
        case ABS_Y:
#ifdef _EVENT_LOGGING
            LOGI("EV: %s => EV_ABS  ABS_Y  %d\n", e->deviceName, ev->value);
#endif
            e->p.synced |= 0x02;
            e->p.y = ev->value;
            break;
	case ABS_MT_SLOT:
#ifdef _EVENT_LOGGING
            LOGI("EV: %s => EV_ABS  ABS_MT_SLOT  %d\n", e->deviceName, ev->value);
#endif
            e->mt_p.slot = ev->value;
            break;
        case ABS_MT_POSITION_X:
#ifdef _EVENT_LOGGING
            LOGI("EV: %s => EV_ABS  ABS_MT_POSITION_X  %d\n", e->deviceName, ev->value);
#endif
	    if(e->mt_p.slot == 0 )
 	    {
            	e->mt_p.synced |= 0x01;
            	e->mt_p.x = ev->value;
	    }
            break;
        case ABS_MT_POSITION_Y:
#ifdef _EVENT_LOGGING
            LOGI("EV: %s => EV_ABS  ABS_MT_POSITION_Y  %d\n", e->deviceName, ev->value);
#endif
            if(e->mt_p.slot == 0 )
            {
	    	e->mt_p.synced |= 0x02;
            	e->mt_p.y = ev->value;
	    }
            break;
        case ABS_MT_TOUCH_MAJOR:
#ifdef _EVENT_LOGGING
            LOGI("EV: %s => EV_ABS  ABS_MT_TOUCH_MAJOR  %d\n", e->deviceName, ev->value);
#endif
        case ABS_MT_PRESSURE:
#ifdef _EVENT_LOGGING
            LOGI("EV: %s => EV_ABS  ABS_MT_PRESSURE  %d\n", e->deviceName, ev->value);
#endif
            if(e->mt_p.slot == 0 )
	    {
		if (ev->value == 0)
            	{
                	// We're in a touch release, although some devices will still send positions as well
                	e->mt_p.x = 0;
                	e->mt_p.y = 0;
                	touchReleaseOnNextSynReport = 1;
            	}
	    }
            break;

        default:
            // This is an unhandled message, just skip it
            return 1;
        }

        if (ev->code != ABS_MT_POSITION)
        {
            lastWasSynReport = 0;
            return 1;
        }
    }

    // Check if we should ignore the message
    if (ev->code != ABS_MT_POSITION && (ev->type != EV_SYN || (ev->code != SYN_REPORT && ev->code != SYN_MT_REPORT)))
    {
        lastWasSynReport = 0;
        return 0;
    }

#ifdef _EVENT_LOGGING
    if (ev->type == EV_SYN && ev->code == SYN_REPORT)       LOGI("EV: %s => EV_SYN  SYN_REPORT\n", e->deviceName);
    if (ev->type == EV_SYN && ev->code == SYN_MT_REPORT)    LOGI("EV: %s => EV_SYN  SYN_MT_REPORT\n", e->deviceName);
#endif

    // Discard the MT versions
    if (ev->code == SYN_MT_REPORT)      return 0;

    if (lastWasSynReport == 1 || touchReleaseOnNextSynReport == 1)
    {
        // Reset the value
        touchReleaseOnNextSynReport = 0;

        // We are a finger-up state
        if (!discard)
        {
            // Report the key up
            ev->type = EV_ABS;
            ev->code = 0;
            ev->value = (downX << 16) | downY;
        }
        downX = -1;
        downY = -1;
        if (discard)
        {
            discard = 0;
            return 1;
        }
        return 0;
    }
    lastWasSynReport = 1;

    // Retrieve where the x,y position is
    if (e->p.synced & 0x03)
    {
        vk_tp_to_screen(&e->p, &x, &y);
    }
    else if (e->mt_p.synced & 0x03)
    {
        vk_tp_to_screen(&e->mt_p, &x, &y);
    }
    else
    {
        // We don't have useful information to convey
        return 1;
    }

#ifdef RECOVERY_TOUCHSCREEN_SWAP_XY
    x ^= y;
    y ^= x;
    x ^= y;
#endif
#ifdef RECOVERY_TOUCHSCREEN_FLIP_X
    x = gr_fb_width() - x;
#endif
#ifdef RECOVERY_TOUCHSCREEN_FLIP_Y
    y = gr_fb_height() - y;
#endif

#ifdef _EVENT_LOGGING
    LOGI("EV: x: %d  y: %d\n", x, y);
#endif

    // Clear the current sync states
    e->p.synced = e->mt_p.synced = 0;
    e->p.slot = e->mt_p.slot = 0;

    // If we have nothing useful to report, skip it
    if (x == -1 || y == -1)     return 1;

    // On first touch, see if we're at a virtual key
    if (downX == -1)
    {
        // Attempt mapping to virtual key
        for (i = 0; i < e->vk_count; ++i)
        {
            int xd = ABS(e->vks[i].centerx - x);
            int yd = ABS(e->vks[i].centery - y);

            if (xd < e->vks[i].width/2 && yd < e->vks[i].height/2)
            {
                ev->type = EV_KEY;
                ev->code = e->vks[i].scancode;
                ev->value = 1;

                vibrate(VIBRATOR_TIME_MS);

                // Mark that all further movement until lift is discard, 
                // and make sure we don't come back into this area
                discard = 1;
                downX = 0;
                return 0;
            }
        }
    }

    // If we were originally a button press, discard this event
    if (discard)
    {
        return 1;
    }

    // Record where we started the touch for deciding if this is a key or a scroll
    downX = x;
    downY = y;

    ev->type = EV_ABS;
    ev->code = 1;
    ev->value = (x << 16) | y;
    return 0;
}

int ev_get(struct input_event *ev, unsigned dont_wait)
{
    int r;
    unsigned n;

    do {
        r = poll(ev_fds, ev_count, dont_wait ? 0 : -1);

        if(r > 0) {
            for(n = 0; n < ev_count; n++) {
                if(ev_fds[n].revents & POLLIN) {
                    r = read(ev_fds[n].fd, ev, sizeof(*ev));
                    if(r == sizeof(*ev)) {
                        if (!vk_modify(&evs[n], ev))
                            return 0;
                    }
                }
            }
        }
    } while(dont_wait == 0);

    return -1;
}

