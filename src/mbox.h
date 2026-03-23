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

#ifndef __MBOX__H_
#define __MBOX__H_

#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/queue.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>

enum {
	MBOX_INIT_CONNECT = 0,
	MBOX_WANT_PASS,
	MBOX_TRY_CONNECT,
	MBOX_GET_SRV_CAPS,
	MBOX_CHECK_SRV_CAPS,
	MBOX_AUTHENTICATE,
	MBOX_CHECK_LOGIN,
	MBOX_CONNECT_STARTTLS,
	MBOX_CONNECT_TLS,
	MBOX_TLS_HANDSHAKE,
	MBOX_TLS_GET_SRV_CAPS,
	MBOX_STARTTLS_OFFER,
	MBOX_STARTTLS_HANDSHAKE,
	MBOX_SELECT,
	MBOX_CHECK_SELECT,
	MBOX_SEND_IDLE,
	MBOX_CHECK_IDLE,
	MBOX_CHECK_DONE,
	MBOX_IDLE,
	MBOX_DISABLED,
	MBOX_INVALID
};

enum {
	TLS_TYPE_INVALID = 0,
	TLS_TYPE_NONE,
	TLS_TYPE_STARTTLS,
	TLS_TYPE_SSL
};

enum {
	AUTH_TYPE_INVALID = 0,
	AUTH_TYPE_PLAIN,
	AUTH_TYPE_XOAUTH2
};

struct mbox {
	/* Loaded from configuration */
	char     *name;
	char     *hostname;
	char     *username;
	char     *password;
	char     *pass_cmd;
	char    **pass_args;
	char     *sync_cmd;       /* Command to run on new emails */
	char    **sync_args;
	uint32_t  idle_timeout;
	uint16_t  port;
	uint8_t   tls_type;
	uint8_t   auth_type;
	bool      check_cert;
	/* End of loaded from conf fields */
	int       sock;
	char     *buf;
	size_t    buf_size;
	size_t    buf_len;
	int       state;
	int       old_state;       /* Save old state to check if we are stuck */
	uint32_t  nfails;          /* Number of times we got some failures */
	uint32_t  state_timeout;   /* Single state timeout, not applicable to MBOX_IDLE */
	pid_t     sync_pid;        /* pid of the sync command */
	pid_t     pass_pid;        /* pid of pass command */
	int       pass_pipe_fd;    /* child pipe fd to read password from */
	int       sync_cmd_stdout; /* Sync command standard output */
	uint32_t  re_idle_in;      /* DONE -> IDLE Sequence in ms time */
	uint32_t  delay;           /* Delay in ms to re-connect or to next action */
	uint32_t  caps;
	uint32_t  tag;
	struct sockaddr_in servaddr;
	SSL      *ssl;
	BIO      *bio;
	TAILQ_ENTRY(mbox) mboxes;
};

typedef void (*mbox_conn) (struct mbox *mbox);

bool conf_init(void);
void mbox_foreach(mbox_conn func);
void mbox_remove_all(void);
void mbox_run_sync(struct mbox *m);
void mbox_get_pass (struct mbox *m);
void mbox_free_conn(struct mbox *m);
void mbox_free(struct mbox *m);

#endif /* __MBOX_H__ */
