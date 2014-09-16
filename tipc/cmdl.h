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

#ifndef _CMDL_H
#define _CMDL_H

#include <sys/queue.h>
#include <getopt.h>

TAILQ_HEAD(cmd_head, cmd_struct);
TAILQ_HEAD(cmd_head_option, cmd_option);

struct cmd_option {
	char *key;
	char *val;
	char *usage;
	char *desc;

	TAILQ_ENTRY(cmd_option) list;
};

struct arg_struct {
	int loc;
	int argc;
	char **argv;
};

struct cmd_struct {
	int (*func)(struct cmd_struct *, struct arg_struct *);
	int *optc;
	char *name;
	char *usage;
	char *desc;
	struct cmd_head *children;
	struct cmd_head_option options;

	TAILQ_ENTRY(cmd_struct) list;
};

#define ADD_SUBCMD(rootlist, sublist, cliname, clidesc) \
	struct cmd_head sublist; \
	static int _main_##cliname##_(struct cmd_struct *cmd, \
				      struct arg_struct *arg) { \
		return run_cmdline(&sublist, arg); \
	} \
	static struct cmd_struct _cmd_##cliname##_ = { \
		.name = #cliname, \
		.usage = NULL, \
		.func = _main_##cliname##_, \
		.desc = clidesc, \
		.children = &sublist, \
	}; \
	static void __attribute__((constructor)) _add_cmd_##sublist##_(void) \
	{ \
		add_cmd(rootlist, &_cmd_##cliname##_); \
	} \
	struct cmd_head sublist = TAILQ_HEAD_INITIALIZER(sublist)

#define ADD_CMD(clilist, cliname, clifunc, cliusage, clidesc) \
	static struct cmd_struct _cmd_##cliname##_##clifunc##_;	\
	static void __attribute__((constructor)) \
		    _add_cmd_##cliname##_##clifunc##_(void) \
	{ \
		add_cmd(clilist, &_cmd_##cliname##_##clifunc##_); \
	} \
	static struct cmd_struct _cmd_##cliname##_##clifunc##_ = { \
		.name = #cliname, \
		.usage = cliusage, \
		.func = clifunc, \
		.desc = clidesc, \
		.children = NULL, \
		.options = TAILQ_HEAD_INITIALIZER(_cmd_##cliname##_##clifunc##_.options), \
	}

#define ADD_OPT(cliname, clifunc, clikey, cliusage, clidesc) \
	static struct cmd_option _opt_##cliname##_##clifunc##_##clikey##_ = { \
		.key = #clikey, \
		.usage = cliusage, \
		.desc = clidesc, \
	}; \
	static void __attribute__((constructor)) \
		    _add_opt_##cliname##_##clifunc##_##clikey##_(void) \
	{ \
		add_opt(&_cmd_##cliname##_##clifunc##_, \
			&_opt_##cliname##_##clifunc##_##clikey##_); \
	}

int run_cmdline(struct cmd_head *cli_list, struct arg_struct *arg);
void cmd_help(struct cmd_struct *cmd, struct arg_struct *arg);
void usage_list(struct cmd_head *cmd_list);

int add_cmd(struct cmd_head *cmd_list, struct cmd_struct *cmd);
int add_opt(struct cmd_struct *cmd, struct cmd_option *opt);

int has_extra_arg(struct arg_struct *arg);
int arg_str_search(struct arg_struct *arg, char *string);

int opt_parse(struct cmd_struct *cmd, struct arg_struct *arg);
struct cmd_option *get_opt(struct cmd_struct *cmd, char *option);

#endif
