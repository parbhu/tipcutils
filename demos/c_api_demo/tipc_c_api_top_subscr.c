/* ------------------------------------------------------------------------
 *
 * tipc_c_api_top_subscr.c
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
	printf("Topology Client:" fmt, ## arg);		\
	perror(NULL);				\
	exit(EXIT_FAILURE);				\
} while(0)

int main(int argc, char *argv[], char *dummy[])
{
	bool up;
	tipc_domain_t neigh_node = 0, neigh_topsrvnode = 0;
	char sbuf[BUF_SZ], neigh_topsrvbuf[BUF_SZ];
	struct tipc_addr srv = {RDM_SRV_TYPE, SRV_INST, 0};
	int local_bearer, remote_bearer;
	struct pollfd pfd[6];

	printf("****** TIPC C API Topology Service Client Started ******\n\n");

	memset(pfd, 0, sizeof(pfd));
	tipc_ntoa(&srv, sbuf, BUF_SZ);
	
	/* Subscribe for service events */
	pfd[0].fd = tipc_topsrv_conn(0);
	pfd[0].events = POLLIN | POLLHUP;
	if (tipc_srv_subscr(pfd[0].fd, RDM_SRV_TYPE, 0, ~0, false, -1))
		die("subscribe for RDM server failed\n");
	if (tipc_srv_subscr(pfd[0].fd, STREAM_SRV_TYPE, 0, ~0, false, -1))
		die("subscribe for STREAM server failed\n");
	if (tipc_srv_subscr(pfd[0].fd, SEQPKT_SRV_TYPE, 0, ~0, false, -1))
		die("subscribe for SEQPACKET server failed\n");

	/* Subscribe for neighbor nodes */
	pfd[1].fd = tipc_neigh_subscr(0);
	if (pfd[1].fd <= 0)
		die("subscribe or neighbor nodes failed\n");		
	pfd[1].events = POLLIN | POLLHUP;

	/* Subscribe for neighbor links */
	pfd[2].fd = tipc_link_subscr(0);
	if (pfd[2].fd <= 0)
		die("subscribe for neigbor links failed\n");		
	pfd[2].events = POLLIN | POLLHUP;

	while (poll(pfd, 6, 3000000)) {
		if (pfd[0].revents & POLLIN) {
			if (tipc_srv_evt(pfd[0].fd, &srv, &up, 0))
				die("reception of service event failed\n");
			tipc_ntoa(&srv, sbuf, BUF_SZ);
			if (srv.type == RDM_SRV_TYPE) {
				if (up)
					printf("Service %s on SOCK_RDM is UP\n", sbuf);
				else
					printf("Service %s on SOCK_RDM is DOWN\n", sbuf);
			}
			if (srv.type == STREAM_SRV_TYPE) {
				if (up)
					printf("Service %s on SOCK_STREAM is UP\n", sbuf);
				else
					printf("Service %s on SOCK_STREAM is DOWN\n", sbuf);
			}
			if (srv.type == SEQPKT_SRV_TYPE) {
				if (up)
					printf("Service %s on SOCK_SEQPACKET is UP\n", sbuf);
				else
					printf("Service %s on SOCK_SEQPACKET is DOWN\n", sbuf);
			}
		}
		if (pfd[1].revents & POLLIN) {
			if (tipc_neigh_evt(pfd[1].fd, &neigh_node, &up))
				die("reception of service event failed\n");
			tipc_dtoa(neigh_node, sbuf, BUF_SZ);
			if (up) {
				printf("Found neighbor node %s\n", sbuf);

				/* Allow only one "neighbor's neighbor's" subsription: */
				if (!pfd[3].fd && (neigh_node != tipc_own_node())) {
					neigh_topsrvnode = neigh_node;
					tipc_dtoa(neigh_node, neigh_topsrvbuf, BUF_SZ);
					pfd[3].fd = tipc_neigh_subscr(neigh_node);
					pfd[3].events = POLLIN | POLLHUP;
				}
			} else {
				printf("Lost contact with neighbor node %s\n", sbuf);
				if (neigh_node == neigh_topsrvnode) {
					tipc_close(pfd[3].fd);
					pfd[3].fd = 0;
				}
			}
		}
		if (pfd[2].revents & POLLIN) {
			if (tipc_link_evt(pfd[2].fd, &neigh_node, &up,
					  &local_bearer, &remote_bearer))
				die("reception of service event failed\n");
			tipc_linkname(sbuf,BUF_SZ, neigh_node, local_bearer);
			if (up) {
				printf("Found link %s\n", sbuf);
			} else {
				printf("Lost link %s\n", sbuf);
			}
		}
		if (pfd[3].revents & POLLHUP) {
			tipc_close(pfd[3].fd);
			pfd[3].fd = 0;
			continue;
		}
		if (pfd[3].revents & POLLIN) {
			if (tipc_neigh_evt(pfd[3].fd, &neigh_node, &up))
				die("reception of service event failed\n");
			if (up) {
				printf("Neighbor node %s found node %s\n",
				       neigh_topsrvbuf, tipc_dtoa(neigh_node, sbuf, BUF_SZ));
			} else {
				printf("Neighbor node %s lost contact with node %s\n",
				       neigh_topsrvbuf, tipc_dtoa(neigh_node, sbuf, BUF_SZ));
			}
		}

	}
	exit(0);
}
