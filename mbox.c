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
#include <stdbool.h>
#include <assert.h>
#include <sys/queue.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include <ctype.h>

#include "mbox.h"
#include "common.h"

#define CONFIG_FNAME "mbimapidlerc"

TAILQ_HEAD(tailhead, mbox) mbox_head;

static char* trim(char *str) {
    char *end;

    while (isspace(*str)) str++;

    end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) end--;
    *(end+1) = '\0';

    return str;
}

static bool check_key_value(char *line, char **key, char **val) {
    size_t key_len = 0;
    uint32_t quote1_pos = 0;
    uint32_t quote2_pos = 0;
    bool eq_found = false;
    bool val_found = false;
    int idx = 0;

    /* Sanity checks */
    if (line[idx] == '=' || line[idx] == '[' || line[idx] == ']')
        return false;

    do {
        if (eq_found) {
            if (isspace(line[idx])) continue;
            if (quote1_pos == 0) {
                if (line[idx] == '"') {
                    quote1_pos = idx;
                    continue;
                }
                else
                    return false;
            }

            if (line[idx] == '"' && line[idx - 1] != '\\' && quote1_pos != 0) {
                if (quote2_pos != 0)
                    return false;
                quote2_pos = idx;
            }
        } /* eq_found */

        if (line[idx] != '=' && !eq_found)
            key_len++;
        else
            eq_found = true;

    } while(line[idx++]);

    if (quote1_pos && quote2_pos)
        val_found = true;

    if (!eq_found || !val_found)
        return false;

    assert(key_len > 0);
    assert(quote2_pos > quote1_pos - 1);

    *val = malloc(quote2_pos - quote1_pos);
    memset (*val, 0, quote2_pos - quote1_pos);
    strncpy(*val, line + quote1_pos + 1, quote2_pos - quote1_pos - 1);

    *key = malloc(key_len);
    memset(*key, 0, key_len);
    strncpy(*key, line, key_len);

    *key = trim(*key);

    return true;
}

static bool validate_general_config(char *key, char *val) {
    if (!strncmp(key, "verbose", 7))
            if (!strncmp(val, "true", 4) || !strncmp(val, "false", 5))
                return true;
    return false;
}

#undef UINT16_MAX
#define UINT16_MAX 0xffff

static bool validate_block_config(struct mbox *m, char *key, char *val) {

    assert(m != NULL);

    /* Default values */
    m->idle_timeout = 10;

    if (!strncmp(key, "hostname", 8)) {
        m->hostname = strdup(val);
        return true;
    }

    if (!strncmp(key, "username", 8)) {
        m->username = strdup(val);
        return true;
    }

    if (!strncmp(key, "password", 8)) {
        m->password = strdup(val);
        return true;
    }

    if (!strncmp(key, "sync_cmd", 8)) {
        val = trim(val);
        return parse_cmd(m->name, val, &m->sync_cmd, &m->sync_args);
    }

    if (!strncmp(key, "port", 4)) {
        m->port = strtoimax(val, NULL, 10);
        if (m->port == 0) {
            mlog(LOG_ERR,"Invalid port format for '%s'\n", m->name);
            return false;
        }
        return true;
    }

    if (!strncmp(key, "idle_timeout", 12)) {
        m->idle_timeout = strtoimax(val, NULL, 10);
        if (m->idle_timeout < 5 || m->idle_timeout > 29) {
            mlog(LOG_ERR,"'%s' idle_timeout allowed values >= 5m and <= 29m \n", m->name);
            return false;
        }
        return true;
    }

    if (!strncmp(key, "check_certificate", 17)) {
        val = trim(val);
        if (!strncmp(val, "false", 5) || !strncmp(val, "no", 2)) {
            m->check_cert = false;
            return true;
        }
        else if (!strncmp(val, "true", 4) || !strncmp(val, "yes", 3)) {
            m->check_cert = true;
            return true;
        }
        else {
            mlog(LOG_ERR, "'%s' Unexpected value '%s' for check_certificate key\n", m->name, val);
            return false;
        }
    }

    if (!strncmp(key, "pass_cmd", 8)) {
        val = trim(val);
        return parse_cmd(m->name, val, &m->pass_cmd, &m->pass_args);
    }

    if (!strncmp(key, "tls_type", 8)) {
        val = trim(val);
        if (!strncmp(val, "none", 4)) {
            m->tls_type = TLS_TYPE_NONE;
            return true;
        } else if (!strncmp(val, "starttls", 8)) {
            m->tls_type = TLS_TYPE_STARTTLS;
            return true;
        } else if (!strncmp(val, "ssl", 3)) {
            m->tls_type = TLS_TYPE_SSL;
            return true;
        } else {
            mlog(LOG_ERR, "'%s' invalid ssl type '%s'\n", m->name, val);
            return false;
        }
    }

    if (!strncmp(key, "auth", 4)) {
        val = trim(val);
        if (!strncmp(val, "plain", 5)) {
            m->auth_type = AUTH_TYPE_PLAIN;
            return true;
        } else if (!strncmp(val, "XOAUTH2", 7)) {
            m->auth_type = AUTH_TYPE_XOAUTH2;
            return true;
        } else {
            mlog(LOG_ERR, "'%s' invalid auth method '%s'\n", m->name, val);
            return false;
        }
    }

    return false;
}


