/* ------------------------------------------------------------------------
 *
 * tipc_c_api_server.c
 *
 * Short description:
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
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "tipcc.h"
#include <poll.h>

#define RDM_SRV_TYPE     18888
#define STREAM_SRV_TYPE  17777
#define SEQPKT_SRV_TYPE  16666
#define BUF_SZ 40

#define die(fmt, arg...) do             \
{                                       \
	printf("Server: "fmt, ## arg);	\
	perror(NULL);			\
	exit(1);			\
} while(0)

static int bind_service(int type, tipc_domain_t scope, int sktype, char *sk_str)
{
	int sd;
	struct tipc_addr sockid;
	char cbuf[BUF_SZ], sbuf[BUF_SZ];

	sd = tipc_socket(sktype);
	if (sd <= 0)
		die("failed to create %s socket", sk_str);
	if (tipc_bind(sd, type, 0, ~0, scope))
		die("failed to bind %s socket", sk_str);
	tipc_sockid(sd, &sockid);
	tipc_ntoa(&sockid, sbuf, BUF_SZ);
	printf("Bound %s socket %s to %s\n", sk_str,
	       tipc_ntoa(&sockid, cbuf, BUF_SZ),
	       tipc_rtoa(type, 0, ~0, scope, sbuf, BUF_SZ));
	return sd;
}

static void recv_rdm_msg(int sd)
{
	struct tipc_addr cli, srv;
	char cbuf[BUF_SZ], sbuf[BUF_SZ], msg[BUF_SZ];

	printf("\n-------------------------------------\n");
	if (tipc_recvfrom(sd, msg, BUF_SZ, &cli, &srv, 0) <= 0)
		die("unexpected message on RDM socket\n");

	printf("Received msg: %s on SOCK_RDM\n"
	       "         %s <-- %s\n",msg,
	       tipc_ntoa(&srv, sbuf, BUF_SZ),
	       tipc_ntoa(&cli, cbuf, BUF_SZ));

	sprintf(msg,"Huh?");
	printf("Responding with: %s\n"
	       "                        --> %s\n", msg,
	       tipc_ntoa(&cli, sbuf, BUF_SZ));

	if (0 >= tipc_sendto(sd, msg, BUF_SZ, &cli))
		die("Server: failed to send\n");
	printf("-------------------------------------\n");
}

static int recv_stream_setup(int sd)
{
	int newsd;
	struct tipc_addr cli;
	char cbuf[BUF_SZ], msg[BUF_SZ];

	printf("\n-------------------------------------\n");
	tipc_listen(sd, 32);
	newsd = tipc_accept(sd, &cli);
	if (newsd <= 0)
		die("accept on SOCK_STREAM failed\n");

	tipc_ntoa(&cli, cbuf, BUF_SZ);
	printf("SOCK_STREAM connection established\n"
	       "                        <-- %s\n", cbuf);

	if (tipc_recv(newsd, msg, BUF_SZ, 1) <= 0)
		die("unexpected message on STREAM socket\n");

	printf("Received msg: %s on STREAM connection\n", msg);

	sprintf(msg,"Huh?");
	printf("Responding with: %s\n", msg);

	if (tipc_send(newsd, msg, BUF_SZ) <= 0)
		die("failed to respond\n");
	printf("-------------------------------------\n");
	return newsd;
}

int main(int argc, char *argv[], char *dummy[])
{
	tipc_domain_t scope = tipc_own_cluster();
	int rdmsd, strsd;
	struct pollfd pfd[1];

	printf("****** TIPC C API Demo Server Started ******\n\n");
	
	memset(pfd, 0, sizeof(pfd));
	rdmsd = bind_service(RDM_SRV_TYPE, scope, SOCK_RDM, "RDM");
	strsd = bind_service(STREAM_SRV_TYPE, 0, SOCK_STREAM, "STREAM");
	
	while (1) {
		recv_rdm_msg(rdmsd);
		pfd[0].fd = recv_stream_setup(strsd);
		pfd[0].events = POLLIN | POLLHUP;
		
		while (poll(pfd, 1, 3000000)) {
			if (!(pfd[0].revents & POLLHUP))
				continue;
			printf("\n-------------------------------------\n");
			printf("SOCK_STREAM connection hangup\n");
			tipc_close(pfd[0].fd);
			pfd[0].fd = 0;
			break;
		}
	}
#if 0
       
	sd = tipc_socket(SOCK_RDM);
	if (sd <= 0)
		die("failed to create socket");
	if (tipc_bind(sd, RDM_SRV_TYPE, 0, ~0, scope))
		die("failed to bind RDM socket");
	tipc_sockid(sd, &sockid);
	tipc_ntoa(&sockid, sbuf, BUF_SZ);
	printf("Bound RDM socket %s to %s\n",
	       tipc_ntoa(&sockid, cbuf, BUF_SZ),
	       tipc_rtoa(srv.type, 0, ~0, scope, sbuf, BUF_SZ));

	sd = tipc_socket(SOCK_STREAM);
	if (sd <= 0)
		die("failed to create STREAM socket");
	if (tipc_bind(sd, STREAM_SRV_TYPE, 0, ~0, 0))
		die("failed to bind STREAM socket");
	tipc_sockid(sd, &sockid);
	tipc_ntoa(&sockid, sbuf, BUF_SZ);
	printf("Bound STREAM socket %s to %s\n\n",
	       tipc_ntoa(&sockid, cbuf, BUF_SZ),
	       tipc_rtoa(srv.type, 0, ~0, scope, sbuf, BUF_SZ));

	sd = tipc_socket(SOCK_STREAM);
	if (sd <= 0)
		die("failed to create STREAM socket");
	if (tipc_bind(sd, STREAM_SRV_TYPE, 0, ~0, 0))
		die("failed to bind STREAM socket");
	tipc_sockid(sd, &sockid);
	tipc_ntoa(&sockid, sbuf, BUF_SZ);
	printf("Bound STREAM socket %s to %s\n",
	       tipc_ntoa(&sockid, cbuf, BUF_SZ),
	       tipc_rtoa(srv.type, 0, ~0, scope, sbuf, BUF_SZ));

	rc = tipc_recvfrom(sd, msg, BUF_SZ, &cli, &srv, 0);
	if (rc <= 0)
		die("Server: unexpected message\n");

	printf("Received msg: %s\n"
	       "         %s --> %s\n",msg,
	       tipc_ntoa(&cli, cbuf, BUF_SZ),
	       tipc_ntoa(&srv, sbuf, BUF_SZ));

	sprintf(msg,"Huh?");
	printf("Responding with: %s\n"
	       "           --> %s\n", msg,
	       tipc_ntoa(&cli, sbuf, BUF_SZ));

	if (0 >= tipc_sendto(sd, msg, BUF_SZ, &cli))
		die("Server: failed to send\n");

	close (sd);
#endif
	printf("\n****** TIPC C API Demo Server Finished ******\n\n");
	exit(0);
}
