sbin_PROGRAMS=tipc
tipc_SOURCES=tipc.c tipc.h \
	cmdl.c cmdl.h \
	netlink.c netlink.h \
	misc.c misc.h \
	socket.c \
	node.c \
	nametable.c \
	bearer.c
tipc_CFLAGS=-Werror $(LIBNL3_CFLAGS)
tipc_LDADD=-lrt $(LIBNL3_LIBS) -lnl-genl-3
tipc_LDFLAGS=-I /usr/include/libnl3/
