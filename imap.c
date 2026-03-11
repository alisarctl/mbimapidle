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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <sys/select.h>

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

#include <assert.h>

#include "imap.h"
#include "common.h"
#include "base64.h"

extern volatile int main_loop_running;

static SSL_CTX *ssl_ctx = NULL;
#define RESET_BUFFER(m) memset(m->buf, 0, m->buf_size); m->buf_len = 0;

static inline void handle_failure(struct mbox *m) {
    uint32_t delay;

    mbox_free_conn(m);

    m->nfails++;
    delay = m->nfails << 5;

    mlog(LOG_DEBUG,"'%s' retrying connection in %lu seconds\n", m->name, delay);

    m->delay = SEC_MS(delay);
    m->delay = (m->delay / 100) * 100;
    m->state = MBOX_INIT_CONNECT;
    m->state_timeout = SEC_MS(20);
}

static bool imap_check_tag(struct mbox *m) {
    char msg_tag[12];

    memset (msg_tag, 0, sizeof(msg_tag));
    sprintf(msg_tag, "A%010d", m->tag);

    return strstr(m->buf, msg_tag) != NULL;
}

static bool imap_is_sasl_challenge(struct mbox *m) {

    return m->buf[m->buf_len - 1] == '=' &&
           m->buf[m->buf_len - 2] == '=' &&
           m->buf[0] == '+';
}

static bool imap_is_idle_completed(struct mbox *m) {
    char idle_msg[30];

    memset (idle_msg, 0, sizeof(idle_msg));
    sprintf(idle_msg, "A%010d OK Idle completed", m->tag);

    return !strncmp(m->buf, idle_msg, 29);
}

static bool imap_check_idle(struct mbox *m) {
    mlog(LOG_DEBUG, "'%s' IDLE message '%s'\n", m->name, m->buf);

    assert(m->buf_len > 0);

    /* Expecting '+idling' message */
    if (m->buf_len < 8)
        return false;

    if (!strncmp(m->buf, "+ idling", 8))
        return true;

    return false;
}

static void imap_decode64(struct mbox *m) {
    char *tmp;

    printf("String to decode %s\n", m->buf);
    tmp = b64_decode(m->buf + 2, m->buf_len);
    RESET_BUFFER(m);

    m->buf_len = strlen(tmp);
    strncpy(m->buf, tmp, m->buf_len);

    printf("Decoded string '%s'\n", m->buf);
    free(tmp);
}

static bool imap_check_select(struct mbox *m) {
    if (strstr(m->buf, "Select completed") != NULL)
        return true;

    if (strstr(m->buf, "selected") != NULL)
        return true;

    if (strstr(m->buf, "[AUTHENTICATIONFAILED]") != NULL)
        return false;

    return false;
}

static bool imap_check_login(struct mbox *m) {
    mlog(LOG_DEBUG, "'%s' Login response from server %s\n", m->name, m->buf);
    if (strstr(m->buf, "Logged in") != NULL) {
        return true;
    }

    if (strstr(m->buf, "authenticated") != NULL) {
        return true;
    }

    return false;
}

static bool imap_is_keep_alive(struct mbox *m) {
    mlog(LOG_DEBUG, "'%s' Keep alive '%s'\n", m->name, m->buf);
    if (strstr(m->buf, "* OK Still here") != NULL)
        return true;
    return false;
}

static bool imap_is_capability(struct mbox *m) {
    if (!strncmp(m->buf, "* OK [CAPABILITY ", 17))
        return true;
    if (!strncmp(m->buf, "* OK CAPABILITY ", 16))
        return true;
    if (!strncmp(m->buf, "* CAPABILITY ", 13))
        return true;
    return false;
}