static bool check_mbox_fields(struct mbox *m) {

    if (!m->hostname) {
        mlog(LOG_ERR,"Missing hostname from config '%s'\n", m->name);
        return false;
    }

    if (!m->username) {
        mlog(LOG_ERR,"Missing username from config '%s'\n", m->name);
        return false;
    }

    if (!m->sync_cmd) {
        mlog(LOG_ERR,"Missing sync_cmd from config '%s'\n", m->name);
        return false;
    }

    if (m->password && m->pass_cmd) {
        mlog(LOG_ERR, "Password or password cmd are configured on %s\n", m->name);
        return false;
    }

    if (!m->password && !m->pass_cmd) {
        mlog(LOG_ERR,"Missing password or password command from config '%s'\n", m->name);
        return false;
    }

    if (m->port == 0) {
        mlog(LOG_ERR,"Invalid port:%d '%s'\n", m->port, m->name);
        return false;
    }

    /* Unless tls_type is set to 'none', we set it here to:
     *  SSL       port 993
     *  STARTTLS  port 143
     */
    if (m->tls_type == TLS_TYPE_INVALID) {
        if (m->port == 143) {
            m->tls_type = TLS_TYPE_STARTTLS;
            mlog(LOG_WARN, "'%s' tls_type not specified, assuming STARTTLS on port %d\n", m->name, m->port);
        }
        else if (m->port == 993) {
            m->tls_type = TLS_TYPE_SSL;
            mlog(LOG_WARN, "'%s' tls_type not specified, assuming SSL on port %d\n", m->name, m->port);
        }
        else {
            mlog(LOG_ERR, "'%s' Please set tls_type to 'none, 'ssl' or 'starttls' in config\n");
        }
    }

    return true;
}

