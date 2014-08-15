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
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/attr.h>

#include <linux/tipc_config.h>

#include <errno.h>

#include "netlink.h"
#include "cmdl.h"
#include "tipc.h"
#include "misc.h"

ADD_SUBCMD(&cmd_list, bearer, bearer, "Show or manipulate bearers");
ADD_SUBCMD(&bearer, priority, priority, "Get or set bearer priority");
ADD_SUBCMD(&bearer, tolerance, tolerance, "Get or set bearer tolerance");
ADD_SUBCMD(&bearer, window, window, "Get or set bearer window size");

static int _list(struct nl_msg *nl_msg, void *arg)
{
	struct nlmsghdr *nlh = nlmsg_hdr(nl_msg);
	struct nlattr *info[TIPC_NLA_MAX + 1];
	struct nlattr *attrs[TIPC_NLA_BEARER_MAX + 1];

	genlmsg_parse(nlh, 0, info, TIPC_NLA_MAX, NULL);

	if (!info[TIPC_NLA_BEARER]) {
		log_err("no bearer received from kernel\n");
		return NL_STOP;
	}

	if (nla_parse_nested(attrs, TIPC_NLA_BEARER_MAX,
			     info[TIPC_NLA_BEARER], NULL)) {
		log_err("parsing nested bearer properties\n");
		return NL_STOP;
	}

	if (!attrs[TIPC_NLA_BEARER_NAME]) {
		log_err("no bearer name received from kernel\n");
		return NL_STOP;
	}

	printf("%s\n", nla_get_string(attrs[TIPC_NLA_BEARER_NAME]));

	return NL_OK;
}

