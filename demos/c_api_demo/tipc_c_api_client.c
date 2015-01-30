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
            printf(fmt": ", ## arg); \
            perror(NULL); \
            exit(1);\
        } while(0)

int main(int argc, char *argv[], char *dummy[])
{
	int sd, rc, err;
	tipc_addr_t srv = {SRV_TYPE, SRV_INST, 0};
	tipc_addr_t cli;

	char msg[BUF_SZ] = {"Hello World"};
	char cbuf[BUF_SZ];
	char sbuf[BUF_SZ];

	printf("****** TIPC C API Test Client Started ******\n\n");

	tipc_srv_wait(&srv, -1);
	sd = tipc_socket(SOCK_RDM);
	if (tipc_sock_rejectable(sd) < 0)
		die("Client: set rejectable failed\n");

	if (0 >= tipc_sendto(sd, msg, strlen(msg)+1, &srv))
		die("Client: failed to send\n");

	rc = tipc_recvfrom(sd, msg, sizeof(msg), &srv, &cli, &err);
	if (rc < 0)
		die("Client: unexpected response\n");
	tipc_addr2str(sbuf, BUF_SZ, &srv);
	tipc_addr2str(cbuf, BUF_SZ, &cli);

	printf("Client: received %s %s from %s to %s\n",
	       err ? "rejected":"response", msg, sbuf, cbuf);
	if (err)
		printf("Error Code is %u\n", err);
	printf("****** TIPC C API Test Finished ******\n\n");
	exit(0);
}
