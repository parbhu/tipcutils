#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#include <libdaemon/dfork.h>
#include <libdaemon/dsignal.h>
#include <libdaemon/dlog.h>
#include <libdaemon/dpid.h>
#include <libdaemon/dexec.h>

/*No, this should not be <linux/tipc.h>*/
#include "tipc.h"

struct tipc_subscr node_sub = {
	.usr_handle = "node_evt"
};
struct tipc_subscr link_sub = {
	.usr_handle = "link_evt"
};
struct sockaddr_tipc sa_topsrv = {
	.family = AF_TIPC,
	.addrtype = TIPC_ADDR_NAME,
	.addr.name.name.type = TIPC_TOP_SRV,
	.addr.name.name.instance = TIPC_TOP_SRV,
	.addr.name.domain = 0
};

int topsrv_sd = 0;
unsigned int tipc_own_node = 0;

/**
 * topology_connect - Connect to the TIPC topology server
 *
 * returns the topology server socket fd
 */
int topology_connect()
{
	int sd;
	struct sockaddr_tipc sa_local = {0};
	socklen_t sa_len = sizeof(sa_local);

	if (!(sd = socket(AF_TIPC, SOCK_SEQPACKET, 0)))
	{
		daemon_log(LOG_ERR,"Failed to open TIPC socket");
		exit(-1);
	}

	if (connect(sd, (struct sockaddr*)&sa_topsrv, sizeof(sa_topsrv)) != 0)
	{
		daemon_log(LOG_ERR,"Failed to connect to the TIPC topology server");
		exit(-1);
	}

	if (getsockname(sd, (struct sockaddr*)&sa_local, &sa_len) != 0)
	{
		daemon_log(LOG_ERR,"Failed to fetch the local TIPC address");
		exit(-1);
	}
	tipc_own_node = sa_local.addr.id.node;
	return sd;
}

/**
 * subscribe_evt - subscribe for node and link state events
 */
void subscribe_evt()
{
	if (!topsrv_sd)
		topsrv_sd = topology_connect();
	link_sub.seq.type = htonl(TIPC_LINK_STATE);
	link_sub.seq.lower = htonl(0);
	link_sub.seq.upper = htonl(~0);
	link_sub.timeout = htonl(TIPC_WAIT_FOREVER);
	link_sub.filter = htonl(TIPC_SUB_PORTS);
	node_sub.seq.type = htonl(0);
	node_sub.seq.lower = htonl(0);
	node_sub.seq.upper = htonl(~0);
	node_sub.timeout = htonl(TIPC_WAIT_FOREVER);
	node_sub.filter = htonl(TIPC_SUB_PORTS);
	if (send(topsrv_sd, &link_sub, sizeof(link_sub), 0) != sizeof(link_sub))
	{
		daemon_log(LOG_ERR,"Failed to subscribe to TIPC link state events");
		exit(-1);
	}
	if (send(topsrv_sd, &node_sub, sizeof(node_sub), 0) != sizeof(node_sub))
	{
		daemon_log(LOG_ERR,"Failed to subscribe to TIPC node events");
		exit(-1);
	}
}

void log_link(struct tipc_event *e)
{
	struct tipc_sioc_ln_req lnr = {
		.peer = ntohl(e->found_lower),
		.bearer_id = ntohl(e->port.ref)
	};

	if (ioctl(topsrv_sd, SIOCGETLINKNAME, &lnr) < 0)
	{
		daemon_log(LOG_ERR,"ioctl");
		exit(-1);
	}
	if (ntohl(e->event) == TIPC_PUBLISHED)
		daemon_log(LOG_INFO,
			   "Established link <%s> on network plane %c",
			   lnr.linkname, lnr.bearer_id + 'A');
	else
		daemon_log(LOG_WARNING,
			   "Lost link <%s> on network plane %c",
			   lnr.linkname, lnr.bearer_id + 'A');
}

void log_node(struct tipc_event *e)
{
	unsigned int n = ntohl(e->port.node);

	if (n == tipc_own_node)
		return;

	if (ntohl(e->event) == TIPC_PUBLISHED)
		daemon_log(LOG_INFO,
			   "Established contact with node <%u.%u.%u>",
			   tipc_zone(n), tipc_cluster(n), tipc_node(n));
	else
		daemon_log(LOG_WARNING,
			   "Lost contact with node node <%u.%u.%u>",
			   tipc_zone(n), tipc_cluster(n), tipc_node(n));
}

