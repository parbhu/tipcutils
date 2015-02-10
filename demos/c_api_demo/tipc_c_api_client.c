/* ------------------------------------------------------------------------
 *
 * tipc_c_api_client.c
 *
 * Short description:
 * ------------------------------------------------------------------------
 *
 * Copyright (c) 2015 Ericsson Canada
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of Ericsson Research Canada nor the names of its
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
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include "tipcc.h"
#include <poll.h>

#define RDM_SRV_TYPE     18888
#define STREAM_SRV_TYPE  17777
#define SEQPKT_SRV_TYPE  16666
#define SRV_INST  17
#define BUF_SZ 40

#define die(fmt, arg...) do                     \
{                                               \
	printf("Client:" fmt, ## arg);		\
	perror(NULL);				\
	exit(1);				\
} while(0)

static void rdm_service_demo(int sd, bool up, tipc_domain_t *srv_node)
{
	struct tipc_addr cli, srv = {RDM_SRV_TYPE, SRV_INST, 0};
	char cbuf[BUF_SZ], sbuf[BUF_SZ], rmsg[BUF_SZ];
	char msg[BUF_SZ] = {"Hello World"};
	int rc, err;

	if (!up) {
		printf("Service on SOCK_RDM went down\n");
		return;
	}
	printf("\n-------------------------------------\n");
	printf("Service on SOCK_RDM came up\n");
	tipc_ntoa(&srv, sbuf, BUF_SZ);
	printf("Sending msg: %s on SOCK_RDM\n"
	       "                            -->%s\n", msg, sbuf);
	if (tipc_sock_rejectable(sd) < 0)
		die("Set rejectable failed\n");

	if (tipc_sendto(sd, msg, BUF_SZ, &srv) != BUF_SZ)
		die("sendto() failed\n");

	rc = tipc_recvfrom(sd, rmsg, BUF_SZ, &srv, &cli, &err);
	if ((rc < 0) || err)
		die("Unexpected response\n");

	printf("Received response: %s\n"
	       "         %s <-- %s\n", rmsg, 
	       tipc_ntoa(&cli, cbuf, BUF_SZ),
	       tipc_ntoa(&srv, sbuf, BUF_SZ));
	*srv_node = srv.domain;
}

static void rdm_reject_demo(int sd, bool up, tipc_domain_t srv_node)
{
	struct tipc_addr srv, cli, dum = {42, 1, srv_node};
	char cbuf[BUF_SZ], sbuf[BUF_SZ], msg[BUF_SZ] = {"Hello World"};
	int err, rc;

	if (!up)
		return;

	printf("\nSending msg: %s on SOCK_RDM \n"
	       "                            --> %s (non-existing)\n", msg,
	       tipc_ntoa(&dum, sbuf, BUF_SZ));

	if (tipc_sendto(sd, msg, BUF_SZ, &dum) != BUF_SZ) {
		printf("Client sendto() failed: No route to host\n");
		return;
	}
	
	rc = tipc_recvfrom(sd, msg, BUF_SZ, &srv, &cli, &err);
	if ((rc < 0) || !err)
		die("Unexpected response\n");
	printf("Received rejected msg: %s\n"
	       "         %s <-- %s, err %i\n",
	       msg, tipc_ntoa(&cli, cbuf, BUF_SZ),
	       tipc_ntoa(&srv, sbuf, BUF_SZ), err);
	printf("-------------------------------------\n");
}

static void stream_service_demo(int sd, bool up)
{
	struct tipc_addr srv = {STREAM_SRV_TYPE, SRV_INST, 0};
	char sbuf[BUF_SZ], msg[BUF_SZ] = {"Hello World"};

	if (!up) {
		printf("Service on SOCK_STREAM went down\n");
		return;
	}
	printf("\n\n-------------------------------------\n");
	printf("Service on SOCK_STREAM came up\n");
	tipc_ntoa(&srv, sbuf, BUF_SZ);
	printf("Performing two-way connect\n"
               "with message %s   -->%s\n",msg, sbuf);
	if (tipc_sendto(sd, msg, BUF_SZ, &srv) != BUF_SZ)
		die("send() failed\n");

	if (tipc_recv(sd, msg, BUF_SZ, 1) < 0)
		die("Unexpected response\n");

	printf("Received response: %s on SOCK_STREAM connection\n", msg);

	printf("SOCK_STREAM connection established \n"
	       "                           --> %s\n", sbuf);

	printf("-------------------------------------\n");
}


static void seqpacket_service_demo(int sd, bool up)
{
	struct tipc_addr srv = {SEQPKT_SRV_TYPE, SRV_INST, 0};
	char sbuf[BUF_SZ], msg[BUF_SZ] = {"Hello World"};

	if (!up) {
		printf("Service on SOCK_SEQPACKET went down\n");
		return;
	}
	printf("\n\n-------------------------------------\n");
	printf("Service on SOCK_SEQPACKET came up\n");
	tipc_ntoa(&srv, sbuf, BUF_SZ);
	printf("Connecting to:              -->%s\n",sbuf);
	if (tipc_connect(sd, &srv) < 0)
		die("connect() failed\n");

	printf("Sending msg: %s on connection\n", msg);
	if (tipc_send(sd, msg, BUF_SZ) != BUF_SZ)
		die("send() failed\n");

	if (tipc_recv(sd, msg, BUF_SZ, 1) < 0)
		die("Unexpected response\n");

	printf("Received response: %s on SOCK_SEQPACKET connection\n", msg);
	printf("-------------------------------------\n");
}

int main(int argc, char *argv[], char *dummy[])
{
	bool up;
	tipc_domain_t srv_node = 0;
	char sbuf[BUF_SZ];
	struct tipc_addr srv = {RDM_SRV_TYPE, SRV_INST, 0};
	struct pollfd pfd[6];

	printf("****** TIPC C API Demo Client Started ******\n\n");

	memset(pfd, 0, sizeof(pfd));
	tipc_ntoa(&srv, sbuf, BUF_SZ);
	printf("Waiting for Service %s\n", sbuf);
	tipc_srv_wait(&srv, -1);
	
	/* Create traffic sockets */
	pfd[0].fd = tipc_socket(SOCK_RDM);
	pfd[0].events = POLLIN;
	pfd[1].fd = tipc_socket(SOCK_STREAM);
	pfd[1].events = POLLIN | POLLHUP;
	pfd[2].fd = tipc_socket(SOCK_SEQPACKET);
	pfd[2].events = POLLIN | POLLHUP;

	
	/* Subscribe for service events */
	pfd[3].fd = tipc_topsrv_conn(0);
	pfd[3].events = POLLIN | POLLHUP;
	if (tipc_srv_subscr(pfd[3].fd, RDM_SRV_TYPE, 0, ~0, false, -1))
		die("subscribe for RDM server failed\n");
	if (tipc_srv_subscr(pfd[3].fd, STREAM_SRV_TYPE, 0, ~0, false, -1))
		die("subscribe for STREAM server failed\n");
	if (tipc_srv_subscr(pfd[3].fd, SEQPKT_SRV_TYPE, 0, ~0, false, -1))
		die("subscribe for SEQPACKET server failed\n");


	while (poll(pfd, 6, 3000000)) {
		if (pfd[1].revents & POLLHUP) {
			printf("SOCK_STREAM connection hangup\n");
			tipc_close(pfd[1].fd);
			pfd[1].fd = tipc_socket(SOCK_STREAM);
		}
		if (pfd[2].revents & POLLHUP) {
			printf("SOCK_SEQPACKET connection hangup\n");
			tipc_close(pfd[2].fd);
			pfd[2].fd = tipc_socket(SOCK_SEQPACKET);
		}
		if (pfd[3].revents & POLLIN) {
			if (tipc_srv_evt(pfd[3].fd, &srv, &up, 0))
				die("reception of service event failed\n");
			if (srv.type == RDM_SRV_TYPE) {
				rdm_service_demo(pfd[0].fd, up, &srv_node);
				rdm_reject_demo(pfd[0].fd, up, srv_node);
			}
			if (srv.type == STREAM_SRV_TYPE)
				stream_service_demo(pfd[1].fd, up);
			if (srv.type == SEQPKT_SRV_TYPE)
				seqpacket_service_demo(pfd[2].fd, up);
		}
	}
	printf("\n****** TIPC C API Demo Finished ******\n\n");
	exit(0);
}