static void mbox_conn_init (struct mbox *m) {
    struct hostent *hent;
    int flags;

    if ((hent = gethostbyname(m->hostname)) == NULL) {
        mlog(LOG_ERR, "'%s' Failed to get host info for hostname '%s'\n",
                m->name, m->hostname);

        handle_failure(m);
        return;
    }

    bzero ((char*) &m->servaddr, sizeof (struct sockaddr_in));
    m->servaddr.sin_family       = AF_INET;
    m->servaddr.sin_port         = htons(m->port);
    m->servaddr.sin_addr         = *((struct in_addr *)hent->h_addr);

    m->sock = socket (AF_INET, SOCK_STREAM, 0);
    if (m->sock < 0)
        mlog(LOG_DEBUG, "'%s' Failed to create socket\n", m->name);

    flags = fcntl(m->sock, F_GETFL, 0);
    fcntl(m->sock, F_SETFL, flags | O_NONBLOCK);
    m->state = MBOX_TRY_CONNECT;
}

static void mbox_connect (struct mbox *m) {

    while(main_loop_running) {
        if (connect(m->sock, (struct sockaddr *)&m->servaddr, sizeof(struct sockaddr_in)) < 0) {
            /* Retry again on the next loop */
            if (errno == EINPROGRESS || errno == EALREADY) {
                break;
            }

            if (errno == EISCONN) {
                if (m->tls_type == TLS_TYPE_STARTTLS || m->tls_type == TLS_TYPE_NONE)
                    m->state = MBOX_GET_SRV_CAPS;
                else if (m->tls_type == TLS_TYPE_SSL)
                    m->state = MBOX_CONNECT_TLS;
                else
                    assert_not_reached();
                break;
            }

            mlog(LOG_INFO, "'%s' Failed to connect to %s port %u (error: %s) \n",
                 m->name, m->hostname, m->port, strerror(errno));
            handle_failure(m);
            break;
        }
    }
}

static bool mbox_read_socket (struct mbox *m, bool block) {

    int idx = 0;
    int rc;
    bool ret = false;

    idx = m->buf_len;

    while(main_loop_running) {
        rc = recv(m->sock, m->buf + idx, m->buf_size - m->buf_len, MSG_DONTWAIT);
        if (rc == -1) {
            if (errno == EAGAIN) {
                if (!block) break;
            } else {
                mlog(LOG_INFO, "%s Timeout getting server caps\n", m->name);
                handle_failure(m);
                break;
            }
        } else {
            idx += rc;
            if (m->buf[idx - 1] == '\n' && m->buf[idx - 2] == '\r') {
                m->buf[idx - 2] = '\0';
                m->buf_len = idx - 2;
                ret = true;
                break;
            }
        }
    }
    return ret;
}

static void check_srv_caps (struct mbox *m) {
    char *p, *brkb;
    bool is_cap = false;

    mlog(LOG_DEBUG, "'%s' srv caps %s\n", m->name, m->buf);

    for (p = strtok_r(m->buf, " ", &brkb);
         p;
         p = strtok_r(NULL, " ", &brkb)) {
        if (!strncmp(p, "IDLE", 4)) {
            m->caps |= CAPS_IDLE;
        } else if (!strncmp(p, "STARTTLS", 8)) {
            m->caps |= CAPS_STARTTLS;
        } else if (!strncmp(p, "AUTH=PLAIN", 10)) {
            m->caps |= CAPS_AUTH_PLAIN;
        } else if (!strncmp(p, "AUTH=XOAUTH2", 12)) {
            m->caps |= CAPS_AUTH_XOAUTH2;
        } else if (!strncmp(p, "ready", 5)) {
            m->caps |= CAPS_READY;
        } else if (!strncmp(p, "[CAPABILITY", 11) ||
                   !strncmp(p, "CAPABILITY", 10))
            is_cap = true;
    }

    if (!is_cap) {
        goto out;
    }

    if (!(m->caps & CAPS_AUTH_PLAIN)) {
        mlog(LOG_INFO, "'%s' Server doesn't support AUTH:PLAIN, don't know how to authenticate, disabling\n", m->name);
        goto disable;
    }

    if (!(m->caps & CAPS_AUTH_XOAUTH2) && m->auth_type == AUTH_TYPE_XOAUTH2) {
        mlog(LOG_WARN,
             "'%s' Server doesn't support XOAUTH2 authentication\n",
             m->name);
        goto disable;
    }

    if (m->tls_type == TLS_TYPE_STARTTLS) {
        if (m->caps & CAPS_STARTTLS) {
            m->state = MBOX_CONNECT_STARTTLS;
        } else {
            mlog(LOG_INFO, "'%s' Server doesn't support STARTTLS, disabling\n", m->name);
            goto disable;
        }
    } else if (m->tls_type == TLS_TYPE_SSL || m->tls_type == TLS_TYPE_NONE) {
        m->state = MBOX_AUTHENTICATE;
        if (m->tls_type == TLS_TYPE_NONE) {
            mlog(LOG_WARN,
                 "'%s' *** WARNING: Password will be sent to the server un-ecrypted ***\n",
                 m->name);
        }
    } else
        assert_not_reached();

    return;
    /* FIXME, checl CAPABILITY */
out:
    mlog(LOG_ERR, "'%s' Unexpected capability message from server, disabling %s\n", m->buf, m->name);
disable:
    m->state = MBOX_DISABLED;
}

