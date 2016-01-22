/* ------------------------------------------------------------------------
 *
 * mcast_tipc.c
 *
 * Short description: TIPC multicast demo (client side)
 *
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2015, Ericsson Canada
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
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
 *
 * ------------------------------------------------------------------------
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <tipcc.h>
#include <sys/prctl.h>

#define die(fmt, arg...) do			\
	{					\
		printf("Client:" fmt, ## arg);	\
		perror(NULL);			\
		exit(1);			\
	} while(0)

static struct tipc_addr srv = {
	.type = 76453,
	.instance = 17,
	.domain = 0
};

#define MAX_RCVRS 0xff

#define BUF_LEN 66000
static char buf[BUF_LEN];

struct req {
	unsigned int seqno;
	unsigned int remaining_pkts;
};

static char srvbuf[20];
static int verbose;

struct sndr {
	unsigned int rcv_nxt;
	unsigned int cnt;
	unsigned int total_cnt;
} sndrs[MAX_RCVRS + 1];

void mcast_transmitter(int len, unsigned int count)
{
	int sd;
	struct req *pkt = (struct req *)buf;

	printf("Waiting for first server\n");
	tipc_srv_wait(&srv, -1);
	sd = tipc_socket(SOCK_RDM);
	if (sd < 0)
		die("Failed to create client socket\n");

	printf("Transmitting messages of size %u bytes\n", len);
	while(count) {
		pkt->seqno = htonl(ntohl(pkt->seqno) + 1);
		pkt->remaining_pkts = htonl(count);
		if (0 >= tipc_mcast(sd, buf, len, &srv))
			die("Failed to send multicast\n");
		count--;
	}
}

void mcast_receiver(void)
{
	int sd;
	struct tipc_addr src;
	struct sndr *sndr;
	char srcbuf[10];
	unsigned int _seqno, rcv_nxt, gap;
	struct req *pkt = (struct req *)buf;

	prctl(PR_SET_PDEATHSIG, SIGHUP);

	sd = tipc_socket(SOCK_RDM);
	if (sd < 0)
		die("Failed to create server socket\n");

	if (tipc_bind(sd, srv.type, srv.instance, srv.instance, srv.domain))
		die("Failed to bind server socket");

	printf("Server socket created and bound to %s\n", srvbuf);

	while (1) {
		if (0 >= tipc_recvfrom(sd, buf, BUF_LEN, &src, NULL, 0))
			die("unexpected message on multicast server socket\n");

		tipc_dtoa(src.domain, srcbuf, sizeof(srcbuf));
		sndr = &sndrs[src.domain & MAX_RCVRS];
		_seqno = ntohl(pkt->seqno);
		rcv_nxt = sndr->rcv_nxt;
		gap = _seqno - rcv_nxt;
		if (gap && sndr->cnt) {
			printf("Missing %u msgs from %s\n", gap, srcbuf);
		}
		sndr->rcv_nxt = _seqno + 1;
		if (!sndr->cnt) {
			sndr->total_cnt = ntohl(pkt->remaining_pkts);
		}
		if (!(++sndr->cnt % 10000)) {
			if (verbose)
				printf("Now at %u msgs from %s\n", sndr->cnt, srcbuf);
		}

		if (ntohl(pkt->remaining_pkts) == 1) {
			if (sndr->cnt == sndr->total_cnt) {
				printf("Test OK for %u pkts from %s\n",
				       sndr->cnt, srcbuf);
			} else {
				printf("Test NOK for %u pkts from %s, missing %u\n",
				       sndr->cnt, srcbuf, sndr->total_cnt - sndr->cnt);
			}
			memset(sndr, 0, sizeof(struct sndr));
		}
	}
}

void mcast_receiver_subscriber(void)
{
	int sd;
	bool up;
	struct tipc_addr rcvr;
	char node[10];

	prctl(PR_SET_PDEATHSIG, SIGHUP);

	sd = tipc_topsrv_conn(0);
	if (sd < 0)
		die("Failed to create subscriber socket\n");

	if (tipc_srv_subscr(sd, srv.type, srv.instance, srv.instance, true, -1))
		die("subscription for server failed\n");

	while (1) {
		if (tipc_srv_evt(sd, &rcvr, &up, 0))
			die("reception of service event failed\n");

		tipc_dtoa(rcvr.domain, node, sizeof(node));
		if (verbose) {
			if (up)
				printf("New receiver discovered on node %s\n", node);
			else
				printf("Receiver on node %s lost\n", node);
		}
	}
}

int main(int argc, char *argv[], char *envp[])
{
	int c;
	int len = 66000;
	unsigned int count = ~0;
	bool xmit_only = false, rcvr_only = false;

	while ((c = getopt(argc, argv, ":csvl:n:")) != -1) {
		switch (c) {
		case 'l':
			if (optarg)
				len = atoi(optarg);
			continue;
		case 'c':
			xmit_only = true;
			continue;
		case 'v':
			verbose++;
			continue;
		case 's':
			rcvr_only = true;
			continue;
		case 'n':
			if (optarg)
				count = atoi(optarg);
			continue;
		default:
			fprintf(stderr, "Usage:\n");
			fprintf(stderr," %s ", argv[0]);
			fprintf(stderr, "[-c] [-s] [-l <msglen>]\n"
                         "       [-c client (xmitit) mode only (default off)\n"
                         "       [-s server (receive) mode only (default off)\n"
		         "       [-l <msglen>] message size (default 66000)\n"
                         "       [-v increase verbosity (default off)\n"
		         "       [-n <num_of_msgs>] number of messages to send (default 0xfffffff)\n");
			return 1;
		}
	}

	memset(buf, 0xff, sizeof(buf));
	srv.domain = tipc_own_cluster();
	tipc_ntoa(&srv, srvbuf, sizeof(srvbuf));

	if (!xmit_only && !fork())
		mcast_receiver();

	if (rcvr_only)
		while(1);

	if (!fork())
		mcast_receiver_subscriber();

	mcast_transmitter(len, count);
	exit(0);
}
