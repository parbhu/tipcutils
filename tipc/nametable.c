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

#define PORTID_STR_LEN 45 /* Four u32 and five delimiter chars */

ADD_SUBCMD(&cmd_list, cmd_nametable, nametable, "Show name table");

static int nametable_print(struct nl_msg *nl_msg, void *arg)
{
	int *iteration = arg;
	char port_id[PORTID_STR_LEN];
	struct nlmsghdr *nlh = nlmsg_hdr(nl_msg);
	struct nlattr *info[TIPC_NLA_MAX + 1];
	struct nlattr *attrs[TIPC_NLA_NAME_TABLE_MAX + 1];
	struct nlattr *publ[TIPC_NLA_PUBL_MAX + 1];

	genlmsg_parse(nlh, 0, info, TIPC_NLA_MAX, NULL);

	if (!info[TIPC_NLA_NAME_TABLE]) {
		log_err("no name table received from kernel\n");
		return NL_STOP;
	}

	if (nla_parse_nested(attrs, TIPC_NLA_NAME_TABLE_MAX,
			     info[TIPC_NLA_NAME_TABLE], NULL)) {
		log_err("parsing nested name table attributes\n");
		return NL_STOP;
	}

	if (!attrs[TIPC_NLA_NAME_TABLE_PUBL]) {
		log_err("no publication info received from kernel\n");
		return NL_STOP;
	}

	if (nla_parse_nested(publ, TIPC_NLA_PUBL_MAX,
			     attrs[TIPC_NLA_NAME_TABLE_PUBL], NULL)) {
		log_err("parsing nested publication info\n");
		return NL_STOP;
	}

	/* Make sure we got everything we need */
	if (!publ[TIPC_NLA_PUBL_TYPE] || !publ[TIPC_NLA_PUBL_LOWER] ||
	    !publ[TIPC_NLA_PUBL_UPPER] || !publ[TIPC_NLA_PUBL_SCOPE] ||
	    !publ[TIPC_NLA_PUBL_NODE] || !publ[TIPC_NLA_PUBL_REF] ||
	    !publ[TIPC_NLA_PUBL_KEY])
		return NL_STOP;

	if (!*iteration)
		printf("%-10s %-10s %-10s %-26s %-10s\n",
		       "Type", "Lower", "Upper", "Port Identity",
		       "Publication Scope");
	(*iteration)++;

	snprintf(port_id, sizeof(port_id), "<%u.%u.%u:%u>",
		 tipc_zone(nla_get_u32(publ[TIPC_NLA_PUBL_NODE])),
		 tipc_cluster(nla_get_u32(publ[TIPC_NLA_PUBL_NODE])),
		 tipc_node(nla_get_u32(publ[TIPC_NLA_PUBL_NODE])),
		 nla_get_u32(publ[TIPC_NLA_PUBL_REF]));

	printf("%-10u %-10u %-10u %-26s %-12u",
	       nla_get_u32(publ[TIPC_NLA_PUBL_TYPE]),
	       nla_get_u32(publ[TIPC_NLA_PUBL_LOWER]),
	       nla_get_u32(publ[TIPC_NLA_PUBL_UPPER]),
	       port_id,
	       nla_get_u32(publ[TIPC_NLA_PUBL_KEY]));

	switch (nla_get_u32(publ[TIPC_NLA_PUBL_SCOPE])) {
	case 1:
		printf("zone\n");
		break;
	case 2:
		printf("cluster\n");
		break;
	case 3:
		printf("node\n");
		break;
	default:
		printf("\n");
		break;
	}

	return NL_OK;
}

static int show(struct cmd_struct *cmd, struct arg_struct *arg)
{
	int err;
	int iteration = 0;
	struct tipc_nl_msg msg;

	msg.nl_flags = NLM_F_REQUEST | NLM_F_DUMP;
	msg.nl_cmd = TIPC_NL_NAME_TABLE_GET;
	msg.callback = &nametable_print;
	err = msg_init(&msg);
	if (err)
		return err;

	err = msg_send(&msg);
	if (err)
		return err;

	err = msg_recv(&msg, &iteration);
	if (err)
		return err;

	return 0;
}

ADD_CMD(&cmd_nametable, show, show, "", "Show name table");
