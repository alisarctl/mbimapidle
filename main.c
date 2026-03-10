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

#include <string.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <getopt.h>

#include <unistd.h>
#include <signal.h>

#include <assert.h>

#include "mbox.h"
#include "imap.h"
#include "common.h"

enum {
    ARG_HELP,
    ARG_VERSION,
    ARG_DEBUG
};

static struct option opts[] = {
    { "help", no_argument, 0, ARG_HELP},
    { "version", no_argument, 0, ARG_VERSION },
    { "debug", no_argument, 0, ARG_DEBUG },
    {NULL, 0, NULL, 0}
};

/* extern variables */
volatile int main_loop_running = 1;

/* Ignore SIGHUP for 10 seconds after the first one received */
static int countdown_reload = SEC_MS(10);
static volatile bool reload_all = false;

int log_to_syslog = 0;
bool debug = false;

static void sig_handler(int signum) {

    if (signum == SIGHUP) {
        if (reload_all || countdown_reload != 0) {
            mlog(LOG_INFO, "Daemon is starting up or reloading, ignoring SIGHUP\n");
        } else {
            reload_all = true;
            countdown_reload = SEC_MS(10);
        }
    } else
        main_loop_running = 0;
}

static void mbox_proc(struct mbox*m) {
    if (m->state == MBOX_DISABLED)
        return;

    COUNTDOWN(m->delay, 0);
    if (m->delay != 0) return;

    if (m->sync_pid > 0) {
        int status = 0;
        if (waitpid(m->sync_pid, &status, WNOHANG) == m->sync_pid) {
            m->sync_pid = 0;
        }
    }

    mbox_idle_proc(m);
}

static void mbox_check_state (struct mbox *m) {
    if (m->state == MBOX_DISABLED)
        return;

    if (m->old_state != m->state) {
        m->state_timeout = SEC_MS(10);
        m->old_state = m->state;
    } else {
        COUNTDOWN(m->state_timeout, 0);
        /* Error switching state */
        if (m->state != MBOX_IDLE && m->state != MBOX_INIT_CONNECT && m->state_timeout == 0) {
            mlog(LOG_INFO, "'%s' connection seems to be stuck, re-trying\n", m->name);
            mbox_free_conn(m);
            m->state = MBOX_INIT_CONNECT;
            m->state_timeout = SEC_MS(10);
        }
    }
}

int main(int argc, char *argv[])
{
    int ch;
    struct sigaction sa;

    while ((ch = getopt_long(argc, argv, "h", opts, NULL)) != -1) {
        switch(ch) {
            case ARG_HELP:
                printf("help\n");
                return EXIT_SUCCESS;
                break;
            case ARG_VERSION:
                printf("version\n");
                return EXIT_SUCCESS;
                break;
            case ARG_DEBUG:
                debug = true;
                break;
            case '?':
            default:
                printf("Unknown command\n");
                return EXIT_FAILURE;
        }
    }

    openlog("mbimapidle", LOG_PID, LOG_DAEMON);

    if (!conf_init()) {
        mlog(LOG_ERR, "Failed to load configuration\n");
        mbox_foreach(&mbox_free);
        return EXIT_FAILURE;
    }

    if (!mbox_init_ssl()) {
        return EXIT_FAILURE;
    }

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sig_handler;

    if (sigaction(SIGINT, &sa, NULL) == -1)
        mlog(LOG_ERR, "Failed to set SIGINT action\n");

    if (sigaction(SIGTERM, &sa, NULL) == -1)
        mlog(LOG_ERR, "Failed to set SIGTERM action\n");

    if (sigaction(SIGHUP, &sa, NULL) == -1)
        mlog(LOG_ERR, "Failed to set SIGTERM action\n");


    while (main_loop_running) {

        /* Reload configuration on SIGHUP */
        COUNTDOWN(countdown_reload, 0);
        if (reload_all) {

            mlog(LOG_INFO, "Reloading configuration\n");
            mbox_foreach(&mbox_shutdown_ssl);
            mbox_remove_all();

            if (!conf_init()) {
                mlog(LOG_ERR, "Failed to load configuration\n");
                mlog(LOG_INFO, "Please fix configuration and reload with SIGHUP\n");
                mbox_remove_all();
                /* Accept SIGHUP for reloading without delays */
                countdown_reload = 0;
            }
            reload_all = false;
        }

        /* Process messages/connections/etc.*/
        mbox_foreach(&mbox_proc);

        /* Check mbox state */
        mbox_foreach(&mbox_check_state);

        tick_wait();
    }

    mlog(LOG_INFO, "Exiting gracefully");
    mbox_foreach(&mbox_shutdown_ssl);
    mbox_remove_all();
    mbox_free_ssl();

    return EXIT_SUCCESS;
}
