/* ------------------------------------------------------------------------
 *
 * tipcc.c
 *
 * Short description: TIPC C binding API
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
 * Neither the name of Ericsson Canada nor the names of its
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
 * Version 0.9: Jon Maloy, 2015
 *
 * ------------------------------------------------------------------------
 */

#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/socket.h>
#include <linux/tipc.h>
#include "tipcc.h"

static tipc_domain_t own_node = 0;

static inline bool my_scope(tipc_domain_t domain)
{
	if (!domain)
		return true;
	if (tipc_own_node() == domain)
		return true;
	if (tipc_own_cluster() == domain)
		return true;
	if (tipc_own_zone() == domain)
		return true;
	return false;
}

tipc_domain_t tipc_own_node(void)
{
	struct tipc_addr sockid;
	int sd;

	if (own_node)
		return own_node;

	sd = tipc_socket(SOCK_RDM);
	if (tipc_sockid(sd, &sockid) == 0)
		own_node = sockid.domain;
	close(sd);
	return own_node;
}

tipc_domain_t tipc_own_cluster(void)
{
	return tipc_own_node() & ~0xfff;
}

tipc_domain_t tipc_own_zone(void)
{
	return tipc_own_node() & ~0xffffff;
}

int tipc_socket(int sk_type)
{
	return socket(AF_TIPC, sk_type, 0);
}

int tipc_sock_non_block(int sd)
{
	int flags;

	flags = fcntl(sd, F_GETFL, 0);
	if (flags < 0)
		return -1;
	flags = O_NONBLOCK;
	if (fcntl(sd, F_SETFL, flags) < 0)
		return -1;
	return sd;
}

int tipc_sock_rejectable(int sd)
{
	int val = 0;

	return setsockopt(sd, SOL_TIPC, TIPC_DEST_DROPPABLE,
			  &val, sizeof(val));
}

int tipc_close(int sd)
{
	return close(sd);
}

int tipc_sockid(int sd, struct tipc_addr *id)
{
	struct sockaddr_tipc addr;
	socklen_t sz = sizeof(addr);

	if (!id)
		return -1;
	if (getsockname(sd, (struct sockaddr *)&addr, &sz) < 0)
		return -1;
	id->type = 0;
	id->instance = addr.addr.id.ref;
	id->domain = addr.addr.id.node;
	return 0;
}

int tipc_bind(int sd, uint32_t type, uint32_t lower, uint32_t upper,
	      tipc_domain_t scope)
{
	struct sockaddr_tipc addr = {
		.family                  = AF_TIPC,
		.addrtype                = TIPC_ADDR_NAMESEQ,
		.addr.nameseq.type       = type,
		.addr.nameseq.lower      = lower,
		.addr.nameseq.upper      = upper
	};

	if (tipc_own_node() == scope)
		addr.scope = TIPC_NODE_SCOPE;
	if (tipc_own_cluster() == scope)
		addr.scope = TIPC_CLUSTER_SCOPE;
	if (tipc_own_zone() == scope)
		addr.scope = TIPC_ZONE_SCOPE;
	/* TODO: introduce support for global scope in module */
	if (!scope)
		addr.scope = TIPC_ZONE_SCOPE;
	if (!my_scope(scope))
		return -1;
	return bind(sd, (struct sockaddr *)&addr, sizeof(addr));
}

int tipc_unbind(int sd, uint32_t type, uint32_t lower, uint32_t upper)

{
	struct sockaddr_tipc addr = {
		.family                  = AF_TIPC,
		.addrtype                = TIPC_ADDR_NAMESEQ,
		.addr.nameseq.type       = type,
		.addr.nameseq.lower      = lower,
		.addr.nameseq.upper      = upper,
		.scope                   = -1
	};
	return bind(sd, (struct sockaddr *)&addr, sizeof(addr));
}

int tipc_connect(int sd, const struct tipc_addr *dst)
{
	struct sockaddr_tipc addr;

	if (!dst)
		return -1;
	addr.family                  = AF_TIPC;
	addr.addrtype                = TIPC_ADDR_NAME;
	addr.addr.name.name.type     = dst->type;
	addr.addr.name.name.instance = dst->instance;
	addr.addr.name.domain        = dst->domain;
	return connect(sd, (struct sockaddr*)&addr, sizeof(addr));
}

int tipc_listen(int sd, int backlog)
{
	return listen(sd, backlog);
}

int tipc_accept(int sd, struct tipc_addr *src)
{
	struct sockaddr_tipc addr;
	socklen_t addrlen = sizeof(addr);
	int rc;

	rc = accept(sd, (struct sockaddr *) &addr, &addrlen);
	if (src) {
		src->type = 0;
		src->instance = addr.addr.id.ref;
		src->domain = addr.addr.id.node;
	}
	return rc;
}