bool conf_init() {

    FILE *config;
    ssize_t rc;
    bool in_block, in_general;
    char *p = NULL;
    char *line = NULL;
    struct mbox *m = NULL;
    bool general_found = false;
    int linenum = 0;
    int idx = 0;
    size_t linecap = 0;
    int num_mbox = 0;

    TAILQ_INIT(&mbox_head);

    p = get_conf_file_path();

    if (!p) return false;

    config = fopen(p, "r");

    if (!config) {
        mlog(LOG_ERR,"Failed to load configuration file '%s'\n", strerror(errno));
        return false;
    }

    if (p) free(p);
    p = NULL;

    in_general = false;
    in_block = false;
    while ((rc = getline(&line, &linecap, config)) != -1) {

        char *key = NULL, *val = NULL;
        int i = 0;
        linenum++;
        idx = 0;
        p = malloc(rc);
        memset(p, 0, rc);
        if (rc == 1 && line[0] == '\n') continue;

        /* Skip leading whitespaces */
        while (line[idx] == ' ') {
            idx++;
        }

        /* Line is a comment, skip it*/
        if (line[idx] == '#') continue;

        do {
            if (line[idx] != '\n' && line[idx] != '\0') {
                /* Comment such as blabla # This is a comment */
                if (line[idx] == '#' && idx > 2 && line[idx-1] == ' ') break;
                p[i++] = line[idx];
            }
        } while (++idx < rc);

        p[i] = '\0';

        if (!strncmp(p, "[general]", 9)) {
            /* We are already in the general section */
            if (in_general)
                goto end;
            if (general_found) {
                mlog(LOG_ERR, "Duplicated general section found\n");
                goto end;
            }
            in_general = true; in_block = false;
            general_found = true;
            continue;
        /* Opening of a mbox block */
        } else if (p[0] == '[') {
            if (p[strlen(p)-1] != ']' || strlen(p) < 3)
                goto end;

            m = (struct mbox*)malloc(sizeof(struct mbox));
            memset(m, 0, sizeof(struct mbox));
            /* strip off '[' and ']' add 'MBOX: ' */
            m->name = malloc(strlen(p) + 6);
            memset(m->name, 0, strlen(p));
            memcpy(m->name, "MBOX: ", 6);
            strncpy(m->name + 6, p + 1, strlen(p) - 2);

            m->check_cert = true;
            m->auth_type = AUTH_TYPE_PLAIN;
            m->state_timeout = SEC_MS(10);

            in_block = true; in_general = false;
            TAILQ_INSERT_HEAD(&mbox_head, m, mboxes);
            continue;
        }

        if (in_general || in_block) {
            /* Check for key=value */
            if (!check_key_value(p, &key, &val))
                goto end;
        } else {
            mlog(LOG_ERR, "Unexpected outside of block configuration %s\n", p);
            goto end;
        }

        if (in_general && !validate_general_config(key, val))
            goto end;
        if (in_block && !validate_block_config(m, key, val))
            goto end;
        free(p);
        p = NULL;
    }

    TAILQ_FOREACH(m, &mbox_head, mboxes) {
        num_mbox++;
        if (!check_mbox_fields(m)) {
            mlog(LOG_ERR,"Incomplete configuration for mbox '%s'\n", m->name);
            goto end;
        }
        m->buf_size = 1024;
        m->buf = malloc(m->buf_size);
        mlog(LOG_INFO, "'%s' configuration loaded\n", m->name);
    }

    if (num_mbox == 0) {
        mlog(LOG_ERR,"No imap mail found in configuration\n");
        goto end;
    }

    fclose(config);
    return true;
end:
    mlog(LOG_ERR, "Syntax error near '%s' line %d\n", p, linenum);
    if (p != NULL) free(p);
    fclose(config);

    return false;
}

void mbox_foreach(mbox_conn func) {
    struct mbox *m;
    TAILQ_FOREACH(m, &mbox_head, mboxes) {
        (func)(m);
    }
}

void mbox_remove_all(void) {
    struct mbox *m;
    struct mbox *tmp;
    TAILQ_FOREACH_SAFE(m, &mbox_head, mboxes, tmp) {
        TAILQ_REMOVE(&mbox_head, m, mboxes);
        mbox_free(m);
    }
}

void mbox_run_sync (struct mbox *m) {
    if (m->sync_pid > 0) {
        mlog(LOG_DEBUG,"'%s' sync command is still running\n", m->name);
        return;
    }

    if ((m->sync_pid = fork()) < 0) {
        mlog(LOG_ERR, "Failed call fork %s\n", strerror(errno));
        return;
    }

    if (m->sync_pid == 0) {
        int ret = execvp(m->sync_cmd, m->sync_args);
        if (ret == -1) {
            mlog(LOG_ERR, "'%s' Failed to run sync command '%s':'%s'\n", m->name, m->sync_cmd, strerror(errno));
            exit(-1);
        }
    }
}

#define FREE_STR(ptr) \
    if (ptr != NULL) { \
        free(ptr); \
        ptr = NULL; \
    }

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

/* Close connection to retry again */
void mbox_free_conn(struct mbox *m) {
    if (m->ssl) {
        SSL_free(m->ssl);
        m->ssl = NULL;
    }
    if (m->bio) {
        BIO_free_all(m->bio);
        m->bio = NULL;
    }
    if (m->sock) {
        close(m->sock);
        m->sock = -1;
    }
    memset(m->buf, 0, m->buf_size);
    m->buf_len = 0;
}

void mbox_free(struct mbox *m) {
   mbox_free_conn(m);

   FREE_STR(m->name);
   FREE_STR(m->hostname);
   FREE_STR(m->username);
   FREE_STR(m->sync_cmd);
   FREE_STR(m->password);
   FREE_STR(m->buf);

   FREE_STRV(m->sync_args);
   FREE_STRV(m->pass_args);

   free(m);
}
