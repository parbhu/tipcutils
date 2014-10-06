AC_PREREQ([2.4])
AC_INIT([tipcutils], [2.1.0], [richard.alpe@ericsson.com])

AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL

# Checks for header files.
AC_CHECK_HEADERS([linux/tipc.h])

# Checks for library functions.
PKG_CHECK_MODULES(LIBNL3, libnl-3.0)

CFLAGS="$CFLAGS -Wall -fno-strict-aliasing"

AC_MSG_CHECKING([TIPC netlink API])
AC_COMPILE_IFELSE([
	AC_LANG_PROGRAM([[
		#include <linux/tipc_config.h>
		]],[[

		if (TIPC_NL_UNSPEC == 0)
			return 0;

		return -1;
		]])],
               [tipc_api=new],
               [tipc_api=old])
AC_MSG_RESULT([$tipc_api])
if test "x$tipc_api" = "xnew"; then
	AC_DEFINE(TIPC_NEW_NL_API, 1, [using new tipc tool])
fi
AM_CONDITIONAL(TIPC_NEW_NL_API, test "x$tipc_api" = "xnew")

AC_CONFIG_FILES([
	Makefile
	tipc-config/Makefile
	tipc-pipe/Makefile
	tipc-config-wrapper/Makefile
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
	])
AM_CONDITIONAL(WITH_SCRIPTS, false)
AC_ARG_ENABLE(scripts,
        [  --enable-scripts  Install utility scripts in sysconfdir],
	 AM_CONDITIONAL(WITH_SCRIPTS, true))

AC_OUTPUT
