#include <stdio.h>
#include <linux/types.h>
#include <stdlib.h>
#include <linux/tipc.h>
#include <sys/socket.h>

#define MY_TYPE 666

static void die()
{
	perror("");
	exit(-1);
}

static void client()
{
	int sd;
	char *msg = "HDDDDDDD";
	struct sockaddr_tipc sa = {
		.family = AF_TIPC,
		.addrtype = TIPC_ADDR_NAME,
		.addr.name.name.type = MY_TYPE,
		.addr.name.domain = 0,
	};

	/*Address a random instance in MY_TYPE*/
	sa.addr.name.name.instance = rand();
	if (!(sd = socket(AF_TIPC, SOCK_RDM, 0)))
		die();
	/*Give the server a chance to start*/
	sleep(1);
	printf("Client: Send packet to {%u, %u}\n",
		MY_TYPE, sa.addr.name.name.instance);
	sendto(sd, msg, 8, 0, (struct sockaddr*)&sa, sizeof(sa));
}

static void server()
{
	int sd, n;
	struct sockaddr_tipc sa_sender = {0};
	struct cmsghdr *cmsg;
	struct tipc_name_seq *name;
	char header[1];
	char data[7];
	char control[255];
	struct sockaddr_tipc sa = {
		.family = AF_TIPC,
		.addrtype = TIPC_ADDR_NAMESEQ,
		.addr.nameseq.type = MY_TYPE,
		.addr.nameseq.lower = 0,
		.addr.nameseq.upper = ~0,
		.scope = TIPC_ZONE_SCOPE,
	};
	/*Set up an IOV with a 1 byte header and 7 byte data buffer*/
	struct iovec iov[2] = {
		{.iov_base = header,
		 .iov_len = 1},
		{.iov_base = data,
		 .iov_len = 7}
	};
	struct msghdr m = {
		.msg_iov = iov,
		.msg_iovlen = 2,
		.msg_name = &sa_sender,
		.msg_namelen = sizeof(sa_sender),
		.msg_control = control,
		.msg_controllen = 255,
	};

	if (!(sd = socket(AF_TIPC, SOCK_RDM, 0)))
		die();
	if (0 != bind(sd, (struct sockaddr*)&sa, sizeof(sa)))
		die();
	if ((n = recvmsg(sd, &m, 0)) <=0)
		die();

	printf("Server received %d bytes\n", n);
	/*Look for the TIPC_DESTNAME cmsg, this holds information
	 * about which type/lower/upper that the message was transmitted
	 * to. In the case where a server */
	for (cmsg = CMSG_FIRSTHDR(&m);
	     cmsg != NULL;
	     cmsg = CMSG_NXTHDR(&m, cmsg)) {
		if ((cmsg->cmsg_level = SOL_TIPC) &&
		    (cmsg->cmsg_type == TIPC_DESTNAME))
			name = (struct tipc_name_seq*)CMSG_DATA(cmsg);
	}
	if (name) {
		printf("Message was sent to {%u, %u, %u}\n",
			name->type, name->lower, name->upper);
		printf("Header: %c Data:%s\n",header[0], data);
	}
	else
		printf("Fail\n");
}

int main (int argc, char* argv[])
{
	srand(time(NULL));
	if (!fork())
		client();
	else
		server();
	return 0;
}
