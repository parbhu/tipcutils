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

#include <linux/tipc_netlink.h>

#include <errno.h>

#include "netlink.h"
#include "cmdl.h"
#include "tipc.h"

ADD_SUBCMD(&cmd_list, cmd_link, link, "Show or manage links");
ADD_SUBCMD(&cmd_link, cmd_link_prio, priority, "Get or set link priority");
ADD_SUBCMD(&cmd_link, cmd_link_tol, tolerance, "Get or set link tolerance");
ADD_SUBCMD(&cmd_link, cmd_link_win, window, "Get or set link window size");
ADD_SUBCMD(&cmd_link, cmd_link_stat, statistics,
	   "Show or reset link statistics");

static __u32 perc(__u32 count, __u32 total)
{
	return (count * 100 + (total / 2)) / total;
}

static int _list(struct nl_msg *nl_msg, void *arg)
{
	char *name;
	struct nlmsghdr *nlh = nlmsg_hdr(nl_msg);
	struct nlattr *info[TIPC_NLA_MAX + 1];
	struct nlattr *attrs[TIPC_NLA_LINK_MAX + 1];

	genlmsg_parse(nlh, 0, info, TIPC_NLA_MAX, NULL);

	if (!info[TIPC_NLA_LINK]) {
		log_err("no link received from kernel\n");
		return NL_STOP;
	}

	if (nla_parse_nested(attrs, TIPC_NLA_LINK_MAX,
			     info[TIPC_NLA_LINK], NULL)) {
		log_err("parsing nested link attributes\n");
		return NL_STOP;
	}

	if (!attrs[TIPC_NLA_LINK_NAME]) {
		log_err("no link name received from kernel\n");
		return NL_STOP;
	}

	name = nla_get_string(attrs[TIPC_NLA_LINK_NAME]);

	printf("%s: ", name);

	if (attrs[TIPC_NLA_LINK_UP])
		printf("up\n");
	else
		printf("down\n");

	return NL_OK;
}

static int list(struct cmd_struct *cmd, struct arg_struct *arg)
{
	int err;
	struct tipc_nl_msg msg;

	msg.nl_flags = NLM_F_REQUEST | NLM_F_DUMP;
	msg.nl_cmd = TIPC_NL_LINK_GET;
	msg.callback = &_list;
	err = msg_init(&msg);
	if (err)
		return err;

	err = msg_send(&msg);
	if (err)
		return err;

	err = msg_recv(&msg, arg);
	if (err)
		return err;

	return 0;
}

static int _show_stat(struct nlattr *attrs[], struct nlattr *prop[],
		      struct nlattr *stats[])
{
	__u32 prof_tot;

	if (attrs[TIPC_NLA_LINK_ACTIVE])
		printf("  ACTIVE");
	else if (attrs[TIPC_NLA_LINK_UP])
		printf("  STANDBY");
	else
		printf("  DEFUNCT");

	printf("  MTU:%u  Priority:%u  Tolerance:%u ms  Window:%u packets\n",
	       nla_get_u32(attrs[TIPC_NLA_LINK_MTU]),
	       nla_get_u32(prop[TIPC_NLA_PROP_PRIO]),
	       nla_get_u32(prop[TIPC_NLA_PROP_TOL]),
	       nla_get_u32(prop[TIPC_NLA_PROP_WIN]));

	printf("  RX packets:%u fragments:%u/%u bundles:%u/%u\n",
	       nla_get_u32(attrs[TIPC_NLA_LINK_RX]) -
	       nla_get_u32(stats[TIPC_NLA_STATS_RX_INFO]),
	       nla_get_u32(stats[TIPC_NLA_STATS_RX_FRAGMENTS]),
	       nla_get_u32(stats[TIPC_NLA_STATS_RX_FRAGMENTED]),
	       nla_get_u32(stats[TIPC_NLA_STATS_RX_BUNDLES]),
	       nla_get_u32(stats[TIPC_NLA_STATS_RX_BUNDLED]));

	printf("  TX packets:%u fragments:%u/%u bundles:%u/%u\n",
	       nla_get_u32(attrs[TIPC_NLA_LINK_TX]) -
	       nla_get_u32(stats[TIPC_NLA_STATS_TX_INFO]),
	       nla_get_u32(stats[TIPC_NLA_STATS_TX_FRAGMENTS]),
	       nla_get_u32(stats[TIPC_NLA_STATS_TX_FRAGMENTED]),
	       nla_get_u32(stats[TIPC_NLA_STATS_TX_BUNDLES]),
	       nla_get_u32(stats[TIPC_NLA_STATS_TX_BUNDLED]));