static int wait_for_activity(SSL *ssl, int write)
{
    fd_set fds;
    int width, sock;
    struct timeval sel_timeout;
    int ret;

    sel_timeout.tv_sec = 0;
    sel_timeout.tv_usec = TICK_MS;

    /* Get hold of the underlying file descriptor for the socket */
    sock = SSL_get_fd(ssl);

    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    width = sock + 1;

    if (write)
        ret = select(width, NULL, &fds, NULL, &sel_timeout);
    else
        ret = select(width, &fds, NULL, NULL, &sel_timeout);

    return ret;
}

static int handle_io_failure(struct mbox *m, int res)
{
    switch (SSL_get_error(m->ssl, res)) {
        case SSL_ERROR_WANT_READ:
            /* Temporary failure. Wait until we can read and try again */
            wait_for_activity(m->ssl, 0);
            return 1;

        case SSL_ERROR_WANT_WRITE:
            /* Temporary failure. Wait until we can write and try again */
            wait_for_activity(m->ssl, 1);
            return 1;

        case SSL_ERROR_ZERO_RETURN:
            /* EOF */
            return 0;

        case SSL_ERROR_SYSCALL:
            return -1;

        case SSL_ERROR_SSL:
            /*
             * If the failure is due to a verification error we can get more
             * information about it from SSL_get_verify_result().
             */
            if (SSL_get_verify_result(m->ssl) != X509_V_OK)
                mlog(LOG_ERR, "'%s' Verify error: %s\n",
                     m->name,
                     X509_verify_cert_error_string(SSL_get_verify_result(m->ssl)));
            return -1;

        default:
            return -1;
    }
}

static int mbox_skip_cert_validation(int unused, X509_STORE_CTX *ctx) {
    (void)unused;
    (void)ctx;
    return 1;
}


static bool mbox_bio_init(struct mbox *m) {
    bool ret = false;

    m->bio = BIO_new(BIO_s_socket());

    if (m->bio == NULL) {
        mlog(LOG_ERR, "'%s' Failed to create BIO socket wrapper\n", m->name);
        handle_failure(m);
        goto end;
    }

    /* This seems to reset O_NONBLOCK */
    BIO_set_fd(m->bio, m->sock, BIO_CLOSE);
    BIO_socket_nbio(m->sock, 1);

    ret = true;
end:
    return ret;
}

static bool mbox_ssl_new (struct mbox *m) {
    bool ret = false;

    m->ssl = SSL_new(ssl_ctx);
    if (m->ssl == NULL) {
        mlog(LOG_ERR,"'%s' Failed to create the SSL object\n", m->name);
        goto end;
    }

    SSL_set_bio(m->ssl, m->bio, m->bio);

    if (!SSL_set_tlsext_host_name(m->ssl, m->hostname)) {
        mlog(LOG_ERR, "'%s' Failed to set the SNI hostname\n", m->name);
        goto end;
    }

    if (m->check_cert) {
        mlog(LOG_DEBUG, "'%s' Setting hostname %s for certificate validation\n", m->name, m->hostname);
        if (!SSL_set1_host(m->ssl, m->hostname)) {
            mlog(LOG_ERR, "'%s' Failed to set the certificate verification hostname\n", m->name);
            goto end;
        }
    } else {
        SSL_set_verify(m->ssl, SSL_VERIFY_PEER, mbox_skip_cert_validation);
        mlog(LOG_WARN, "'%s' Skipping certificate validation\n", m->name);
    }

    ret = true;
end:
    return ret;
}