void log_event(struct tipc_event *e)
{
	switch (ntohl(e->s.seq.type))
	{
		case TIPC_LINK_STATE:
			log_link(e);
		break;
		case 0:
			log_node(e);
		break;
		default:
			daemon_log(LOG_ERR, "Unknown event received\n");
		break;
	}
}

void tipc_event_logger()
{
	struct tipc_event e;
	int fd, sig, res;
	fd_set fds;

	subscribe_evt();
	/* Send OK to parent process */
	daemon_retval_send(0);
	daemon_log(LOG_INFO, "TIPC network event logger started");
        /* Prepare for select() on the signal fd */
	FD_ZERO(&fds);
	fd = daemon_signal_fd();
	FD_SET(fd, &fds);
	FD_SET(topsrv_sd, &fds);
	while (1) {
		if (select(FD_SETSIZE, &fds, 0, 0, 0) < 0) {
			if (errno == EINTR)
				continue;
			daemon_log(LOG_ERR, "select(): %s", strerror(errno));
			break;
		}
		if (FD_ISSET(fd, &fds)) {
			if ((sig = daemon_signal_next()) <= 0) {
				daemon_log(LOG_ERR, "daemon_signal_next() failed: %s",
					   strerror(errno));
				break;
			}
			switch (sig) {
			case SIGINT:
			case SIGQUIT:
			case SIGTERM:
				daemon_log(LOG_WARNING, "Interrupted by signal %s, shutting down",
					   strsignal(sig));
				return;
			break;
			}
		} else if (FD_ISSET(topsrv_sd, &fds)) {
			res = recv(topsrv_sd, &e, sizeof(e), 0);
			if (res != sizeof(e))
			{
				daemon_log(LOG_ERR, "Failed to read topology event: %s",
					   strerror(errno));
				return;
			}
			log_event(&e);
		}
	}
}

int main(int argc, char *argv[])
{
	pid_t pid;
	int res;

	if (daemon_reset_sigs(-1) < 0) {
		daemon_log(LOG_ERR, "Failed to reset all signal handlers: %s",
			   strerror(errno));
		return 1;
	}
	if (daemon_unblock_sigs(-1) < 0) {
		daemon_log(LOG_ERR, "Failed to unblock all signals: %s",
			   strerror(errno));
	}
	daemon_pid_file_ident = daemon_log_ident = daemon_ident_from_argv0(argv[0]);
	if ((pid = daemon_pid_file_is_running()) >= 0) {
		daemon_log(LOG_ERR, "Daemon already running on PID file %u", pid);
		return 1;
	}
	if (daemon_retval_init() < 0) {
		daemon_log(LOG_ERR, "Failed to create pipe.");
		return 1;
	}
	if ((pid = daemon_fork()) < 0) {
		daemon_retval_done();
		return 1;
	} else if (pid) { /* The parent */
		if ((res = daemon_retval_wait(20)) < 0) {
			daemon_log(LOG_ERR, "Could not recieve return value from daemon process: %s",
				   strerror(errno));
			return 255;
		}
		daemon_log(res != 0 ? LOG_ERR : LOG_INFO,
			   "Daemon returned %i as return value.", res);
		return res;
	} else { /*The daemon, close FD's, create pidfile, initialize signal
		   handling and enter event processing loop*/
		if (daemon_close_all(-1) < 0) {
			daemon_log(LOG_ERR, "Failed to close all file descriptors: %s",
				   strerror(errno));
			daemon_retval_send(1);
			goto finish;
		}
		if (daemon_pid_file_create() < 0) {
			daemon_log(LOG_ERR, "Could not create PID file (%s).",
				   strerror(errno));
			daemon_retval_send(2);
			goto finish;
		}
		if (daemon_signal_init(SIGINT, SIGTERM, SIGQUIT, SIGHUP, 0) < 0) {
			daemon_log(LOG_ERR, "Could not register signal handlers (%s).",
				   strerror(errno));
			daemon_retval_send(3);
			goto finish;
		}
		tipc_event_logger();
finish:
	if (topsrv_sd)
		shutdown(topsrv_sd, SHUT_RDWR);
	daemon_log(LOG_INFO, "Exiting...");
	daemon_retval_send(255);
	daemon_signal_done();
	daemon_pid_file_remove();
        return 0;
	}
}
