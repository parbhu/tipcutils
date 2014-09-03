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
#include <unistd.h>
#include <errno.h>

#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/attr.h>

#include <linux/tipc_config.h>

#include "netlink.h"
#include "cmdl.h"
#include "tipc.h"
#include "misc.h"

ADD_SUBCMD(&cmd_list, cmd_addr, address, "Show or set address");

static int addr_show(struct cmd_struct *cmd, struct arg_struct *arg)
{
	int sk;
	socklen_t sz;
	struct sockaddr_tipc addr;

	sz = sizeof(struct sockaddr_tipc);

	sk = socket(AF_TIPC, SOCK_RDM, 0);
	if (sk < 0) {
		log_err("opening TIPC socket: %s\n", strerror(errno));
		return -1;
	}


	if (getsockname(sk, (struct sockaddr *)&addr, &sz) < 0) {
		log_err("getting TIPC socket address: %s\n", strerror(errno));
		close(sk);
		return -1;
	}

	close(sk);

	printf("<%u.%u.%u>\n",
		tipc_zone(addr.addr.id.node),
		tipc_cluster(addr.addr.id.node),
		tipc_node(addr.addr.id.node));

	return 0;
}

static int addr_set(struct cmd_struct *cmd, struct arg_struct *arg)
{
	int err;
	__u32 addr;
	struct tipc_nl_msg msg;
	struct nlattr *attrs;

	if (arg->argc != arg->loc + 1)
		return -EINVAL;

	addr = str2addr(arg->argv[arg->loc]);
	if (!addr)
		return -1;

	msg.nl_flags =  NLM_F_REQUEST;
	msg.nl_cmd = TIPC_NL_NET_SET;
	err = msg_init(&msg);
	if (err)
		return err;

	attrs = nla_nest_start(msg.nl_msg, TIPC_NLA_NET);
	NLA_PUT_U32(msg.nl_msg, TIPC_NLA_NET_ADDR, addr);
	nla_nest_end(msg.nl_msg, attrs);

	err = msg_send(&msg);
	if (err)
		return err;

	err = msg_recv(&msg, NULL);
	if (err)
		return err;

	log_info("Address set to <%s>\n", arg->argv[arg->loc]);
	return 0;

nla_put_failure:
	msg_abort(&msg);

	return -ENOBUFS;
}

ADD_CMD(&cmd_addr, show, addr_show, "", "Show address");
ADD_CMD(&cmd_addr, set, addr_set, "<address>", "Set address");
