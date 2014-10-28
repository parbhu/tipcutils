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

#include <netlink/netlink.h>
#include <netlink/genl/genl.h>

#include <linux/tipc_netlink.h>

#include "netlink.h"
#include "cmdl.h"
#include "tipc.h"

ADD_SUBCMD(&cmd_list, cmd_sock, socket, "Show sockets");

static int publ_print(struct nl_msg *nl_msg, void *arg)
{
	struct nlmsghdr *nlh = nlmsg_hdr(nl_msg);
	struct nlattr *info[TIPC_NLA_MAX + 1];
	struct nlattr *attrs[TIPC_NLA_PUBL_MAX + 1];

	genlmsg_parse(nlh, 0, info, TIPC_NLA_MAX, NULL);

	if (!info[TIPC_NLA_PUBL]) {
		log_err("no publ received from kernel\n");
		return NL_STOP;
	}

	if (nla_parse_nested(attrs, TIPC_NLA_PUBL_MAX,
			     info[TIPC_NLA_PUBL], NULL)) {
		log_err("parsing nested publ attributes\n");
		return NL_STOP;
	}

	printf("  bound to {%u,%u,%u}\n",
	       nla_get_u32(attrs[TIPC_NLA_PUBL_TYPE]),
	       nla_get_u32(attrs[TIPC_NLA_PUBL_LOWER]),
	       nla_get_u32(attrs[TIPC_NLA_PUBL_UPPER]));

	return NL_OK;
}

static int publ_list(__u32 sock)
{
	int err;
	struct tipc_nl_msg msg;
	struct nlattr *attrs;

	msg.nl_flags = NLM_F_REQUEST | NLM_F_DUMP;
	msg.nl_cmd = TIPC_NL_PUBL_GET;
	msg.callback = &publ_print;

	err = msg_init(&msg);
	if (err)
		return err;

	attrs = nla_nest_start(msg.nl_msg, TIPC_NLA_SOCK);
	NLA_PUT_U32(msg.nl_msg, TIPC_NLA_SOCK_REF, sock);
	nla_nest_end(msg.nl_msg, attrs);

	err = msg_send(&msg);
	if (err)
		return err;

	err = msg_recv(&msg, NULL);
	if (err)
		return err;

	return 0;

nla_put_failure:
	msg_abort(&msg);

	return -2;
}

static int sock_print(struct nl_msg *nl_msg, void *arg)
{
	struct nlmsghdr *nlh = nlmsg_hdr(nl_msg);
	struct nlattr *info[TIPC_NLA_MAX + 1];
	struct nlattr *attrs[TIPC_NLA_SOCK_MAX + 1];

	genlmsg_parse(nlh, 0, info, TIPC_NLA_MAX, NULL);

	if (!info[TIPC_NLA_SOCK]) {
		log_err("no socket received from kernel\n");
		return NL_STOP;
	}
	if (nla_parse_nested(attrs, TIPC_NLA_SOCK_MAX, info[TIPC_NLA_SOCK],
			     NULL)) {
		log_err("parsing nested socket attributes\n");
		return NL_STOP;
	}
	printf("socket %u\n", nla_get_u32(attrs[TIPC_NLA_SOCK_REF]));

	if (attrs[TIPC_NLA_SOCK_CON]) {
		__u32 node;
		struct nlattr *con[TIPC_NLA_CON_MAX + 1];

		if (nla_parse_nested(con, TIPC_NLA_CON_MAX,
				     attrs[TIPC_NLA_SOCK_CON], NULL)) {
			log_err("parsing nested connection info\n");
			return NL_STOP;
		}
		node = nla_get_u32(con[TIPC_NLA_CON_NODE]);

		printf("  connected to <%u.%u.%u:%u>", tipc_zone(node),
			tipc_cluster(node), tipc_node(node),
			nla_get_u32(con[TIPC_NLA_CON_SOCK]));

		if (con[TIPC_NLA_CON_FLAG])
			printf(" via {%u,%u}\n",
				nla_get_u32(con[TIPC_NLA_CON_TYPE]),
				nla_get_u32(con[TIPC_NLA_CON_INST]));
		else
			printf("\n");
	} else if (attrs[TIPC_NLA_SOCK_HAS_PUBL]) {
		publ_list(nla_get_u32(attrs[TIPC_NLA_SOCK_REF]));
	}

	return NL_OK;
}

static int list(struct cmd_struct *cmd, struct arg_struct *arg)
{
	int err;
	__u32 last_ref = 0;
	struct tipc_nl_msg msg;

	msg.nl_flags = NLM_F_REQUEST | NLM_F_DUMP;
	msg.nl_cmd = TIPC_NL_SOCK_GET;
	msg.callback = &sock_print;
	err = msg_init(&msg);
	if (err)
		return err;
	err = msg_send(&msg);
	if (err)
		return err;
	err = msg_recv(&msg, &last_ref);
	if (err)
		return err;

	return 0;
}

ADD_CMD(&cmd_sock, list, list, "", "List all sockets");
