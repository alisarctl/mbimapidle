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
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <getopt.h>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include <errno.h>
#include <assert.h>

#include "mbox.h"
#include "imap.h"
#include "common.h"

enum {
	ARG_HELP,
	ARG_VERSION,
	ARG_FOREGROUND,
	ARG_DEBUG
};

static struct option opts[] = {
	{ "help",		no_argument, 0, ARG_HELP},
	{ "version",		no_argument, 0, ARG_VERSION },
	{ "foreground",		no_argument, 0, ARG_FOREGROUND},
	{ "debug",		no_argument, 0, ARG_DEBUG },
	{NULL, 0, NULL, 0}
};

/* extern variables */
volatile int main_loop_running = 1;

/* Ignore SIGHUP for 10 seconds after the first one received */
static int countdown_reload = SEC_MS(10);
static volatile bool reload_all = false;

bool log_to_syslog = false;
bool debug = false;
bool foreground = false;

static void show_help(void)
{
	printf ("%s [options]\n\n"
		"COMMANDS:\n"
		"	-h, --help			Show this help\n"
		"	-v, --version			Show version\n"
		"	-d, --debug			Print debugging messages (default: disabled)\n"
		"OPTIONS:\n"
		"	-f, --foreground		Run in the foreground (default: run in the background)\n\n",
		PROG);
}

static void sig_handler(int signum)
{
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

#define MAX_PASS_TOKEN_LEN 8192
static void mbox_proc(struct mbox*m)
{
	if (m->state == MBOX_DISABLED)
		return;

	COUNTDOWN(m->delay, 0);
	if (m->delay != 0) return;

	if (m->sync_pid > 0) {
		int status = 0;
		if (waitpid(m->sync_pid, &status, WNOHANG) == m->sync_pid) {
			m->sync_pid = 0;
			/* FIXME: Maybe an option to print sync command output */
			if (!foreground && debug) {
				ssize_t nbytes = 0;
				size_t rc = 0;
				char msg[256];

				ioctl(m->sync_cmd_stdout, FIONREAD, &nbytes);

				do {
					memset(msg, 0, sizeof(msg));
					rc += read(m->sync_cmd_stdout, msg, MIN(nbytes - rc, 255));
					mlog(LOG_DEBUG, "Sync: '%s' '%s'\n", m->name, msg);
				} while (rc < nbytes);

				close(m->sync_cmd_stdout);
			}
			mlog(LOG_DEBUG, "'%s' Sync done with exit code %d:%s\n",
				m->name, status, status == 0 ? "SUCCESS" : "FAILURE");
		}
	}

	if (m->pass_pid > 0) {
		int status = 0;
		if (waitpid(m->pass_pid, &status, WNOHANG) == m->pass_pid) {
			ssize_t nbytes = 0;
			size_t rc = 0;

			if (status != 0) {
				mlog(LOG_ERR,
				"'%s' password command failed with status code %d\n",
				m->name, status);
				mlog(LOG_ERR,
				"'%s' disabled, please fix the problem and reload the daemon with SIGHUP\n",
				m->name);
				goto disable;
			}

			ioctl(m->pass_pipe_fd, FIONREAD, &nbytes);

			if (nbytes > MAX_PASS_TOKEN_LEN - 1) {
				mlog(LOG_ERR, "'%s' password token is too long\n", m->name);
				goto disable;
			}
			FREE_STR(m->password);
			m->password = malloc(nbytes + 1);
			memset (m->password, 0, nbytes + 1);

			do {
				rc += read(m->pass_pipe_fd, m->password + rc, nbytes - rc);
				if (rc == -1) {
					mlog(LOG_ERR, "unexpected end of read '%s'\n", strerror(errno));
					goto disable;
				}
			} while (rc != nbytes);

			if (m->password[nbytes - 1] == '\n') {
				m->password[nbytes - 1] = '\0';
			}

			m->state = MBOX_INIT_CONNECT;
			goto out;
disable:
			m->state = MBOX_DISABLED;
out:
			m->pass_pid = 0;
			close(m->pass_pipe_fd);
		}

	}
	mbox_idle_proc(m);
}

static void mbox_check_state (struct mbox *m)
{
	if (m->state == MBOX_DISABLED)
		return;

	if (m->old_state != m->state) {
		m->state_timeout = SEC_MS(10);
		m->old_state = m->state;
	} else {
		COUNTDOWN(m->state_timeout, 0);
		/* Error switching state */
		if (m->state != MBOX_IDLE &&
		    m->state != MBOX_INIT_CONNECT &&
		    m->state_timeout == 0) {
			mlog(LOG_INFO, "'%s' connection seems to be stuck, re-trying\n", m->name);

			if (m->pass_pid) {
				kill(m->pass_pid, SIGKILL);
				close(m->pass_pipe_fd);
				m->pass_pipe_fd = 0;
			}

			mbox_free_conn(m);

			m->state = MBOX_INIT_CONNECT;
			m->state_timeout = SEC_MS(10);
		}
	}
}

int main(int argc, char *argv[])
{
	int ch;
	pid_t daemon;
	struct sigaction sa;
	int ret = EXIT_FAILURE;

	while ((ch = getopt_long(argc, argv, "hvfd", opts, NULL)) != -1) {
		switch(ch) {
			case 'h':
			case ARG_HELP:
				show_help();
				return EXIT_SUCCESS;

			case 'v':
			case ARG_VERSION:
				printf("%s: %s\n", PROG, VERSION);
				return EXIT_SUCCESS;

			case 'f':
			case ARG_FOREGROUND:
				foreground = true;
				break;

			case 'd':
			case ARG_DEBUG:
				debug = true;
				break;

			case '?':
			default:
				printf("Unknown command\n");
				return EXIT_FAILURE;
		}
	}

	if (!foreground) {
		openlog("mbimapidle", LOG_PID, LOG_DAEMON);
		log_to_syslog = true;
	}

	if (!conf_init()) {
		mlog(LOG_ERR, "Failed to load configuration\n");
		goto failure_config;
	}

	if (!mbox_init_ssl()) {
		goto failure_ssl;
	}

	if (!foreground) {
		if ((daemon = fork()) == -1) {
			mlog(LOG_ERR, "Failed to fork to run in the background\n");
		} else if (daemon != 0) {
			return EXIT_SUCCESS;
		} else {
			mlog(LOG_INFO, PROG ": daemon started\n");
		}
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

	ret = EXIT_SUCCESS;

	mbox_free_ssl();
failure_ssl:
	mbox_foreach(&mbox_shutdown_ssl);
failure_config:
	mbox_remove_all();

	return ret;
}