int tipc_send(int sd, const char *msg, size_t msg_len)
{
	return send(sd, msg, msg_len, 0);
}

int tipc_sendmsg(int sd, const struct msghdr *msg)
{
	return sendmsg(sd, msg, 0);
}

int tipc_sendto(int sd, const char *msg, size_t msg_len,
		const struct tipc_addr *dst)
{
	struct sockaddr_tipc addr;

	if(!dst)
		return -1;

	addr.family = AF_TIPC;
	if (dst->type) {
		addr.addrtype = TIPC_ADDR_NAME;
		addr.addr.name.name.type = dst->type;
		addr.addr.name.name.instance = dst->instance;
		addr.addr.name.domain = dst->domain;
	} else {
		addr.addrtype = TIPC_ADDR_ID;
		addr.addr.id.ref = dst->instance;
		addr.addr.id.node = dst->domain;
	}
	return sendto(sd, msg, msg_len, 0,
		      (struct sockaddr*)&addr, sizeof(addr));
}

int tipc_mcast(int sd, const char *msg, size_t msg_len,
	       const struct tipc_addr *dst)
{
	struct sockaddr_tipc addr = {
		.family                  = AF_TIPC,
		.addrtype                = TIPC_ADDR_MCAST,
		.addr.name.domain        = TIPC_CLUSTER_SCOPE
	};
	if(!dst)
		return -1;
	addr.addr.nameseq.type = dst->type;
	addr.addr.nameseq.lower = dst->instance;
	addr.addr.nameseq.upper = dst->instance;
	if (dst->domain != tipc_own_cluster())
		return -ENOTSUP;
	return sendto(sd, msg, msg_len, 0,
		      (struct sockaddr*)&addr, sizeof(addr));
}

int tipc_recv(int sd, char* buf, size_t buf_len, bool waitall)
{
	int flags = waitall ? MSG_WAITALL : 0;

	return recv(sd, buf, buf_len, flags);

}

int tipc_recvfrom(int sd, char *buf, size_t len, struct tipc_addr *src,
		  struct tipc_addr *dst, int *err)
{
	int rc, _err = 0;
	struct sockaddr_tipc addr;
	struct iovec iov = {buf, len};
	struct msghdr msg = {0, };
	char anc_space[CMSG_SPACE(8) + CMSG_SPACE(1024) + CMSG_SPACE(16)];
	struct cmsghdr *anc;

