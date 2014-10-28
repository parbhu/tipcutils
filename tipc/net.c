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
#include "misc.h"

ADD_SUBCMD(&cmd_list, cmd_net, net, "Show or set net info");
ADD_SUBCMD(&cmd_net, cmd_net_id, id, "Show or set net id");

int __id_print(struct nl_msg *nl_msg, void *arg)
{
	struct nlmsghdr *nlh = nlmsg_hdr(nl_msg);
	struct nlattr *info[TIPC_NLA_MAX + 1];
	struct nlattr *attrs[TIPC_NLA_NET_MAX + 1];

	genlmsg_parse(nlh, 0, info, TIPC_NLA_MAX, NULL);

	if (!info[TIPC_NLA_NET]) {
		log_err("no net received from kernel\n");
		return NL_STOP;
	}

	if (nla_parse_nested(attrs, TIPC_NLA_NET_MAX,
			     info[TIPC_NLA_NET], NULL)) {
		log_err("parsing nested net attributes\n");
		return NL_STOP;
	}

	if (!attrs[TIPC_NLA_NET_ID]) {
		log_err("no net id received from kernel\n");
		return NL_STOP;
	}
	printf("%u\n", nla_get_u32(attrs[TIPC_NLA_NET_ID]));

	return NL_OK;
}

static int id_show(struct cmd_struct *cmd, struct arg_struct *arg)
{
	int err;
	struct tipc_nl_msg msg;

	msg.nl_flags = NLM_F_REQUEST | NLM_F_DUMP;
	msg.nl_cmd = TIPC_NL_NET_GET;
	msg.callback = &__id_print;
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

static int id_set(struct cmd_struct *cmd, struct arg_struct *arg)
{
	int err;
	struct tipc_nl_msg msg;
	struct nlattr *attrs;
	__u32 id;

	if (arg->argc != arg->loc + 1)
		return -EINVAL;

	id = atoi(arg->argv[arg->loc]);

	msg.nl_flags =  NLM_F_REQUEST;
	msg.nl_cmd = TIPC_NL_NET_SET;
	err = msg_init(&msg);
	if (err)
		return err;

	attrs = nla_nest_start(msg.nl_msg, TIPC_NLA_NET);
	NLA_PUT_U32(msg.nl_msg, TIPC_NLA_NET_ID, id);
	nla_nest_end(msg.nl_msg, attrs);

	err = msg_send(&msg);
	if (err)
		return err;

	err = msg_recv(&msg, NULL);
	if (err)
		return err;

	log_info("Net id set to %u\n", id);
	return 0;

nla_put_failure:
	nlmsg_free(msg.nl_msg);
	return -ENOBUFS;
}

ADD_CMD(&cmd_net_id, show, id_show, "", "Show net id");
ADD_CMD(&cmd_net_id, set, id_set, "<id>", "Set net id");