static void mbox_starttls(struct mbox *m) {

    char msg[24];
    ssize_t rc;

    /* Create a BIO to wrap the socket */
    mbox_bio_init(m);

    sprintf(msg, "A%010d STARTTLS\r\n", ++m->tag);
    rc = send(m->sock, msg, strlen(msg), 0);

    if (rc != strlen(msg)) {
        mlog(LOG_INFO, "'%s' Failed to send starttls\n", m->name);
        handle_failure(m);
    } else {
        m->state = MBOX_STARTTLS_OFFER;
    }
}

static void mbox_tls(struct mbox *m) {
    mbox_bio_init(m);
    m->state = MBOX_TLS_HANDSHAKE;
}

static bool mbox_write_ssl (struct mbox *m, char *msg) {

    size_t written = 0;

    while (main_loop_running && !SSL_write_ex(m->ssl, msg, strlen(msg), &written)) {
        if (handle_io_failure(m, 0) == 1)
            continue; /* Retry */
        /* FIXME */
        mlog(LOG_ERR, "'%s' Failed to perform command\n", m->name);
        goto end; /* Cannot retry: error */
    }

    return true;
end:
    return false;
}

static bool mbox_write(struct mbox *m, char *msg) {
    bool ret = false;
    size_t written = 0;

    while (main_loop_running) {
        written += send(m->sock, msg + written, strlen(msg) - written, MSG_DONTWAIT);
        if (written == -1) {
            mlog(LOG_ERR, "'%s' Failed to send message to the server\n", m->name);
            break;
        }
        if (written == strlen(msg)) {
            ret = true;
            break;
        }
    }
    return ret;
}

static bool mbox_authenticate(struct mbox *m) {
    char *msg, *xoauth;
    char *b64;
    size_t msg_len, xoauth_len;
    bool ret;
    int b64_len = 0;

    if (m->auth_type == AUTH_TYPE_PLAIN) {
        msg_len = strlen(m->username) + strlen(m->password) + 25;
        msg = malloc(msg_len);

        memset(msg, 0, msg_len);
        sprintf(msg, "A%010d LOGIN \"%s\" \"%s\"\r\n",
                ++m->tag, m->username, m->password);
    } else if (m->auth_type == AUTH_TYPE_XOAUTH2) {
        xoauth_len = strlen(m->username) + strlen(m->password) + 24;

        xoauth = malloc(xoauth_len);
        memset(xoauth, 0, xoauth_len);

        sprintf(xoauth, "user=%s\001auth=Bearer %s\001\001",
                m->username, m->password);

        b64 = b64_encode(xoauth, &b64_len);

        /* tag length + auth message + \r \n */
        msg_len = b64_len + 36;
        msg = malloc(msg_len);

        memset(msg, 0, msg_len);

        sprintf(msg, "A%010d AUTHENTICATE XOAUTH2 %s\r\n", ++m->tag, b64);
        free(xoauth);
        free(b64);
    } else
        assert_not_reached();

    if (m->tls_type == TLS_TYPE_NONE) {
        ret = mbox_write(m, msg);
    } else {
        ret = mbox_write_ssl(m, msg);
    }

    if (!ret)
        mlog(LOG_ERR, "'%s' Failed to send login message\n", m->name);
    free(msg);
    return ret;
}

