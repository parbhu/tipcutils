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
#include <unistd.h>
#include <errno.h>

#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/attr.h>

#include <linux/tipc_netlink.h>

#include "netlink.h"
#include "cmdl.h"
#include "tipc.h"

ADD_SUBCMD(&cmd_list, cmd_media, media, "Show or manage media");
ADD_SUBCMD(&cmd_media, cmd_media_prio, priority, "Get or set media priority");
ADD_SUBCMD(&cmd_media, cmd_media_tol, tolerance, "Get or set media tolerance");
ADD_SUBCMD(&cmd_media, cmd_media_win, window, "Get or set media window size");

static int _list(struct nl_msg *nl_msg, void *arg)
{
	struct nlmsghdr *nlh = nlmsg_hdr(nl_msg);
	struct nlattr *info[TIPC_NLA_MAX + 1];
	struct nlattr *attrs[TIPC_NLA_MEDIA_MAX + 1];

	genlmsg_parse(nlh, 0, info, TIPC_NLA_MAX, NULL);

	if (!info[TIPC_NLA_MEDIA]) {
		log_err("no media received from kernel\n");
		return NL_STOP;
	}

	if (nla_parse_nested(attrs, TIPC_NLA_MEDIA_MAX,
			     info[TIPC_NLA_MEDIA], NULL)) {
		log_err("parsing nested media attributes\n");
		return NL_STOP;
	}

	if (!attrs[TIPC_NLA_MEDIA_NAME]) {
		log_err("no media name received from kernel\n");
		return NL_STOP;
	}

	printf("%s\n", nla_get_string(attrs[TIPC_NLA_MEDIA_NAME]));

	return NL_OK;
}

static int list()
{
	int err;
	struct tipc_nl_msg msg;

	msg.nl_flags = NLM_F_REQUEST | NLM_F_DUMP;
	msg.nl_cmd = TIPC_NL_MEDIA_GET;
	msg.callback = &_list;
	err = msg_init(&msg);
	if (err)
		return err;

	err = msg_send(&msg);
	if (err)
		return err;

	err = msg_recv(&msg, NULL);
	if (err)
		return err;

	return 0;
}

static int print_prop(struct nl_msg *nl_msg, void *arg)
{
	int *prop = arg;
	struct nlmsghdr *nlh = nlmsg_hdr(nl_msg);
	struct nlattr *info[TIPC_NLA_MAX + 1];
	struct nlattr *attrs[TIPC_NLA_MEDIA_MAX + 1];
	struct nlattr *props[TIPC_NLA_PROP_MAX + 1];

	genlmsg_parse(nlh, 0, info, TIPC_NLA_MAX, NULL);

	if (!info[TIPC_NLA_MEDIA]) {
		log_err("no media received from kernel\n");
		return NL_STOP;
	}

	if (nla_parse_nested(attrs, TIPC_NLA_MEDIA_MAX,
			     info[TIPC_NLA_MEDIA], NULL)) {
		log_err("parsing nested media attributes\n");
		return NL_STOP;
	}

	if (!attrs[TIPC_NLA_MEDIA_PROP]) {
		log_err("no media properties received from kernel\n");
		return NL_STOP;
	}

	if (nla_parse_nested(props, TIPC_NLA_PROP_MAX,
			     attrs[TIPC_NLA_MEDIA_PROP], NULL)) {
		log_err("parsing nested media properties\n");
		return NL_STOP;
	}

	if (!props[*prop]) {
		log_err("property not received from kernel\n");
		return NL_STOP;
	}

	if (props[*prop])
		printf("%u\n", nla_get_u32(props[*prop]));

	return NL_OK;
}