	prof_tot = nla_get_u32(stats[TIPC_NLA_STATS_MSG_PROF_TOT]);
	printf("  TX profile sample:%u packets  average:%u octets\n",
	       nla_get_u32(stats[TIPC_NLA_STATS_MSG_LEN_CNT]),
	       nla_get_u32(stats[TIPC_NLA_STATS_MSG_LEN_TOT]) / prof_tot);

	printf("  0-64:%u%% -256:%u%% -1024:%u%% -4096:%u%% "
	       "-16384:%u%% -32768:%u%% -66000:%u%%\n",
	       perc(nla_get_u32(stats[TIPC_NLA_STATS_MSG_LEN_P0]), prof_tot),
	       perc(nla_get_u32(stats[TIPC_NLA_STATS_MSG_LEN_P1]), prof_tot),
	       perc(nla_get_u32(stats[TIPC_NLA_STATS_MSG_LEN_P2]), prof_tot),
	       perc(nla_get_u32(stats[TIPC_NLA_STATS_MSG_LEN_P3]), prof_tot),
	       perc(nla_get_u32(stats[TIPC_NLA_STATS_MSG_LEN_P4]), prof_tot),
	       perc(nla_get_u32(stats[TIPC_NLA_STATS_MSG_LEN_P5]), prof_tot),
	       perc(nla_get_u32(stats[TIPC_NLA_STATS_MSG_LEN_P6]), prof_tot));

	printf("  RX states:%u probes:%u naks:%u defs:%u dups:%u\n",
	       nla_get_u32(stats[TIPC_NLA_STATS_RX_STATES]),
	       nla_get_u32(stats[TIPC_NLA_STATS_RX_PROBES]),
	       nla_get_u32(stats[TIPC_NLA_STATS_RX_NACKS]),
	       nla_get_u32(stats[TIPC_NLA_STATS_RX_DEFERRED]),
	       nla_get_u32(stats[TIPC_NLA_STATS_DUPLICATES]));

	printf("  TX states:%u probes:%u naks:%u acks:%u dups:%u\n",
	       nla_get_u32(stats[TIPC_NLA_STATS_TX_STATES]),
	       nla_get_u32(stats[TIPC_NLA_STATS_TX_PROBES]),
	       nla_get_u32(stats[TIPC_NLA_STATS_TX_NACKS]),
	       nla_get_u32(stats[TIPC_NLA_STATS_TX_ACKS]),
	       nla_get_u32(stats[TIPC_NLA_STATS_RETRANSMITTED]));

	printf("  Congestion link:%u  Send queue max:%u avg:%u\n",
	       nla_get_u32(stats[TIPC_NLA_STATS_LINK_CONGS]),
	       nla_get_u32(stats[TIPC_NLA_STATS_MAX_QUEUE]),
	       nla_get_u32(stats[TIPC_NLA_STATS_AVG_QUEUE]));

	return NL_OK;
}

static int _show_bc_stat(struct nlattr *prop[], struct nlattr *stats[])
{
	printf("  Window:%u packets\n",
	       nla_get_u32(prop[TIPC_NLA_PROP_WIN]));

	printf("  RX packets:%u fragments:%u/%u bundles:%u/%u\n",
	       nla_get_u32(stats[TIPC_NLA_STATS_RX_INFO]),
	       nla_get_u32(stats[TIPC_NLA_STATS_RX_FRAGMENTS]),
	       nla_get_u32(stats[TIPC_NLA_STATS_RX_FRAGMENTED]),
	       nla_get_u32(stats[TIPC_NLA_STATS_RX_BUNDLES]),
	       nla_get_u32(stats[TIPC_NLA_STATS_RX_BUNDLED]));

	printf("  TX packets:%u fragments:%u/%u bundles:%u/%u\n",
	       nla_get_u32(stats[TIPC_NLA_STATS_TX_INFO]),
	       nla_get_u32(stats[TIPC_NLA_STATS_TX_FRAGMENTS]),
	       nla_get_u32(stats[TIPC_NLA_STATS_TX_FRAGMENTED]),
	       nla_get_u32(stats[TIPC_NLA_STATS_TX_BUNDLES]),
	       nla_get_u32(stats[TIPC_NLA_STATS_TX_BUNDLED]));

	printf("  RX naks:%u defs:%u dups:%u\n",
	       nla_get_u32(stats[TIPC_NLA_STATS_RX_NACKS]),
	       nla_get_u32(stats[TIPC_NLA_STATS_RX_DEFERRED]),
	       nla_get_u32(stats[TIPC_NLA_STATS_DUPLICATES]));

	printf("  TX naks:%u acks:%u dups:%u\n",
	       nla_get_u32(stats[TIPC_NLA_STATS_TX_NACKS]),
	       nla_get_u32(stats[TIPC_NLA_STATS_TX_ACKS]),
	       nla_get_u32(stats[TIPC_NLA_STATS_RETRANSMITTED]));

	printf("  Congestion link:%u  Send queue max:%u avg:%u\n",
	       nla_get_u32(stats[TIPC_NLA_STATS_LINK_CONGS]),
	       nla_get_u32(stats[TIPC_NLA_STATS_MAX_QUEUE]),
	       nla_get_u32(stats[TIPC_NLA_STATS_AVG_QUEUE]));

	return NL_OK;
}

