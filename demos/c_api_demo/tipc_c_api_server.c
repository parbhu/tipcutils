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

#define SRV_TYPE  18888
#define SRV_INST  17
#define BUF_SZ 40

#define die(fmt, arg...)  \
	do {			     \
            printf("Server: "fmt, ## arg); \
            perror(NULL); \
            exit(1);\
        } while(0)

int main(int argc, char *argv[], char *dummy[])
{
	int sd, rc;
	tipc_addr_t cli, sockid;
	tipc_addr_t srv = {SRV_TYPE, SRV_INST, 0};
	char msg[BUF_SZ], cbuf[BUF_SZ], sbuf[BUF_SZ];
	tipc_domain_t scope = tipc_own_cluster();

	printf("****** TIPC C API Demo Server Started ******\n\n");

	sd = tipc_socket(SOCK_RDM);
	if (sd <= 0)
		die("failed to create socket");

	if (tipc_bind(sd, SRV_TYPE, 0, ~0, scope))
		die("failed to bind");

	tipc_sockid(sd, &sockid);
	tipc_ntoa(&sockid, sbuf, BUF_SZ);
	printf("Bound socket %s to \n",
	       tipc_rtoa(srv.type, 0, ~0, scope, sbuf, BUF_SZ));

	rc = tipc_recvfrom(sd, msg, BUF_SZ, &cli, &srv, 0);
	if (rc <= 0)
		die("Server: unexpected message\n");

	printf("Received msg: %s\n"
	       "         %s --> %s\n",msg,
	       tipc_ntoa(&cli, cbuf, BUF_SZ),
	       tipc_ntoa(&srv, sbuf, BUF_SZ));

	sprintf(msg,"Uh??");
	printf("Responding with: %s\n"
	       "           --> %s\n", msg,
	       tipc_ntoa(&cli, sbuf, BUF_SZ));

	if (0 >= tipc_sendto(sd, msg, BUF_SZ, &cli))
		die("Server: failed to send\n");

	close (sd);

	printf("\n****** TIPC C API Demo Server Finished ******\n\n");
}
