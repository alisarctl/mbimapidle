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
#include <ctype.h>
#include <assert.h>
#include <libgen.h>

#include "common.h"

#define CONFIG_FNAME "mbimapidlerc"
#define CONFIG_DNAME "mbimapidle"
#define CONFIG_FIFO  "mbimapidle"

static char *get_conf_dir_path(void)
{
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

static char *get_fpath(const char *fname)
{
	char *conf_dir_path;
	char *fp;

	conf_dir_path = get_conf_dir_path();

	assert (strlen(conf_dir_path) + strlen(fname) + 1 < PATH_MAX);
	/* adding 2: slash + '\0' */
	fp = malloc (strlen(conf_dir_path) + strlen(fname) + 2);

	sprintf(fp, "%s/%s", conf_dir_path, fname);
	free(conf_dir_path);
	return fp;
}

char *get_conf_file_path(void)
{
	return get_fpath (CONFIG_FNAME);
}

bool parse_cmd (const char *mbox_name, char *val, char **cmd, char **argv[])
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
		mlog(LOG_ERR, "'%s' command is too short\n", mbox_name);
		return false;
	}

	sub = val;
	do {
		if (!isspace(*sub) && !isalnum(*sub) && *sub != '/' && *sub != '-' && *sub != '\0' &&
				*sub != '.') {
			mlog(LOG_ERR, "'%s' Invalid character %c\n", mbox_name, *sub);
			return false;
		}
	} while(*sub++);

	sub = val;
	while (!isspace(*sub++)) {cmd_len++;}
	if (val[cmd_len - 1] == '/') {
		mlog(LOG_ERR, "'%s' invalid slash terminated command '%s' \n", mbox_name, val);
		return false;
	}

	path = strdup(getenv("PATH"));
	tmp = strdup(val);
	dirn = dirname(tmp);

	if (strlen(dirn) > strlen(path)) {
		mlog(LOG_ERR, "'%s' invalid command length '%s' \n", mbox_name, val);
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
		mlog(LOG_ERR, "'%s' command '%s' not found in $PATH\n",
				mbox_name, val);
		goto out;
	}

	free(tmp);
	tmp = strdup(val);
	basen = basename(tmp);

	if (basen[0] == '/' || basen[0] == '.' || basen[0] == '\\') {
		mlog(LOG_ERR, "'%s' invalid command '%s' \n", mbox_name, val);
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

	*cmd = malloc(cmd_len + 1);
	memset(*cmd, 0, cmd_len + 1);
	strncpy (*cmd, val, cmd_len);

	*argv = args;
	ret = true;
out:
	free(tmp);
	return ret;
}
