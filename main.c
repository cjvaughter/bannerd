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

#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef SRV_NAME
#define SRV_NAME "bannerd"
#endif

#include "animation.h"
#include "fb.h"
#include "log.h"
#include "string_list.h"

int Interactive = 0; /* Not daemon */
int LogDebug = 0; /* Do not suppress debug messages when logging */
int StartFrame = 0; /* Loops back to this frame index */
int EndFrame = 0; /* Index of the end of the loop */
int PreserveMode = 0; /* Do not restore previous framebuffer mode */
int AllowFinish = 0; /* Waits for the animation to complete before terminating */
int DisplayFirst = 0; /* Displays the first frame while the rest finish loading */

static struct screen_info _Fb;

static int usage(char *cmd, char *msg)
{
	char cmd_copy[256];
	char *command;

	strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
	command = basename(cmd_copy);

	if (msg)
		printf("%s\n", msg);
	printf("Usage: %s [options] [interval[fps]] frame.bmp ...\n\n", command);
	printf("-D, --no-daemon       Do not fork into the background, log to stdout\n");
	printf("-v, --verbose         Do not suppress debug messages in the log\n"
               "                      (may also be suppressed by syslog configuration)\n");
	printf("-s <num>,\n"
	       "--start=<num>         Loop starts at frame <num>. Defaults to 0.\n");
        printf("-e <num>,\n"
               "--end=<num>           Loop ends at frame <num>. Defaults to last frame.\n");
	printf("-p, --preserve        Do not restore framebuffer mode on exit which\n"
	       "                      usually means leaving last displayed\n");
        printf("-f, --finish          Allows the animation to complete before termination.\n");
        printf("-d, --display-first   Display the first frame while the rest finish loading.\n");
	printf("interval              Interval in milliseconds between frames. If \'fps\'\n"
	       "                      suffix is present then it is in frames per second\n"
	       "                      Default:  41 (24fps)\n");
	printf("frame.bmp ...         List of filenames of frames in BMP format\n");

	return 1;
}

static int get_options(int argc, char **argv)
{
	static struct option _longopts[] = {
			{"no-daemon",     no_argument, &Interactive, 1},  /* -D */
			{"verbose",       no_argument, &LogDebug, 1},     /* -v */
			{"start",         required_argument, 0, 's'},     /* -s */
                        {"end",           required_argument, 0, 'e'},     /* -e */
			{"preserve",      no_argument, &PreserveMode, 1}, /* -p */
                        {"finish",        no_argument, &AllowFinish, 1},  /* -f */
                        {"display-first", no_argument, &DisplayFirst, 1}, /* -d */
			{0, 0, 0, 0}
	};

	while (1) {
		int option_index = 0;
		int c = getopt_long(argc, argv, "Dvs:e:pfd", _longopts,
				&option_index);

		if (c == -1)
			break;
		switch (c) {
		case 0: /* Do nothing, the proper flag was set */
			break;

		case 'D':
			Interactive = 1;
			break;

		case 'v':
			LogDebug = 1;
			break;

		case 's':
			if (!optarg)
				StartFrame = 0;
			else {
				int v = (int)strtol(optarg, NULL, 0);

				if (v > 0)
					StartFrame = v;
			}
			break;

                case 'e':
                        if (!optarg)
                                EndFrame = 0;
                        else {
                                int v = (int)strtol(optarg, NULL, 0);

                                if (v > 0)
                                        EndFrame = v;
                        }
                        break;

                case 'p':
                        PreserveMode = 1;
                        break;

		case 'f':
			AllowFinish = 1;
			break;

                case 'd':
                        DisplayFirst = 1;
                        break;

		case '?':
			/* The error message has already been printed
			 * by getopts_long() */
			return -1;
		}
	}

	return optind;
}

static int init_log(void)
{
	if(Interactive)
		return 0;

	openlog(SRV_NAME, LOG_CONS | LOG_NDELAY, LOG_DAEMON);

	return 0;
}

static int daemonify(void)
{
	pid_t pid, sid;

	pid = fork();

	if(pid < 0)
		return -1;

	if(pid > 0)
		_exit(0); /* Parent process exits here */

	umask(0);
	sid = setsid();

	if(sid < 0)
		return -1;

	if(chdir("/") < 0)
		return -1;

	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	return 0;
}

static void free_resources(void)
{
	fb_close(&_Fb, !PreserveMode);
	LOG(LOG_INFO, "exited");
}

static void sig_handler(int num)
{
        if (AllowFinish) {
                finish_animation = 1;
        }
        else {
                LOG(LOG_INFO, "signal %d caught", num);
	        exit(0);
        }
}

static inline int init_proper_exit(void)
{
	struct sigaction action = { .sa_handler = sig_handler, };

	sigemptyset(&action.sa_mask);

	atexit(free_resources);
	if (sigaction(SIGINT, &action, NULL)
			|| sigaction(SIGTERM, &action, NULL))
		ERR_RET(-1, "could not install signal handlers");

	return 0;
}

static inline int parse_interval(char *param, unsigned int *interval)
{
	char *p;
	unsigned int v = (unsigned int)strtoul(param, &p, 0);
	int is_fps = (p != param) && *p && !strcmp(p, "fps");

	if (!*p || is_fps) {
		if (is_fps) {
			if (!v) { /* 0fps */
				LOG(LOG_WARNING, "0fps argument in cmdline,"
						" changed to 1fps");
				v = 1;
			}
			v = 1000 / v;
		}
		*interval = v;
		return 0;
	}

	return -1;
}

static int init(int argc, char **argv, struct animation *banner)
{
	int i;
	struct string_list *filenames = NULL, *filenames_tail = NULL;
	int filenames_count = 0;

	init_log();

	i = get_options(argc, argv);
	if (i < 0)
		return usage(argv[0], NULL);

	for ( ; i < argc; ++i) {
		if (banner->interval == (unsigned int)-1)
			if (!parse_interval(argv[i], &banner->interval))
				continue;

		filenames_tail = string_list_add(&filenames, filenames_tail,
				argv[i]);
		if (!filenames_tail)
			return 1;
		else
			filenames_count++;
	}

	if (!filenames_count)
		return usage(argv[0], "No filenames specified");

	if (fb_init(&_Fb))
		return 1;
	if (init_proper_exit())
		return 1;
	if (animation_init(filenames, filenames_count, &_Fb, banner, DisplayFirst))
		return 1;
	string_list_destroy(filenames);

	if (banner->interval == (unsigned int)-1)
		banner->interval = 1000 / 24; /* 24fps */

	if (!Interactive && daemonify())
		ERR_RET(1, "could not create a daemon");

	return 0;
}

int main(int argc, char **argv) {
	struct animation banner = { .interval = (unsigned int)-1, };

	if (init(argc, argv, &banner))
		return 1;
	LOG(LOG_INFO, "started");

	return animation_run(&banner, StartFrame, EndFrame);
}

