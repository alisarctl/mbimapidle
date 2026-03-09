/*
 * Copyright (c) 2026, Ali Abdallah <ali.abdallah@suse.com>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of the FreeBSD Project.
 */

#ifndef __COMMON__H__
#define __COMMON__H__

#include <stdarg.h>
#include <syslog.h>
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

#endif