static int get_prop(struct arg_struct *arg, int prop)
{
	int err;
	char *media;
	struct tipc_nl_msg msg;
	struct nlattr *attrs;

	/* One mandatory (media) */
	if (arg->argc < arg->loc + 1)
		return -EINVAL;

	media = arg->argv[arg->loc];

	msg.nl_flags = NLM_F_REQUEST;
	msg.nl_cmd = TIPC_NL_MEDIA_GET;
	msg.callback = &print_prop;
	err = msg_init(&msg);
	if (err)
		return err;

	attrs = nla_nest_start(msg.nl_msg, TIPC_NLA_MEDIA);
	NLA_PUT_STRING(msg.nl_msg, TIPC_NLA_MEDIA_NAME, media);
	nla_nest_end(msg.nl_msg, attrs);

	err = msg_send(&msg);
	if (err)
		return err;

	err = msg_recv(&msg, &prop);
	if (err)
		return err;

	return 0;

nla_put_failure:
	msg_abort(&msg);

	return -ENOBUFS;
}

static int set_prop(struct arg_struct *arg, int nla, char *name)
{
	int err;
	char *media;
	struct tipc_nl_msg msg;
	struct nlattr *attrs;
	struct nlattr *prop;
	__u32 val;

	/* Two mandatory (media, attribute) */
	if (arg->argc < arg->loc + 2)
		return -EINVAL;

	media = arg->argv[arg->loc];
	val = atoi(arg->argv[arg->loc + 1]);

	msg.nl_flags = NLM_F_REQUEST;
	msg.nl_cmd = TIPC_NL_MEDIA_SET;
	err = msg_init(&msg);
	if (err)
		return err;

	attrs = nla_nest_start(msg.nl_msg, TIPC_NLA_MEDIA);
	NLA_PUT_STRING(msg.nl_msg, TIPC_NLA_MEDIA_NAME, media);

	prop = nla_nest_start(msg.nl_msg, TIPC_NLA_BEARER_PROP);
	NLA_PUT_U32(msg.nl_msg, nla, val);
	nla_nest_end(msg.nl_msg, prop);

	nla_nest_end(msg.nl_msg, attrs);

	err = msg_send(&msg);
	if (err)
		return err;

	err = msg_recv(&msg, NULL);
	if (err)
		return err;

	log_info("Media %s %s set to %u\n", media, name, val);

	return 0;

nla_put_failure:
	msg_abort(&msg);

	return -ENOBUFS;
}

static int set_prio(struct cmd_struct *cmd, struct arg_struct *arg)
{
	return set_prop(arg, TIPC_NLA_PROP_PRIO, "priority");
}

static int set_tol(struct cmd_struct *cmd, struct arg_struct *arg)
{
	return set_prop(arg, TIPC_NLA_PROP_TOL, "tolerance");
}

static int set_win(struct cmd_struct *cmd, struct arg_struct *arg)
{
	return set_prop(arg, TIPC_NLA_PROP_WIN, "window size");
}

static int get_prio(struct cmd_struct *cmd, struct arg_struct *arg)
{
	return get_prop(arg, TIPC_NLA_PROP_PRIO);
}

static int get_tol(struct cmd_struct *cmd, struct arg_struct *arg)
{
	return get_prop(arg, TIPC_NLA_PROP_TOL);
}

static int get_win(struct cmd_struct *cmd, struct arg_struct *arg)
{
	return get_prop(arg, TIPC_NLA_PROP_WIN);
}

ADD_CMD(&cmd_media, list, list, "", "List media");

ADD_CMD(&cmd_media_prio, set, set_prio, "<media> <priority>",
	"Set media priority");
ADD_CMD(&cmd_media_prio, get, get_prio, "<media>",
	"Get media priority");

ADD_CMD(&cmd_media_tol, set, set_tol, "<media> <tolerance>",
	"Set media tolerance");
ADD_CMD(&cmd_media_tol, get, get_tol, "<media>",
	"Get media tolerance");

ADD_CMD(&cmd_media_win, set, set_win, "<media> <window>",
	"Set media window size");
ADD_CMD(&cmd_media_win, get, get_win, "<media>",
	"Get media window size");
