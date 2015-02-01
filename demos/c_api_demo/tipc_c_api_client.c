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

#define SRV_TYPE  18888
#define SRV_INST  17
#define BUF_SZ 40

#define die(fmt, arg...)  \
	do { \
            printf("Client:" fmt, ## arg); \
            perror(NULL); \
            exit(1);\
        } while(0)

int main(int argc, char *argv[], char *dummy[])
{
	int sd, rc, err;
	struct tipc_addr cli, sockid;;
	struct tipc_addr srv = {SRV_TYPE, SRV_INST, 0};
	struct tipc_addr dum = {42,1,0};
	char cbuf[BUF_SZ], sbuf[BUF_SZ], rmsg[BUF_SZ];
	char msg[BUF_SZ] = {"Hello World"};

	printf("****** TIPC C API Demo Client Started ******\n\n");

	tipc_ntoa(&srv, sbuf, BUF_SZ);
	printf("Waiting for Service %s\n", sbuf);
	tipc_srv_wait(&srv, -1);
	
	printf("\nSending msg: %s on SOCK_RDM to %s\n", msg, sbuf);
	sd = tipc_socket(SOCK_RDM);
	if (sd <= 0)
		die("failed to create socket");
	tipc_sockid(sd, &sockid);
	if (tipc_sock_rejectable(sd) < 0)
		die("Set rejectable failed\n");

	if (tipc_sendto(sd, msg, BUF_SZ, &srv) != BUF_SZ)
		die("sendto() failed\n");

	rc = tipc_recvfrom(sd, rmsg, BUF_SZ, &srv, &cli, &err);
	if ((rc < 0) || err)
		die("Unexpected response\n");

	printf("Received response: %s\n"
	       "         %s --> %s\n", rmsg, tipc_ntoa(&srv, sbuf, BUF_SZ),
	       tipc_ntoa(&cli, cbuf, BUF_SZ));

	dum.domain = srv.domain;
	printf("\nSending msg: %s on SOCK_RDM to non-existing %s\n", msg,
	       tipc_ntoa(&dum, sbuf, BUF_SZ));

	if (tipc_sendto(sd, msg, BUF_SZ, &dum) != BUF_SZ)
		die("sendto() failed\n");
	
	rc = tipc_recvfrom(sd, rmsg, BUF_SZ, &srv, &cli, &err);
	if ((rc < 0) || !err)
		die("Unexpected response\n");
	printf("Received rejected msg: %s\n"
	       "         %s --> %s, err %i\n", rmsg,
	       tipc_ntoa(&srv, sbuf, BUF_SZ),
	       tipc_ntoa(&cli, cbuf, BUF_SZ),
	       err);

	tipc_close(sd);
	printf("\n****** TIPC C API Demo Finished ******\n\n");
	exit(0);
}
