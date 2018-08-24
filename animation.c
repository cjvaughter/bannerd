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

#include "animation.h"
#include "bmp.h"
#include "fb.h"
#include "log.h"
#include "string_list.h"

volatile sig_atomic_t finish_animation = 0;

static inline void center2top_left(struct image_info *image, int cx, int cy,
		int *top_left_x, int *top_left_y)
{
	*top_left_x = cx - image->width / 2;
	*top_left_y = cy - image->height / 2;
}

/**
 * Draws the current frame
 */
static inline int draw(struct animation *banner)
{
        int x, y;
        struct image_info *frame = &banner->frames[banner->frame_num];

        center2top_left(frame, banner->x, banner->y, &x, &y);
        return fb_write_bitmap(banner->fb, x, y, frame);
}

/**
 * Run the animation either infinitely or until 'frames' frames have been shown
 */
int animation_run(struct animation *banner, int start, int end)
{
        if (end == 0) {
                end = banner->frame_count - 1;
        }

	int rc = 0;

	while (1) {
		rc = draw(banner);
		if (rc)
			break;

                int fnum = banner->frame_num + 1;

                if (finish_animation) {
                        if (fnum >= banner->frame_count) {
                                break;
                        }
                        else {
                                banner->frame_num = fnum;
                        }
                }
                else {
                        if (fnum > end)
			        banner->frame_num = start;
                        else
                                banner->frame_num = fnum;
                }

		if (banner->interval) {
			const struct timespec sleep_time = {
				.tv_sec = banner->interval / 1000,
				.tv_nsec = (banner->interval % 1000) * 1000000 };
			nanosleep(&sleep_time, NULL);
		}
	}

	return rc;
}

int animation_init(struct string_list *filenames, int filenames_count,
		struct screen_info *fb, struct animation *a, int display_first)
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

    a->frames = malloc(filenames_count * sizeof(struct image_info));
    if (a->frames == NULL) {
        LOG(LOG_ERR, "Unable to get %d bytes of memory for animation",
            filenames_count * sizeof(struct image_info));
        return -1;
    }

    for (i = 0; i < filenames_count; ++i, filenames = filenames->next) {
        if (bmp_read(filenames->s, &a->frames[i])) {
            return -1;
        }
        if (i == 0 && display_first) {
            if (draw(a))
                return -1;
        }
    }

    return 0;
}