static int _dump_stat(struct nl_msg *nl_msg, void *a)
{
	char *name;
	struct arg_struct *arg = a;
	struct nlmsghdr *nlh = nlmsg_hdr(nl_msg);

	struct nlattr *info[TIPC_NLA_MAX + 1];
	struct nlattr *attrs[TIPC_NLA_LINK_MAX + 1];
	struct nlattr *prop[TIPC_NLA_PROP_MAX + 1];
	struct nlattr *stats[TIPC_NLA_STATS_MAX + 1];

	genlmsg_parse(nlh, 0, info, TIPC_NLA_MAX, NULL);

	if (!info[TIPC_NLA_LINK]) {
		log_err("no link received from kernel\n");
		return NL_STOP;
	}

	if (nla_parse_nested(attrs, TIPC_NLA_LINK_MAX,
			     info[TIPC_NLA_LINK], NULL)) {
		log_err("parsing nested link attributes\n");
		return NL_STOP;
	}

	if (!attrs[TIPC_NLA_LINK_NAME]) {
		log_err("no link name received from kernel\n");
		return NL_STOP;
	}

	name = nla_get_string(attrs[TIPC_NLA_LINK_NAME]);

	/* Filter */
	if (has_extra_arg(arg))
		if (!arg_str_search(arg, name))
			return NL_SKIP;

	if (!attrs[TIPC_NLA_LINK_PROP]) {
		log_err("no link properties received from kernel\n");
		return NL_STOP;
	}

	if (nla_parse_nested(prop, TIPC_NLA_PROP_MAX,
			     attrs[TIPC_NLA_LINK_PROP], NULL)) {
		log_err("parsing nested link properties\n");
		return NL_STOP;
	}

	if (!attrs[TIPC_NLA_LINK_STATS]) {
		log_err("no link stats received from kernel\n");
		return NL_STOP;
	}

	if (nla_parse_nested(stats, TIPC_NLA_STATS_MAX,
			     attrs[TIPC_NLA_LINK_STATS], NULL)) {
		log_err("parsing nested link stats\n");
		return NL_STOP;
	}

	if (attrs[TIPC_NLA_LINK_BROADCAST]) {
		printf("Link <%s>\n", name);
		return _show_bc_stat(prop, stats);
	}

	printf("\nLink <%s>\n", name);
	return _show_stat(attrs, prop, stats);
}

static int show_stat(struct cmd_struct *cmd, struct arg_struct *arg)
{
	int err;
	struct tipc_nl_msg msg;

	msg.nl_flags = NLM_F_REQUEST | NLM_F_DUMP;
	msg.nl_cmd = TIPC_NL_LINK_GET;
	msg.callback = &_dump_stat;
	err = msg_init(&msg);
	if (err)
		return err;

	err = msg_send(&msg);
	if (err)
		return err;

	err = msg_recv(&msg, arg);
	if (err)
		return err;

	return 0;
}

