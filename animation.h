/*
 *  Animation processing
 *
 *  Copyright (C) 2012 Alexander Lukichev
 *
 *  Alexander Lukichev <alexander.lukichev@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 */

#ifndef _ANIMATION_H
#define _ANIMATION_H

#include <signal.h>

struct screen_info;
struct string_list;
struct commands_data;

struct animation {
	struct screen_info *fb;
    int x; /* Center of frames */
    int y; /* Center of frames */
    struct image_info *frames;
    int frame_num;
    int frame_count;
    unsigned int interval;
    struct commands_data *commands;
};

extern volatile sig_atomic_t finish_animation;

int animation_init(struct string_list *filenames, int filenames_count,
		struct screen_info *fb, struct animation *a, int display_first);
int animation_run(struct animation *banner, int start, int end);

#endif /* _ANIMATION_H */
