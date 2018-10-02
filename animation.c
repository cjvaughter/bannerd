/*
 *  A framebuffer animation daemon
 *
 *  Copyright (C) 2012 Alexander Lukichev
 *
 *  Alexander Lukichev <alexander.lukichev@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 */

#include <time.h>
#include <string.h>
#include "animation.h"
#include "image.h"
#include "fb.h"
#include "log.h"
#include "string_list.h"

#define PATH_MAX 1024

volatile sig_atomic_t finish_animation = 0;

static int look_for_process(const char* processname) {
	FILE *fp;
	int status, count = 0;
	char path[PATH_MAX];
	char pattern[PATH_MAX];
	if (processname == NULL)
		return 0;
	strcpy(pattern, "pgrep ");
	strcat(pattern, processname);
	fp = popen(pattern, "r");
	if (fp == NULL)
		return 0;
	while (fgets(path, PATH_MAX, fp) != NULL)
		count++;
	status = pclose(fp);
	return count;
}


static inline void center2top_left(struct image_info *image, int cx, int cy,
		int *top_left_x, int *top_left_y)
{
	*top_left_x = cx - image->width / 2;
	*top_left_y = cy - image->height / 2;
}

/**
 * Draws the current frame
 */
static inline int draw(struct animation *banner) {
	int x, y;
	struct image_info *frame = &banner->frames[banner->frame_num];

	center2top_left(frame, banner->x, banner->y, &x, &y);
	return fb_write_bitmap(banner->fb, x, y, frame);
}

/**
 * Run the animation either infinitely or until 'frames' frames have been shown
 */
int animation_run(struct animation *banner) {
	int rc = 0;

	while (1) {
		rc = draw(banner);
		if (rc)
			break;

		if (banner->frame_num == banner->wait_frame) {
			LOG(LOG_INFO, "Waiting at frame %d for %d ms\n", banner->wait_frame,
					banner->wait_time);
			const struct timespec sleep_time = { .tv_sec = banner->wait_time
					/ 1000, .tv_nsec = (banner->wait_time % 1000) * 1000000L };
			nanosleep(&sleep_time, NULL);
		}

		int fnum = banner->frame_num + 1;
		if (finish_animation) {
			if (fnum >= banner->frame_count) {
				break;
			} else {
				banner->frame_num = fnum;
			}
		} else {
			if (fnum >= banner->loop_end)
				banner->frame_num = banner->loop_start;
			else
				banner->frame_num = fnum;
		}

		if (banner->interval) {
			const struct timespec sleep_time = { .tv_sec = banner->interval
					/ 1000, .tv_nsec = (banner->interval % 1000) * 1000000L };
			nanosleep(&sleep_time, NULL);
		}
		if (banner->processname) {
			if (look_for_process(banner->processname) > 0)
				break;
		}
	}

	return rc;
}


int animation_init(struct string_list *filenames, int filenames_count, struct screen_info *fb,
                       struct animation *a, int display_first, int loop_start, int loop_end, int wait_frame, int wait_time, char* processname)
{
    int i;

    if (!fb->fb_size) {
        LOG(LOG_ERR, "Unable to init animation against uninitialized "
                "framebuffer");
        return -1;
    }

    a->fb = fb;
    a->x = fb->width / 2;
    a->y = fb->height / 2;
    a->frame_num = 0;
    a->frame_count = filenames_count;
    a->loop_start = loop_start;
    a->loop_end = (loop_end == 0) ? filenames_count : loop_end;
    a->wait_frame = wait_frame;
    a->wait_time = wait_time;
	a->processname = (processname == NULL ? NULL : strdup(processname));

    a->frames = malloc(filenames_count * sizeof(struct image_info));
    if (a->frames == NULL) {
        LOG(LOG_ERR, "Unable to get %d bytes of memory for animation",
            filenames_count * sizeof(struct image_info));
        return -1;
    }

    for (i = 0; i < filenames_count; ++i, filenames = filenames->next) {
        if (image_read(filenames->s, &a->frames[i])) {
            return -1;
        }
        if (i == 0 && display_first) {
            if (draw(a))
               return -1;
        }
    }

    return 0;
}