static int list()
{
	int err;
	struct tipc_nl_msg msg;

	msg.nl_flags = NLM_F_REQUEST | NLM_F_DUMP;
	msg.nl_cmd = TIPC_NL_BEARER_GET;
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

static int enable(struct cmd_struct *cmd, struct arg_struct *arg)
{
	int cnt;
	int err;
	char *bearer;
	struct tipc_nl_msg msg;
	struct cmd_option *opt;
	struct nlattr *attrs;
	__u32 domain = 0;
	__u32 priority = 0;

	/* One mandatory argument (bearer) */
	if (arg->argc < arg->loc + 1)
		return -EINVAL;

	bearer = arg->argv[arg->loc];
	(arg->loc)++;

	cnt = opt_parse(cmd, arg);
	if (cnt < 0)
		return -EINVAL;

	msg.nl_flags =  NLM_F_REQUEST;
	msg.nl_cmd = TIPC_NL_BEARER_ENABLE;
	err = msg_init(&msg);
	if (err)
		return err;

	attrs = nla_nest_start(msg.nl_msg, TIPC_NLA_BEARER);
	NLA_PUT_STRING(msg.nl_msg, TIPC_NLA_BEARER_NAME, bearer);

	opt = get_opt(cmd, "priority");
	if (opt) {
		struct nlattr *prop;

		priority = atoi(opt->val);
		prop = nla_nest_start(msg.nl_msg, TIPC_NLA_BEARER_PROP);
		NLA_PUT_U32(msg.nl_msg, TIPC_NLA_PROP_PRIO, priority);
		nla_nest_end(msg.nl_msg, prop);
	}

	opt = get_opt(cmd, "domain");
	if (opt) {
		domain = str2addr(opt->val);
		if (!domain) {
			msg_abort(&msg);
			return -1;
		}

		NLA_PUT_U32(msg.nl_msg, TIPC_NLA_BEARER_DOMAIN, domain);
	}

	nla_nest_end(msg.nl_msg, attrs);

	err = msg_send(&msg);
	if (err)
		return err;

	err = msg_recv(&msg, NULL);
	if (err)
		return err;

	log_info("Bearer %s enabled\n", bearer);

	return 0;

nla_put_failure:
	msg_abort(&msg);

	return -ENOBUFS;
}

static int disable(struct cmd_struct *cmd, struct arg_struct *arg)
{
	int err;
	char *bearer;
	struct tipc_nl_msg msg;
	struct nlattr *attrs;

	/* One mandatory (bearer) */
	if (arg->argc < arg->loc + 1)
		return -EINVAL;

	bearer = arg->argv[arg->loc];

	msg.nl_flags =  NLM_F_REQUEST;
	msg.nl_cmd = TIPC_NL_BEARER_DISABLE;
	err = msg_init(&msg);
	if (err)
		return err;

	attrs = nla_nest_start(msg.nl_msg, TIPC_NLA_BEARER);
	NLA_PUT_STRING(msg.nl_msg, TIPC_NLA_BEARER, bearer);
	nla_nest_end(msg.nl_msg, attrs);

	err = msg_send(&msg);
	if (err)
		return err;

	err = msg_recv(&msg, NULL);
	if (err)
		return err;

	log_info("Bearer %s disabled\n", bearer);

	return 0;

nla_put_failure:
	msg_abort(&msg);

	return -ENOBUFS;
}

static int print_prop(struct nl_msg *nl_msg, void *arg)
{
	int *sel = arg;
	struct nlmsghdr *nlh = nlmsg_hdr(nl_msg);
	struct nlattr *info[TIPC_NLA_MAX + 1];
	struct nlattr *attrs[TIPC_NLA_BEARER_MAX + 1];
	struct nlattr *prop[TIPC_NLA_PROP_MAX + 1];

	genlmsg_parse(nlh, 0, info, TIPC_NLA_MAX, NULL);

	if (!info[TIPC_NLA_BEARER]) {
		log_err("no bearer received from kernel\n");
		return NL_STOP;
	}

	if (nla_parse_nested(attrs, TIPC_NLA_BEARER_MAX,
			     info[TIPC_NLA_BEARER], NULL)) {
		log_err("parsing nested bearer properties\n");
		return NL_STOP;
	}

	if (!attrs[TIPC_NLA_BEARER_PROP]) {
		log_err("no bearer properties received from kernel\n");
		return NL_STOP;
	}

	if (nla_parse_nested(prop, TIPC_NLA_PROP_MAX,
			     attrs[TIPC_NLA_BEARER_PROP], NULL)) {
		log_err("parsing nested bearer properties\n");
		return NL_STOP;
	}

	if (!prop[*sel]) {
		log_err("property not received from kernel\n");
		return NL_STOP;
	}

	if (prop[*sel])
		printf("%u\n", nla_get_u32(prop[*sel]));

	return NL_OK;
}

static int get_prop(struct arg_struct *arg, int prop)
{
	int err;
	struct tipc_nl_msg msg;
	struct nlattr *attrs;
	char *bearer;

	/* One mandatory (bearer) */
	if (arg->argc < arg->loc + 1)
		return -EINVAL;

	bearer = arg->argv[arg->loc];

	msg.nl_flags = NLM_F_REQUEST;
	msg.nl_cmd = TIPC_NL_BEARER_GET;
	msg.callback = &print_prop;
	err = msg_init(&msg);
	if (err)
		return err;

	attrs = nla_nest_start(msg.nl_msg, TIPC_NLA_BEARER);
	NLA_PUT_STRING(msg.nl_msg, TIPC_NLA_BEARER_NAME, bearer);
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
	char *bearer;
	struct tipc_nl_msg msg;
	struct nlattr *attrs;
	struct nlattr *prop;
	__u32 val;

	/* Two mandatory (bearer, attribute) */
	if (arg->argc < arg->loc + 2)
		return -EINVAL;

	bearer = arg->argv[arg->loc];
	val = atoi(arg->argv[arg->loc + 1]);

	msg.nl_flags =  NLM_F_REQUEST;
	msg.nl_cmd = TIPC_NL_BEARER_SET;
	err = msg_init(&msg);
	if (err)
		return err;

	attrs = nla_nest_start(msg.nl_msg, TIPC_NLA_BEARER);
	NLA_PUT_STRING(msg.nl_msg, TIPC_NLA_BEARER_NAME, bearer);

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

	log_info("Bearer %s %s set to %u\n", bearer, name, val);

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

ADD_CMD(&bearer, enable, enable, "<name> [options]", "Enable bearer");
ADD_OPT(enable, enable, priority, "<priority>", "Bearer priority");
ADD_OPT(enable, enable, domain, "<domain>", "Bearer discovery domain");

ADD_CMD(&bearer, list, list, "", "List bearers");

ADD_CMD(&bearer, disable, disable, "<name>", "Disable bearer");

ADD_CMD(&priority, set, set_prio, "<bearer> <priority>", "Set bearer priority");
ADD_CMD(&priority, get, get_prio, "<bearer>", "Get bearer priority");

ADD_CMD(&tolerance, set, set_tol, "<bearer> <tolerance>",
	"Set bearer tolerance");
ADD_CMD(&tolerance, get, get_tol, "<bearer>", "Get bearer tolerance");

ADD_CMD(&window, set, set_win, "<bearer> <window>", "Set bearer window size");
ADD_CMD(&window, get, get_win, "<bearer>", "Get bearer window size");