	msg.msg_name = &addr;
	msg.msg_namelen = sizeof(addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	msg.msg_control = (struct cmsghdr *)anc_space;
	msg.msg_controllen = sizeof(anc_space);

	rc = recvmsg(sd ,&msg ,0);
	if (rc < 0)
		return rc;
	if (src) {
		src->type = 0;
		src->instance = addr.addr.id.ref;
		src->domain = addr.addr.id.node;
	}
	anc = CMSG_FIRSTHDR(&msg);

	if (anc && (anc->cmsg_type == TIPC_ERRINFO)) {
		_err = *(int*)(CMSG_DATA(anc));
		rc = *(int*)(CMSG_DATA(anc) + 4);
		if (rc > len)
			rc = len;
		anc = CMSG_NXTHDR(&msg, anc);
		memcpy(buf, (char*)CMSG_DATA(anc), rc);
		anc = CMSG_NXTHDR(&msg, anc);
	}

	if (_err)
		tipc_sockid(sd, src);

	if (err)
		*err = _err;
	else if (_err)
		rc = 0;

	if (!dst)
		return rc;

	if (anc && (anc->cmsg_type == TIPC_DESTNAME)) {
		dst->type = *((uint32_t*)(CMSG_DATA(anc)));
		dst->instance = *((uint32_t*)(CMSG_DATA(anc) + 4));
		dst->domain = 0;
	} else {
		tipc_sockid(sd, dst);
	}
	return rc;
}

int tipc_topsrv_conn(tipc_domain_t node)
{
	int sd;
	struct tipc_addr srv = {TIPC_TOP_SRV, TIPC_TOP_SRV, node};

	sd = tipc_socket(SOCK_SEQPACKET);
	if (sd <= 0)
		return sd;
	if (tipc_connect(sd, &srv) < 0)
		tipc_close(sd);
	return sd;
}

int tipc_srv_subscr(int sd, uint32_t type, uint32_t lower, uint32_t upper,
		    bool all, int expire)
{
	struct tipc_subscr subscr;

	subscr.seq.type  = type;
	subscr.seq.lower = lower;
	subscr.seq.upper = upper;
	subscr.timeout   = expire < 0 ? TIPC_WAIT_FOREVER : expire;
	subscr.filter    = all ? TIPC_SUB_PORTS : TIPC_SUB_SERVICE;
	if (send(sd, &subscr, sizeof(subscr), 0) != sizeof(subscr))
		return -1;
	return 0;
}

int tipc_srv_evt(int sd, struct tipc_addr *srv, bool *available, bool *expired)
{
	struct tipc_event evt;

        if (recv(sd, &evt, sizeof(evt), 0) != sizeof(evt))
                return -1;
	if (evt.event == TIPC_SUBSCR_TIMEOUT) {
		if (expired)
			*expired = true;
		return 0;
	}
	if (srv) {
		srv->type = evt.s.seq.type;
		srv->instance = evt.found_lower;
		srv->domain = evt.port.node;
	}
	if (available)
		*available = (evt.event == TIPC_PUBLISHED);
	if (expired)
		*expired = false;
	return 0;
}

bool tipc_srv_wait(const struct tipc_addr *srv, int wait)
{
	int sd, rc = 0;
	bool up = false;
	bool expired = false;

	sd = tipc_topsrv_conn(0);
	if (sd < 0)
		return false;
	if (tipc_srv_subscr(sd, srv->type, srv->instance,
			    srv->instance, false, wait))
		rc = -1;
	if (tipc_srv_evt(sd, 0, &up, &expired))
		rc = -1;
	close(sd);
	if (expired)
		return false;
	return rc ? false : up;
}

int tipc_neigh_subscr(tipc_domain_t node)
{
	int sd;

	sd = tipc_topsrv_conn(node);
	if (sd <= 0)
		return -1;
	if (tipc_srv_subscr(sd, TIPC_CFG_SRV, 0, ~0, true, TIPC_WAIT_FOREVER))
		return -1;
	return sd;
}

int tipc_neigh_evt(int sd, tipc_domain_t *neigh_node, bool *available)
{
	struct tipc_addr srv;
	int rc;

	rc = tipc_srv_evt(sd, &srv, available, 0);
	if (neigh_node)
		*neigh_node = srv.instance;
	return rc;
}

int tipc_link_subscr(tipc_domain_t node)
{
	int sd;

	sd = tipc_topsrv_conn(node);
	if (sd <= 0)
		return -1;
	if (tipc_srv_subscr(sd, TIPC_LINK_STATE, 0, ~0,
			    true, TIPC_WAIT_FOREVER))
		return -1;
	return sd;
}

int tipc_link_evt(int sd, tipc_domain_t *neigh_node, bool *available,
	          int *local_bearerid, int *remote_bearerid)
{
	struct tipc_event evt;

        if (recv(sd, &evt, sizeof(evt), 0) != sizeof(evt))
                return -1;

	if (local_bearerid)
		*local_bearerid = evt.port.ref & 0xffff;
	if (remote_bearerid)
		*remote_bearerid = (evt.port.ref >> 16) & 0xffff;
	if (neigh_node)
		*neigh_node = evt.found_lower;
	if (available)
		*available = (evt.event == TIPC_PUBLISHED);
	return 0;
}

char* tipc_linkname(char *buf, size_t len, tipc_domain_t peer, int bearerid)
{
	struct tipc_sioc_ln_req req = {peer, bearerid, };
	int sd, rc;

	buf[0] = 0;
	sd = tipc_socket(SOCK_RDM);
	if (sd <= 0)
		return buf;
	rc = ioctl(sd, SIOCGETLINKNAME, &req);
	if (rc < 0)
		return buf;
	strncpy(buf, req.linkname, len);
	buf[len] = 0;
	return buf;
}

char* tipc_dtoa(tipc_domain_t domain, char *buf, size_t len)
{
	snprintf(buf, len, "%u.%u.%u", tipc_zone(domain),
		 tipc_cluster(domain),tipc_node(domain));
	buf[len] = 0;
	return buf;
}

char* tipc_ntoa(const struct tipc_addr *addr, char *buf, size_t len)
{
	snprintf(buf, len, "%u:%u:%u.%u.%u",
		 addr->type, addr->instance, tipc_zone(addr->domain),
		 tipc_cluster(addr->domain),tipc_node(addr->domain));
	buf[len] = 0;
	return buf;
}

char* tipc_rtoa(uint32_t type, uint32_t lower, uint32_t upper,
		tipc_domain_t domain, char *buf, size_t len)
{
	snprintf(buf, len, "%u:%u:%u:%u.%u.%u", type, lower, upper,
		 tipc_zone(domain), tipc_cluster(domain),tipc_node(domain));
	buf[len] = 0;
	return buf;
}
