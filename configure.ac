AC_PREREQ([2.4])
AC_INIT([tipcutils], [2.1.1], [parthasarathy.bhuvaragan@ericsson.com])

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

# Checks for library functions.
PKG_CHECK_MODULES(LIBNL3, libnl-3.0)
PKG_CHECK_MODULES([LIBDAEMON], [libdaemon])
CFLAGS="$CFLAGS -Wall -fno-strict-aliasing"

AC_CHECK_TYPE(struct tipc_sioc_ln_req, [tipc_lss=yes],[], [[#include <linux/tipc.h>]])
AM_CONDITIONAL(TIPC_LINK_STATE_SUBSCRITION, test "x$tipc_lss" = "xyes")

AC_CONFIG_FILES([
	Makefile
	tipc-config/Makefile
	tipc-pipe/Makefile
	ptts/Makefile
	multicast_blast/Makefile
	demos/Makefile
	demos/benchmark/Makefile
	demos/connection_demo/Makefile
	demos/hello_world/Makefile
	demos/c_api_demo/Makefile
	demos/inventory_sim/Makefile
	demos/multicast_demo/Makefile
	demos/stream_demo/Makefile
	demos/topology_subscr_demo/Makefile
	demos/iov_control/Makefile
        man/Makefile
	scripts/Makefile
	tipclog/Makefile
	])
AM_CONDITIONAL(WITH_SCRIPTS, false)
AC_ARG_ENABLE(scripts,
        [  --enable-scripts  Install utility scripts in sysconfdir],
	 AM_CONDITIONAL(WITH_SCRIPTS, true))

AC_OUTPUT
