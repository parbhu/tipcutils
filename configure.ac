AC_PREREQ([2.4])
AC_INIT([tipcutils], [2.1.0], [richard.alpe@ericsson.com])

AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL
AC_CHECK_PROG([has_pkg_config],[pkg-config],[yes])
if test "x$has_pkg_config" != "xyes"; then
	AC_MSG_ERROR([pkg-config not found])
fi

# Checks for header files.
AC_CHECK_HEADERS([linux/tipc.h])
AC_CHECK_HEADERS([linux/tipc_netlink.h], [tipc_api=new],[tipc_api=old])

# Checks for library functions.
PKG_CHECK_MODULES(LIBNL3, libnl-3.0)
PKG_CHECK_MODULES([LIBDAEMON], [libdaemon])
CFLAGS="$CFLAGS -Wall -fno-strict-aliasing"

AC_MSG_CHECKING([TIPC netlink API])
AC_MSG_RESULT([$tipc_api])
if test "x$tipc_api" = "xnew"; then
	AC_DEFINE(TIPC_NEW_NL_API, 1, [using new tipc tool])
fi
AM_CONDITIONAL(TIPC_NEW_NL_API, test "x$tipc_api" = "xnew")

AC_CONFIG_FILES([
	Makefile
	tipc-config/Makefile
	tipc-pipe/Makefile
	tipc/Makefile
	ptts/Makefile
	demos/Makefile
	demos/benchmark/Makefile
	demos/connection_demo/Makefile
	demos/hello_world/Makefile
	demos/inventory_sim/Makefile
	demos/multicast_demo/Makefile
	demos/stream_demo/Makefile
	demos/topology_subscr_demo/Makefile
        man/Makefile
	scripts/Makefile
	tipclog/Makefile
	])
AM_CONDITIONAL(WITH_SCRIPTS, false)
AC_ARG_ENABLE(scripts,
        [  --enable-scripts  Install utility scripts in sysconfdir],
	 AM_CONDITIONAL(WITH_SCRIPTS, true))

AC_OUTPUT
