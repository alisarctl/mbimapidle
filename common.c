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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#include "common.h"

#define CONFIG_FNAME "mbimapidlerc"
#define CONFIG_DNAME "mbimapidle"
#define CONFIG_FIFO  "mbimapidle"

static char *get_conf_dir_path(void) {
    char *config_home;
    char *tmp;
    char *home = getenv("HOME");
    char *p = NULL;
    size_t config_path_len = 0;

    if (!home) {
        mlog(LOG_ERR,"HOME env is not set aborting\n");
        goto out;
    }

    tmp = getenv("XDG_CONFIG_HOME");

    if(!tmp) {
        mlog(LOG_INFO,"XDG_CONFIG_HOME not set, assuming ~/.config\n");
        config_home = malloc(strlen(home) + strlen("/.config") + 1);
        sprintf(config_home, "%s/.config", home);
    } else {
        config_home = strdup(tmp);
    }

    config_path_len = strlen(home) + strlen(config_home);
    /* count also a '/' + '\0' */
    config_path_len += strlen(CONFIG_DNAME) + 2;

    assert (config_path_len < PATH_MAX);

    p = malloc(config_path_len);

    sprintf(p, "%s/%s", config_home, CONFIG_DNAME);
    free(config_home);
out:
    return p;
}

static char *get_fpath(const char *fname) {
    char *conf_dir_path;
    char *fp;

    conf_dir_path = get_conf_dir_path();

    assert (strlen(conf_dir_path) + strlen(fname) + 1 < PATH_MAX);
    fp = malloc (strlen(conf_dir_path) + strlen(fname) + 2);

    sprintf(fp, "%s/%s", conf_dir_path, fname);
    free(conf_dir_path);
    return fp;
}

char *get_conf_file_path(void) {
    return get_fpath (CONFIG_FNAME);
}

char *get_conf_fifo_path(void) {
    return get_fpath (CONFIG_FIFO);
}

