/*
 * Copyright (c) 2014, Ericsson AB
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "cmdl.h"
#include "tipc.h"

static void usage(struct cmd_struct *cmd, char *argv[], int loc)
{
	int i;

	fprintf(stderr, "Usage: %s", argv[0]);
	for (i = optind; i < loc; i++)
		fprintf(stderr, " %s", argv[i]);
	if (cmd)
		fprintf(stderr, " %s", cmd->usage);
	fprintf(stderr, "\n");
}

int add_cmd(struct cmd_head *cmd_head, struct cmd_struct *cmd)
{
	int i;
	struct cmd_struct *p;

	/* We sort lexicographically to be able to use abbreviation matching */
	TAILQ_FOREACH(p, cmd_head, list) {
		i = strcmp(p->name, cmd->name);
		if (i > 0) {
			TAILQ_INSERT_BEFORE(p, cmd, list);
			return 0;
		}
	}
	TAILQ_INSERT_TAIL(cmd_head, cmd, list);
	return 0;
}

struct cmd_struct *find_cmd(const struct cmd_head * const cmd_list,
			    const char * const name)
{
	size_t l;
	struct cmd_struct *cmd, *next;

	if (!name)
		return NULL;

	l = strlen(name);

	TAILQ_FOREACH(cmd, cmd_list, list) {
		if (strstr(cmd->name, name) == cmd->name) {
			if (l == strlen(cmd->name))
				return cmd;

			next = TAILQ_NEXT(cmd, list);
			if (next && strstr(next->name, name) == next->name)
				return NULL;

			return cmd;
		}
	}

	return NULL;
}

int add_opt(struct cmd_struct *cmd, struct cmd_option *opt)
{
	int i;
	struct cmd_option *cmd_opt;

	/* We sort lexicographically to be able to use abbreviation matching */
	TAILQ_FOREACH(cmd_opt, &cmd->options, list) {
		i = strcmp(cmd_opt->key, opt->key);
		if (i > 0) {
			TAILQ_INSERT_BEFORE(cmd_opt, opt, list);
			return 0;
		}
	}
	TAILQ_INSERT_TAIL(&cmd->options, opt, list);
	return 0;
}

static void opt_help(struct cmd_struct *cmd)
{
	struct cmd_option *opt;

	if (!TAILQ_EMPTY(&cmd->options))
		fprintf(stderr, "\nOptions:\n");

	TAILQ_FOREACH(opt, &cmd->options, list) {
		fprintf(stderr, " %-15s %-25s %s\n",
				opt->key, opt->usage, opt->desc);
	}
}

static int opt_fill(struct cmd_struct *cmd, char *key, char *val)
{
	struct cmd_option *cmd_opt;

	TAILQ_FOREACH(cmd_opt, &cmd->options, list) {
		if (strstr(cmd_opt->key, key) == cmd_opt->key) {
			/* We make the assumption that argv will not change */
			cmd_opt->val = val;
			return 0;
		}
	}
	return -EINVAL;
}

int opt_parse(struct cmd_struct *cmd, struct arg_struct *arg)
{
	int i;
	int err;
	int cnt = 0;

	for (i = arg->loc; i < arg->argc; i += 2) {
		if (!arg->argv[i + 1])
			return -EINVAL;

		err = opt_fill(cmd, arg->argv[i], arg->argv[i + 1]);
		if (err) {
			log_err("unknown option \"%s\"\n", arg->argv[i]);
			return -EINVAL;
		}
		cnt++;
	}
	return cnt;
}

struct cmd_option *get_opt(struct cmd_struct *cmd, char *option)
{
	struct cmd_option *cmd_opt;

	TAILQ_FOREACH(cmd_opt, &cmd->options, list) {
		if (strcmp(cmd_opt->key, option) == 0 && cmd_opt->val)
			return cmd_opt;
	}
	return NULL;
}

void cmd_help(struct cmd_struct *cmd, struct arg_struct *arg)
{
	usage(cmd, arg->argv, arg->loc);
	opt_help(cmd);
}

void usage_list(struct cmd_head *cmd_list)
{
	struct cmd_struct *cmd;

	TAILQ_FOREACH(cmd, cmd_list, list) {
		fprintf(stderr, " %-15s", cmd->name);
		if (cmd->usage)
			fprintf(stderr, " %-25s ", cmd->usage);
		else
			fprintf(stderr, " %-25s ", "...");
		fprintf(stderr, "%s\n", cmd->desc);
	}
}

int run_cmdline(struct cmd_head *cmd_list, struct arg_struct *arg)
{
	int err;
	struct cmd_struct *cmd;

	if (arg->argc == arg->loc) {
		usage(NULL, arg->argv, arg->loc);
		usage_list(cmd_list);
		return -1;
	}

	cmd = find_cmd(cmd_list, arg->argv[arg->loc]);
	if (!cmd) {
		log_err("unknown bareword \"%s\"\n\n", arg->argv[arg->loc]);
		usage(NULL, arg->argv, arg->loc);
		usage_list(cmd_list);
		return -1;
	}

	(arg->loc)++;

	/* Last child */
	if (!cmd->children) {
		if (help_flag) {
			cmd_help(cmd, arg);
			return -1;
		}
	}

	err = cmd->func(cmd, arg);
	if (err) {
		if (!cmd->children && err == -EINVAL)
			cmd_help(cmd, arg);

		return err;
	}

	return 0;
}

int has_extra_arg(struct arg_struct *arg)
{
	return (arg->argc > arg->loc);
}

int arg_str_search(struct arg_struct *arg, char *string)
{
	int i;

	for (i = arg->loc; i < arg->argc; i++) {
		if (strcmp(arg->argv[i], string) == 0)
			return 1;
	}
	return 0;
}
