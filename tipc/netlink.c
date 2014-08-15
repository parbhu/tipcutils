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

#include <errno.h>
#include <time.h>

#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>

#include <linux/tipc_config.h>

#include "netlink.h"
#include "tipc.h"

static int cb_invalid(struct nl_msg *msg, void *arg)
{
	log_err("got invalid netlink messages\n");
	return NL_STOP;
}

static int cb_interrupted(struct nl_msg *msg, void *arg)
{
	log_err("dumping was interrupted\n");
	return NL_STOP;
}

static int cb_acknowledge(struct nl_msg *msg, void *arg)
{
	int *ack = arg;
	*ack = 1;

	return NL_STOP;
}

static int cb_seq_check(struct nl_msg *nl_msg, void *arg)
{
	struct tipc_nl_msg *msg = arg;
	struct nlmsghdr *hdr = nlmsg_hdr(nl_msg);

	if (hdr->nlmsg_seq != msg->nl_seq) {
		log_err("Sequence number mismatch\n");
		return NL_STOP;
	}
	return NL_OK;
}

int msg_init(struct tipc_nl_msg *msg)
{
	int err;
	void *hdr;

	msg->nl_sock = nl_socket_alloc();
	if (!msg->nl_sock) {
		log_err("allocating netlink socket\n");
		return -1;
	}

	err = nl_connect(msg->nl_sock, NETLINK_GENERIC);
	if (err) {
		log_err("creating nl file descriptor and binding socket\n");
		goto sock_err;
	}

	msg->nl_family = genl_ctrl_resolve(msg->nl_sock, TIPC_GENL_NAME);
	if (!msg->nl_family) {
		log_err("retrieving TIPC netlink family id\n");
		goto sock_err;
	}

	msg->nl_msg = nlmsg_alloc();
	if (!msg->nl_msg) {
		log_err("allocating netlink message\n");
		goto sock_err;
	}

	/* We never step the seq nr as we only send one message per execution */
	msg->nl_seq = time(0);

	hdr = genlmsg_put(msg->nl_msg, NL_AUTO_PID, msg->nl_seq, msg->nl_family,
			  0, msg->nl_flags, msg->nl_cmd, 0);
	if (!hdr) {
		log_err("adding generic netlink headers to message\n");
		goto msg_err;
	}

	return 0;

msg_err:
	nlmsg_free(msg->nl_msg);
sock_err:
	nl_socket_free(msg->nl_sock);
	return -1;
}

void msg_abort(struct tipc_nl_msg *msg)
{
	nlmsg_free(msg->nl_msg);
	nl_socket_free(msg->nl_sock);
}

int msg_send(struct tipc_nl_msg *msg)
{
	int n;

	n = nl_send_auto(msg->nl_sock, msg->nl_msg);
	if (n < 0) {
		nl_close(msg->nl_sock);
		nl_socket_free(msg->nl_sock);
		return -1;
	}

	nlmsg_free(msg->nl_msg);

	return 0;
}

int msg_recv(struct tipc_nl_msg *msg, void *data)
{
	int err;
	int ret = 0;
	int acked = 0;
	struct nl_cb *cb;
	struct nl_cb *orig_cb;

	orig_cb = nl_socket_get_cb(msg->nl_sock);
	cb = nl_cb_clone(orig_cb);
	nl_cb_put(orig_cb);
	if (!cb) {
		log_err("cloning original netlink callbacks\n");
		ret = -1;
		goto early_out;
	}

	if (msg->callback)
		nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, msg->callback, data);

	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, cb_seq_check, msg);
	nl_cb_set(cb, NL_CB_INVALID, NL_CB_CUSTOM, cb_invalid, NULL);
	nl_cb_set(cb, NL_CB_DUMP_INTR, NL_CB_CUSTOM, cb_interrupted, NULL);
	nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, cb_acknowledge, &acked);
	nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, cb_acknowledge, &acked);

	err = nl_recvmsgs(msg->nl_sock, cb);
	if (err) {
		log_err("kernel: %s\n", nl_geterror(err));
		ret = -1;
		goto out;
	}

	/*
	 * There is multiple reasons why this would happen, something might have
	 * gone wrong on the kernel side or our callback might have issued
	 * NL_STOP.
	 */
	if (!acked) {
		log_err("message is incomplete\n");
		ret = -1;
		goto out;
	}

out:
	nl_cb_put(cb);
early_out:
	nl_close(msg->nl_sock);
	nl_socket_free(msg->nl_sock);

	return ret;
}
