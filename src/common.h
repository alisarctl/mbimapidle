/*
 * Copyright (c) 2026, Ali Abdallah <ali.abdallah@suse.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the <organization> nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __COMMON__H__
#define __COMMON__H__

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <stdbool.h>
#include <time.h>

#define TICK_MS 200
#define SEC_MS(x) (x*1000)
#define MIN2MS(x) (SEC_MS(x)*60)

#define COUNTDOWN(_val,_reset) _val = _val == 0 ? _reset : _val <= TICK_MS ? 0 : _val - TICK_MS

extern volatile int main_loop_running;
extern int log_to_syslog;
extern bool debug;

#ifdef LOG_WARN
#undef LOG_WARN
#endif

#define LOG_WARN LOG_WARNING

static inline void mlog (int level, const char *format, ...)
{
	va_list args;

	if (!debug && (level == LOG_DEBUG))
		return;

	va_start (args, format);

	if (log_to_syslog) {
		vsyslog(level, format, args);
	} else {
		time_t t = time(NULL);
		char *c = ctime(&t);
		switch(level) {
			case LOG_DEBUG:
				printf("Debug: ");
				break;
			case LOG_ERR:
				printf("ERR:   ");
				break;
			case LOG_WARNING:
				printf("Warn:  ");
				break;
			case LOG_INFO:
				printf("Info:  ");
				break;
			default:
				printf("       ");
				break;
		}
		printf("[");
		do {
			printf("%c", *c++);
		} while(*c != '\n' && *c != '\0');
		printf("] ");
		vprintf(format, args);
	}
	va_end(args);
}

static inline void tick_wait(void)
{
	struct timespec ts;
	ts.tv_sec = TICK_MS / 1000;
	ts.tv_nsec = (TICK_MS % 1000) * 1000000;
	nanosleep(&ts, &ts);
}

char *get_conf_file_path (void);

bool  parse_cmd          (const char *mbox_name,
		char *val,
		char **cmd,
		char **argv[]);

#define FREE_STR(ptr) \
	do  { \
		if (ptr != NULL) { \
			free(ptr); \
			ptr = NULL; \
		} \
	} while (0);

#define FREE_STRV(strv) \
	do { \
		char **tmp; \
		if (strv) { \
			for (tmp = strv; *tmp !=NULL; tmp++) { \
				free(*tmp);\
			} \
			FREE_STR(strv); \
		} \
	} while (0);

#define assert_not_reached() do { \
	mlog(LOG_ERR, "Assertion failed %s:%d code should not be reached", __FILE__, __LINE__); \
	exit(EXIT_FAILURE); \
} while (0);

#endif