static int tls_handshake(struct mbox *m) {
    int ret = 0;
    if (!m->ssl && !mbox_ssl_new(m)) {
        mlog(LOG_ERR, "'%s' failed to init ssl\n", m->name);
        goto end;;
    }

    /* Do the handshake with the server */
    while (main_loop_running && (ret = SSL_connect(m->ssl)) != 1) {
        if (handle_io_failure(m, ret) == 1) {
            break; /* Retry */
        }
        mlog(LOG_ERR,"'%s' Failed to connect to server\n", m->name);
        goto end; /* Cannot retry: error */
    }

    return ret;
end:
    ERR_print_errors_fp(stderr);
    handle_failure(m);
    return -1;
}

static bool mbox_read_ssl(struct mbox *m, bool block) {
    int eof = 0;

    while (main_loop_running && !eof && !SSL_read_ex(m->ssl, m->buf, m->buf_size, &m->buf_len)) {
        switch (handle_io_failure(m, 0)) {
            case 1:
                if (!block) {return false;}
                continue; /* Retry */
            case 0:
                eof = 1;
                continue;
            case -1:
            default:
                mlog(LOG_ERR, "'%s' Failed reading remaining data\n", m->name);
                goto end; /* Cannot retry: error */
        }
    }

    if (!eof) {
            if (m->buf[m->buf_len - 1] == '\n' && m->buf[m->buf_len - 2] == '\r') {
                m->buf[m->buf_len - 2] = '\0';
                return true;
            }
            return false;
    }
    mlog(LOG_ERR, "'%s' Error retry connect eof:%d\n", m->name, eof);
    return !eof;

end:
    ERR_print_errors_fp(stderr);
    mlog(LOG_ERR, "'%s' X Error retry connect eof:%d\n", m->name, eof);
    handle_failure(m);
    return false;
}

static bool mbox_read(struct mbox *m, bool block) {
    bool ret;
    if (m->tls_type == TLS_TYPE_NONE)
        ret = mbox_read_socket(m, block);
    else
        ret = mbox_read_ssl(m, block);

    return ret;
}

static bool mbox_select(struct mbox *m) {
    char msg[32];
    bool ret;

    memset(msg, 0, sizeof(msg));
    sprintf(msg, "A%010d SELECT INBOX\r\n", ++m->tag);

    if (m->tls_type == TLS_TYPE_NONE)
        ret = mbox_write(m, msg);
    else
        ret = mbox_write_ssl(m, msg);

    if (!ret)
        mlog(LOG_ERR, "'%s' Failed to select INBOX \n", m->name);
    return ret;
}

static bool mbox_send_idle(struct mbox *m) {
    char msg[32];
    bool ret;

    memset(msg, 0, sizeof(msg));
    sprintf(msg, "A%010d IDLE\r\n", ++m->tag);

    if (m->tls_type == TLS_TYPE_NONE)
        ret = mbox_write(m, msg);
    else
        ret = mbox_write_ssl(m, msg);

    if (!ret)
        mlog(LOG_ERR, "'%s' Failed to send IDLE\n", m->name);
    return ret;
}

static bool mbox_send_done(struct mbox *m) {
    char msg[32];
    bool ret;

    memset(msg, 0, sizeof(msg));
    sprintf(msg, "DONE\r\n");

    if (m->tls_type == TLS_TYPE_NONE)
        ret = mbox_write(m, msg);
    else
        ret = mbox_write_ssl(m, msg);

    if (!ret)
        mlog(LOG_ERR, "'%s' Failed to send DONE command\n", m->name);
    return ret;
}

static bool mbox_send_empty(struct mbox *m) {
    char msg[32];
    bool ret;

    memset(msg, 0, sizeof(msg));
    sprintf(msg, "\r\n");
    ret = mbox_write_ssl(m, msg);

    if (!ret)
        mlog(LOG_ERR, "'%s' Failed to send challenge response\n", m->name);
    return ret;
}

static bool mbox_query_caps(struct mbox *m) {
    char msg[32];
    bool ret;

    memset(msg, 0, sizeof(msg));
    sprintf(msg, "A%010d CAPABILITY\r\n", ++m->tag);

    if (m->tls_type == TLS_TYPE_NONE)
        ret = mbox_write(m, msg);
    else
        ret = mbox_write_ssl(m, msg);

    if (!ret)
        mlog(LOG_ERR, "'%s' Failed to send IDLE\n", m->name);
    return ret;
}

