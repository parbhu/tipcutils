/* ------------------------------------------------------------------------
 *
 * tipc_c_api_link_subscr.c
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

#define BUF_SZ 40

#define die(fmt, arg...)  \
	do { \
            printf(fmt": ", ## arg); \
            perror(NULL); \
            exit(1);\
        } while(0)

int main(int argc, char *argv[], char *dummy[])
{
	int sd;
	char sbuf[BUF_SZ];
	int local_bearer, remote_bearer;
	tipc_domain_t neigh;
	bool up;
	struct pollfd pfd = {0, };

	printf("****** TIPC C API Test Client Started ******\n\n");
	sd = tipc_link_subscr(0);
	if (sd < 0)
		die("Client: link subscription failed\n");

	pfd.fd = sd;
	pfd.events = POLLIN;

	while (poll(&pfd, 1, 3000000)) {
		if (tipc_link_evt(sd, &neigh, &up,
				  &local_bearer, &remote_bearer))
			die("Client: link event failed\n");
		tipc_linkname(sbuf,BUF_SZ, neigh, local_bearer);
		printf("Link %s is %s, local bearer %u, remote bearer %u\n",
		       sbuf, up ? "UP" : "DOWN",
		       local_bearer, remote_bearer);
	}
	exit(0);
}