static int reset_stat(struct cmd_struct *cmd, struct arg_struct *arg)
{
	int err;
	char *link;
	struct tipc_nl_msg msg;
	struct nlattr *attrs;

	/* One mandatory argument (link) */
	if (arg->argc < arg->loc + 1)
		return -EINVAL;

	link = arg->argv[arg->loc];
	(arg->loc)++;

	msg.nl_flags =  NLM_F_REQUEST;
	msg.nl_cmd = TIPC_NL_LINK_RESET_STATS;
	err = msg_init(&msg);
	if (err)
		return err;

	attrs = nla_nest_start(msg.nl_msg, TIPC_NLA_LINK);
	NLA_PUT_STRING(msg.nl_msg, TIPC_NLA_LINK_NAME, link);
	nla_nest_end(msg.nl_msg, attrs);

	err = msg_send(&msg);
	if (err)
		return err;

	err = msg_recv(&msg, NULL);
	if (err)
		return err;

	log_info("Link %s stats reset\n", link);

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
	struct nlattr *attrs[TIPC_NLA_LINK_MAX + 1];
	struct nlattr *prop[TIPC_NLA_PROP_MAX + 1];

	genlmsg_parse(nlh, 0, info, TIPC_NLA_MAX, NULL);

	if (!info[TIPC_NLA_LINK]) {
		log_err("no link received from kernel\n");
		return NL_STOP;
	}

	if (nla_parse_nested(attrs, TIPC_NLA_LINK_MAX,
			     info[TIPC_NLA_LINK], NULL)) {
		log_err("parsing nested link properties\n");
		return NL_STOP;
	}

	if (!attrs[TIPC_NLA_LINK_PROP]) {
		log_err("no link properties received from kernel\n");
		return NL_STOP;
	}

	if (nla_parse_nested(prop, TIPC_NLA_PROP_MAX,
			     attrs[TIPC_NLA_LINK_PROP], NULL)) {
		log_err("parsing nested link properties\n");
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
	char *link;
	struct tipc_nl_msg msg;

	/* One mandatory (link) */
	if (arg->argc < arg->loc + 1)
		return -EINVAL;

	link = arg->argv[arg->loc];

	msg.nl_flags = NLM_F_REQUEST;
	msg.nl_cmd = TIPC_NL_LINK_GET;
	msg.callback = &print_prop;
	err = msg_init(&msg);
	if (err)
		return err;

	NLA_PUT_STRING(msg.nl_msg, TIPC_NLA_LINK_NAME, link);

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
	char *link;
	struct tipc_nl_msg msg;
	struct nlattr *attrs;
	struct nlattr *prop;
	__u32 val;

	/* Two mandatory (link, attribute) */
	if (arg->argc < arg->loc + 2)
		return -EINVAL;

	link = arg->argv[arg->loc];
	val = atoi(arg->argv[arg->loc + 1]);

	msg.nl_flags =  NLM_F_REQUEST;
	msg.nl_cmd = TIPC_NL_LINK_SET;
	err = msg_init(&msg);
	if (err)
		return err;

	attrs = nla_nest_start(msg.nl_msg, TIPC_NLA_LINK);
	NLA_PUT_STRING(msg.nl_msg, TIPC_NLA_LINK_NAME, link);

	prop = nla_nest_start(msg.nl_msg, TIPC_NLA_LINK_PROP);
	NLA_PUT_U32(msg.nl_msg, nla, val);
	nla_nest_end(msg.nl_msg, prop);

	nla_nest_end(msg.nl_msg, attrs);

	err = msg_send(&msg);
	if (err)
		return err;

	err = msg_recv(&msg, NULL);
	if (err)
		return err;

	log_info("Link %s %s set to %u\n", link, name, val);

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

ADD_CMD(&cmd_link, list, list, "[LINK]...", "List all links");

ADD_CMD(&cmd_link_stat, reset, reset_stat, "LINK", "Reset link statistics");
ADD_CMD(&cmd_link_stat, show, show_stat, "[LINK]...", "Show link statistics");

ADD_CMD(&cmd_link_prio, set, set_prio, "<link> <priority>",
	"Set link priority");
ADD_CMD(&cmd_link_prio, get, get_prio, "<link>", "Get link priority");

ADD_CMD(&cmd_link_tol, set, set_tol, "<link> <tolerance>",
	"Set link tolerance");
ADD_CMD(&cmd_link_tol, get, get_tol, "<link>", "Get link tolerance");

ADD_CMD(&cmd_link_win, set, set_win, "<link> <window>",
	"Set link window size");
ADD_CMD(&cmd_link_win, get, get_win, "<link>", "Get link window size");