void mbox_idle_proc(struct mbox *m) {

    if (m->state == MBOX_DISABLED)
        return;

    switch(m->state) {
        case MBOX_WANT_PASS:
            if (!m->pass_pid) {
                mlog(LOG_DEBUG, "'%s' getting password from pass_cmd\n", m->name);
                mbox_get_pass(m);
            } else {
                mlog(LOG_DEBUG, "'%s' waiting for password from pass_cmd pid:%d\n",
                     m->name, m->pass_pid);
            }
            break;
        case MBOX_INIT_CONNECT:
            mlog(LOG_DEBUG, "'%s' init connnection\n", m->name);
            mbox_conn_init(m);
            break;
        case MBOX_TRY_CONNECT:
            mlog(LOG_DEBUG, "'%s' connecting...\n", m->name);
            mbox_connect(m);
            break;
        case MBOX_GET_SRV_CAPS:
            mlog(LOG_DEBUG, "'%s' get server caps %s\n", m->name, m->buf);
            if (mbox_read_socket(m, false)) {
                m->state = MBOX_CHECK_SRV_CAPS;
            }
            break;
        case MBOX_CHECK_SRV_CAPS:
            if (imap_is_capability(m)) {
                check_srv_caps(m);
            } else {
                mlog(LOG_DEBUG, "'%s' query server caps\n", m->name);
                mbox_query_caps(m);
                if (m->tls_type == TLS_TYPE_NONE)
                    m->state = MBOX_GET_SRV_CAPS;
                else
                    m->state = MBOX_TLS_GET_SRV_CAPS;
            }
            RESET_BUFFER(m);
            break;
        case MBOX_CONNECT_STARTTLS:
            mlog(LOG_DEBUG, "'%s' starttls connection...\n", m->name);
            mbox_starttls(m);
            RESET_BUFFER(m);
            break;
        case MBOX_STARTTLS_OFFER:
            if (mbox_read_socket(m, false)) {
                /* FIXME, check STARTTLS OFFER */
                m->state = MBOX_STARTTLS_HANDSHAKE;
            }
            break;
        case MBOX_STARTTLS_HANDSHAKE:
             if(tls_handshake(m) > 0) {
                 if (mbox_authenticate(m))
                     m->state = MBOX_CHECK_LOGIN;
             }
            break;
        case MBOX_CONNECT_TLS:
            mlog(LOG_DEBUG, "'%s' tls connection\n", m->name);
            mbox_tls(m);
            break;
        case MBOX_TLS_HANDSHAKE:
            if(tls_handshake(m) > 0) {
                m->state = MBOX_TLS_GET_SRV_CAPS;
            }
            break;
        case MBOX_TLS_GET_SRV_CAPS:
            if (mbox_read(m, false)) {
                m->state = MBOX_CHECK_SRV_CAPS;
            }
            break;
        case MBOX_AUTHENTICATE:
            mlog(LOG_INFO, "'%s' authenticating...\n", m->name);
            if (mbox_authenticate(m)) {
                m->state = MBOX_CHECK_LOGIN;
            }
            RESET_BUFFER(m);
            break;
        case MBOX_CHECK_LOGIN:
            mlog(LOG_DEBUG, "'%s' checking login result\n", m->name);
            if (mbox_read(m, false)) {
                if (imap_check_tag(m)) {
                    if (imap_check_login(m)) {
                        mlog(LOG_DEBUG,"'%s' login okay\n", m->name);
                        m->state = MBOX_SELECT;
                        m->nfails = 0;
                        RESET_BUFFER(m);
                    } else {
                        mlog(LOG_DEBUG,"'%s' login failed\n", m->name);
                        handle_failure(m);
                    }
                } else if (m->auth_type == AUTH_TYPE_XOAUTH2 && imap_is_sasl_challenge(m)) {
                    imap_decode64(m);
                    mlog(LOG_DEBUG, "'%s' AUTH FAILED '%s'\n", m->name, m->buf);
                    mbox_send_empty(m);
                    RESET_BUFFER(m);
                    m->state = MBOX_DISABLED;
                }
            }
            break;
        case MBOX_SELECT:
            mlog(LOG_DEBUG, "'%s' sending select\n", m->name);
            if (mbox_select(m))
                m->state = MBOX_CHECK_SELECT;
            RESET_BUFFER(m);
            break;
        case MBOX_CHECK_SELECT:
            mlog(LOG_DEBUG, "'%s' checking select result\n", m->name);
            if (mbox_read(m, false)) {
                if (imap_check_tag(m)) {
                    if (imap_check_select(m)) {
                        m->state = MBOX_SEND_IDLE;
                    } else {
                        mlog(LOG_DEBUG, "'%s' Failed to select INBOX (got %s)\n", m->name, m->buf);
                        handle_failure(m);
                    }
                }
            }
            break;
        case MBOX_SEND_IDLE:
            mlog(LOG_DEBUG, "'%s' sending IDLE for server\n", m->name);
            if (mbox_send_idle(m))
                m->state = MBOX_CHECK_IDLE;
            RESET_BUFFER(m);
            break;
        case MBOX_CHECK_IDLE:
            if (mbox_read(m, false)) {
                if (imap_check_idle(m)) {
                    mlog(LOG_DEBUG, "'%s' Entering IDLE state\n", m->name);
                    m->state = MBOX_IDLE;
                    m->re_idle_in = MIN2MS(m->idle_timeout);
                    mbox_run_sync(m);
                    RESET_BUFFER(m);
                }
                else
                    mlog(LOG_ERR, "'%s' Waiting for IDLE response from server\n", m->name);
            }
            break;
        case MBOX_IDLE:
            if (mbox_read(m, false)) {
                if (!imap_is_keep_alive(m)) {
                    mlog(LOG_DEBUG, "'%s' Running sync command\n", m->name);
                    mbox_run_sync(m);
                }
                RESET_BUFFER(m);
            } else {
                COUNTDOWN(m->re_idle_in, MIN2MS(m->idle_timeout));
                if (m->re_idle_in == 0) {
                    mlog(LOG_DEBUG, "'%s' Sending done \n", m->name);
                    m->state = MBOX_CHECK_DONE;
                    mbox_send_done(m);
                    RESET_BUFFER(m);
                }
            }
            break;
        case MBOX_CHECK_DONE:
            if (mbox_read(m, false)) {
                mlog(LOG_DEBUG, "'%s' Check response to DONE command '%s'\n", m->name, m->buf);
                if (imap_is_idle_completed(m)) {
                    mlog(LOG_DEBUG, "'%s' IDLE completed\n", m->name);
                    m->state = MBOX_SEND_IDLE;
                }
                RESET_BUFFER(m);
            }
            break;
        default:
            break;
    }
}

bool mbox_init_ssl(void) {

    ssl_ctx = SSL_CTX_new(TLS_client_method());

    if (ssl_ctx == NULL) {
        mlog(LOG_ERR, "Failed to create the SSL_CTX\n");
        return false;
    }

    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL);

    if (!SSL_CTX_set_default_verify_paths(ssl_ctx)) {
        mlog(LOG_ERR, "Failed to set the default trusted certificate store\n");
        goto end;
    }

    /*
     * TLSv1.1 or earlier are deprecated by IETF and are generally to be
     * avoided if possible. We require a minimum TLS version of TLSv1.2.
     */
    if (!SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION)) {
        mlog(LOG_ERR, "Failed to set the minimum TLS protocol version\n");
        goto end;
    }

    return true;

end:
    SSL_CTX_free(ssl_ctx);
    return false;
}


void mbox_shutdown_ssl(struct mbox *m) {
    int ret;

    if (m->state != MBOX_DISABLED && m->ssl) {
        while ((ret = SSL_shutdown(m->ssl)) != 1) {
            if (ret < 0)
                break;
        }
    }
}

void mbox_free_ssl(void) {

    if (ssl_ctx)
        SSL_CTX_free(ssl_ctx);
}


