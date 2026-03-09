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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/queue.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <libgen.h>

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

static bool check_sync_command(struct mbox *m, char *val)
{
    char **args;
    char *path, *sub, *brkb, *dirn, *basen;
    char *tmp = NULL;
    size_t cmd_len = 0;
    int idx = 0;
    int nargs = 0;
    bool found_in_path = false;
    bool ret = false;

    /* at least /bin/1 */
    if (strlen(val) < 6) {
        mlog(LOG_ERR, "'%s' sync command is too short\n", m->name);
        return false;
    }

    sub = val;
    do {
        if (!isspace(*sub) && !isalnum(*sub) && *sub != '/' && *sub != '-' && *sub != '\0') {
            mlog(LOG_ERR, "'%s' Invalid character %c\n", m->name, *sub);
            return false;
        }
    } while(*sub++);

    sub = val;
    while (!isspace(*sub++)) {cmd_len++;}
    if (val[cmd_len - 1] == '/') {
        mlog(LOG_ERR, "'%s' invalid slash terminated sync command '%s' \n", m->name, val);
        return false;
    }

    path = strdup(getenv("PATH"));
    tmp = strdup(val);
    dirn = dirname(tmp);

    if (strlen(dirn) > strlen(path)) {
        mlog(LOG_ERR, "'%s' invalid sync command length '%s' \n", m->name, val);
        goto out;
    }

    for (sub = strtok_r(path, ":", &brkb);
         sub;
         sub = strtok_r(NULL, ":", &brkb))
    {
        if (strcmp(sub, dirn) == 0) {
            found_in_path = true;
            break;
        }
    }
    free(path);

    if (!found_in_path) {
        mlog(LOG_ERR, "'%s' sync command '%s' not found in $PATH\n",
                      m->name, val);
        goto out;
    }

    free(tmp);
    tmp = strdup(val);
    basen = basename(tmp);

    if (basen[0] == '/' || basen[0] == '.' || basen[0] == '\\') {
        mlog(LOG_ERR, "'%s' invalid sync command '%s' \n", m->name, val);
        goto out;
    }

    sub = basen;

    /* Parse arguments */
    do {if (isspace(*sub)) nargs++;} while(*sub++);

    args = malloc(sizeof(char *) * (nargs + 2));
    for (sub = strtok_r(basen, " ", &brkb);
         sub;
         sub = strtok_r(NULL, " ", &brkb)) {
        args[idx++] = strdup(sub);
    }
    args[idx] = NULL;

    m->sync_cmd = malloc(cmd_len);
    memset(m->sync_cmd, 0, cmd_len);
    strncpy (m->sync_cmd, val, cmd_len);
    m->sync_args = args;
    ret = true;
out:
    free(tmp);
    return ret;
}

static bool check_key_value(char *line, char **key, char **val) {
    size_t key_len = 0;
    uint64_t quote1_pos = 0;
    uint64_t quote2_pos = 0;
    bool eq_found = false;
    bool val_found = false;
    int idx = 0;

    /* Sanity checks */
    if (line[idx] == '=' || line[idx] == '[' || line[idx] == ']')
        return false;

    do {
        if (eq_found) {
            if (line[idx] == ' ') continue;
            if (quote1_pos == 0) {
                if (line[idx] == '"')
                    quote1_pos = idx;
                else
                    return false;
            }

            if (line[idx] == '"' && line[idx - 1] != '\\' && quote1_pos != 0) {
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
        return check_sync_command(m, val);
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
        m->pass_cmd = strdup(val);
        return true;
    }

    if (!strncmp(key, "tls_type", 8)) {
        val = trim(val);
        if (!strncmp(val, "none", 4)) {
            m->tls_type = TLS_TYPE_NONE;
        } else if (!strncmp(val, "starttls", 8)) {
            m->tls_type = TLS_TYPE_STARTTLS;
        } else if (!strncmp(val, "ssl", 3)) {
            m->tls_type = TLS_TYPE_SSL;
        } else {
            mlog(LOG_ERR, "'%s' invalid ssl type '%s'\n", m->name, val);
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
    size_t len;
    bool in_block, in_general;
    char *p = NULL;
    char *line = NULL;
    struct mbox *m = NULL;
    bool general_found = false;
    int linenum = 0;
    int idx = 0;
    size_t linecap = 0;
    int num_mbox = 0;
    char *config_home = getenv("XDG_CONFIG_HOME");
    char *home = getenv("HOME");
    if (!home) {
            mlog(LOG_ERR,"HOME env is not set aborting\n");
            return false;
    }

    TAILQ_INIT(&mbox_head);

    if (!config_home) {
        len = strnlen(home, PATH_MAX);

        /* .config/mbimapidle/mbimapidlerc + '\0' = 32 */
        assert (len < PATH_MAX - 32);

        mlog(LOG_INFO,"XDG_CONFIG_HOME not set, assuming ~/.config\n");
        p = malloc(len + 32);
        memset(p, 0, len + 32);

        sprintf(p, "%s/.config/mbimapidle/%s", home, CONFIG_FNAME);
        config = fopen (p, "r");

    } else {
        len = strnlen(config_home, PATH_MAX);

        /* $XDG_CONFIG_HOME + mbimapidle/mbimapidlerc + '\0' = 32 */
        assert (len < PATH_MAX - 25);

        mlog(LOG_INFO, "Loading configuration %s\n", config_home);
        p = malloc(len + 25);
        memset (p, 0, len + 25);

        sprintf(p, "%s/mbimapidle/%s", config_home, CONFIG_FNAME);
        config = fopen(p, "r");
    }

    if (!config) {
        mlog(LOG_ERR,"Failed to load configuration file '%s'\n", strerror(errno));
        return false;
    }

    if (p) free(p);

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
            m->name = malloc(strlen(p) + 4);
            memset(m->name, 0, strlen(p) + 4);
            memcpy(m->name, "MBOX: ", 6);

            m->check_cert = true;
            m->state_timeout = SEC_MS(10);

            strncpy(m->name + 6, p + 1, strlen(p) - 2);
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

void mbox_run_sync(struct mbox *m) {
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

#define CHECK_FREE(ptr) \
    if (ptr != NULL) { \
        free(ptr); \
        ptr = NULL; \
    }

/* Close connection to retry again */
void mbox_free_conn(struct mbox *m) {
   CHECK_FREE(m->ssl);
   CHECK_FREE(m->bio);
   if (m->sock) close(m->sock);
   memset(m->buf, 0, m->buf_size);
   m->buf_len = 0;
}

void mbox_free(struct mbox *m) {
   char **tmp;
   CHECK_FREE(m->name);
   CHECK_FREE(m->hostname);
   CHECK_FREE(m->username);
   CHECK_FREE(m->sync_cmd);
   CHECK_FREE(m->password);
   CHECK_FREE(m->pass_cmd);
   CHECK_FREE(m->buf);
   CHECK_FREE(m->ssl);
   CHECK_FREE(m->bio);

   for (tmp = m->sync_args; *tmp !=NULL; tmp++) {
       free(*tmp);
   }
   CHECK_FREE(m->sync_args);

   if (m->sock) close(m->sock);
   free(m);
}
