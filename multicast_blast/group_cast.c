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
#include <sys/poll.h>
#include <sys/time.h>
#include <sys/prctl.h>

#define INTV_SZ 2000
#define MAX_PEERS 4096

#define die(fmt, arg...) do			\
	{					\
		printf("groupcast:" fmt, ## arg);\
		perror(NULL);			\
		exit(1);			\
	} while(0)

static struct tipc_addr self = {
	.type = 4711,
	.instance = 0,
	.domain = 0
};

static struct tipc_addr dst = {
	.type = 4711,
	.instance = 0,
	.domain = 0
};

#define BUF_LEN 66000
static char buf[BUF_LEN];
static char self_str[30];

unsigned int member_cnt = 0;
unsigned int snt_bc = 0;
unsigned int snt_mc = 0;
unsigned int snt_ac = 0;
unsigned int snt_uc = 0;
unsigned int rcv_bc = 0;
unsigned int rcv_mc = 0;
unsigned int rcv_ac = 0;
unsigned int rcv_uc = 0;

enum {
	NONE =  0,
	BCAST = 1,
	MCAST = 2,
	UCAST = 3,
	ACAST = 4
};

static struct hdr {
	int type : 8;
	bool reply;
} *hdr = (void*)buf;

static unsigned long long elapsedmsec(struct timeval *from, struct timeval *to)
{
	long long from_us, to_us;

	from_us = from->tv_sec * 1000000 + from->tv_usec;
	to_us = to->tv_sec * 1000000 + to->tv_usec;
	return (to_us - from_us) / 1000;
}

void discover_member(int sd)
{
	struct tipc_addr member, memberid;
	char member_str[30];
	char memberid_str[30];
	bool up;

	if (tipc_srv_evt(sd, &member, &memberid, &up, 0))
		die("reception of service event failed\n");

	tipc_ntoa(&member, member_str, sizeof(member_str));
	tipc_ntoa(&memberid, memberid_str, sizeof(memberid_str));
	if (up) {
		printf("New member %s discovered at %s\n",
		       member_str, memberid_str);
		member_cnt++;
	} else {
		printf("Member %s at %s lost\n", member_str, memberid_str);
		member_cnt--;
	}
}

void pr_stats(int rcv_cnt)
{
	if (rcv_cnt % INTV_SZ)
		return;
	printf("Recv %u broadcasts, %u multicasts, %u anycasts, %u unicasts; ",
	       rcv_bc, rcv_mc, rcv_ac, rcv_uc);
	printf("Sent %u unicasts\n", snt_uc);

}

int timeout(int mtyp)
{
	if (!mtyp)
		return -1;
	if (member_cnt <= 1)
		return -1;
	return 0;
}

void group_transceive(struct pollfd *pfd, int mtyp, bool ucast_reply, int len)
{
	struct tipc_addr src;

	while (poll(pfd, 2, timeout(mtyp)) > 0) {

		if (pfd[0].revents & POLLIN)
			discover_member(pfd[0].fd);

		if (pfd[1].revents & POLLIN) {
			if (0 >= tipc_recvfrom(pfd[1].fd, buf, BUF_LEN, &src, NULL, 0))
				die("unexpected message on transceiver socket\n");
			if (hdr->type == BCAST)
				pr_stats(rcv_bc++);
			if (hdr->type == MCAST)
				pr_stats(rcv_mc++);
			else if(hdr->type == ACAST)
				pr_stats(rcv_ac++);
			else if(hdr->type == UCAST)
				pr_stats(rcv_uc++);
			if (hdr->reply) {
				hdr->type = UCAST;
				hdr->reply = false;
				if (0 >= tipc_sendto(pfd[1].fd, buf, len, &src))
					die("Failed to send group unicast\n");
				snt_uc++;
			}
		}
	}

	hdr->type = mtyp;
	hdr->reply = ucast_reply;

	if (mtyp == BCAST) {
		if (0 >= tipc_send(pfd[1].fd, buf, len))
			die("Failed to send group broadcast\n");
		snt_bc++;
		return;
	} else 	if (mtyp == ACAST) {
		if (0 >= tipc_sendto(pfd[1].fd, buf, len, &dst))
			die("Failed to send group anycast\n");
		snt_ac++;
	} else 	if (mtyp == MCAST) {
		if (0 >= tipc_mcast(pfd[1].fd, buf, len, &dst))
			die("Failed to send group multicast\n");
		snt_mc++;
	}
}

int syntax(char *argv[])
{
	fprintf(stderr, "Usage:\n");
	fprintf(stderr," %s ", argv[0]);
	fprintf(stderr, "[-b] [-m] [-a] [-l <msglen>]\n"
		"       [-b send group broadcast (default off)\n"
		"       [-m send group multicast (default off)\n"
		"       [-a send group anycast (default off)\n"
		"       [-r request response on sent Xcast (default off)\n"
		"       [-d destination member instance (default 0)\n"
		"       [-i member instance (default 0)\n"
		"       [-l <msglen>] message size (default 66000)\n"
		"       -a, -b , -m are mutually exclusive\n");
	return 1;
}

int main(int argc, char *argv[], char *envp[])
{
	int option;
	int len = 66000;
	int mtyp = NONE;
	bool ucast_reply = false;
	unsigned int snt;
	struct pollfd pfd[2];
	int conf_inst = 0;
	unsigned long long elapsed_ms_tot;
	unsigned long long elapsed_ms_intv;
	struct timeval start_time;
	struct timeval start_intv;
	struct timeval now;
	unsigned long msg_per_sec_tot, msg_per_sec_intv;
	unsigned int thruput_tot, thruput_intv;

	while ((option = getopt(argc, argv, ":abd:i:l:mr")) != -1) {
		switch (option) {
		case 'l':
			if (optarg)
				len = atoi(optarg);
			continue;
		case 'i':
			if (optarg)
				conf_inst = atoi(optarg);
			continue;
		case 'b':
			if (mtyp)
				return syntax(argv);
			mtyp = BCAST;
			continue;
		case 'm':
			if (mtyp)
				return syntax(argv);
			mtyp = MCAST;
			continue;
		case 'a':
			if (mtyp)
				return syntax(argv);
			mtyp = ACAST;
			continue;
		case 'r':
			ucast_reply = true;
			continue;
		case 'd':
			if (optarg)
				dst.instance = atoi(optarg);
			continue;
		default:
			return syntax(argv);
		}
	}

	/* Create subsriber socket and discover other members */
	pfd[0].fd = tipc_topsrv_conn(0);
	if (pfd[0].fd < 0)
		die("Failed to create subscriber socket\n");
	pfd[0].events = POLLIN | POLLHUP;

	if (tipc_srv_subscr(pfd[0].fd, self.type, 0, ~0, true, -1))
		die("subscription for group members failed\n");

	printf("Waiting for remote members\n");
	while (0 < poll(pfd, 1, 1000)) {
		if (!(pfd[0].revents & POLLIN))
			break;
		discover_member(pfd[0].fd);
	}


	/* Join group */
	pfd[1].fd = tipc_socket(SOCK_RDM);
	if (pfd[1].fd < 0)
		die("Failed to create member socket\n");
	self.instance = conf_inst ? conf_inst : 0;
	self.domain = tipc_own_cluster();
	if (tipc_join(pfd[1].fd, &self))
		die("Failed to join group\n");
	tipc_ntoa(&self, self_str, sizeof(self_str));
	printf("Joined as member %s\n", self_str);

	/* Start receiving/transmitting */
        pfd[1].events = POLLIN;
	memset(buf, 0xff, sizeof(buf));
	gettimeofday(&start_time, 0);
	gettimeofday(&start_intv, 0);

	while (1) {

		group_transceive(&pfd[0], mtyp , ucast_reply, len);

		if ((!snt_bc || (snt_bc % INTV_SZ)) &&
		    (!snt_mc || (snt_mc % INTV_SZ)) &&
		    (!snt_ac || (snt_ac % INTV_SZ)))
		    continue;

		/* Print out statistics at regular intervals */
		snt = snt_bc + snt_mc + snt_ac + snt_uc;
		gettimeofday(&now, 0);
		elapsed_ms_tot = elapsedmsec(&start_time, &now);
		msg_per_sec_tot = (snt * 1000) / elapsed_ms_tot;
		thruput_tot = (msg_per_sec_tot * BUF_LEN * 8) / 1000000;

		elapsed_ms_intv = elapsedmsec(&start_intv, &now);
		msg_per_sec_intv = (INTV_SZ * 1000) / elapsed_ms_intv;
		thruput_intv = (msg_per_sec_intv * BUF_LEN * 8) / 1000000;
		printf("Sent %u broadcast, %u multicast, %u anycast, "
		       "throughput %u MB/s, last intv %u MB/s\n",
		       snt_bc, snt_mc, snt_ac, thruput_tot, thruput_intv);
		start_intv = now;
	}
	exit(0);
}
