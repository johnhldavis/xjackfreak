#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.66])
AC_INIT([xjackfreak], [1.0], [dev@mirainet.org.uk])
AM_INIT_AUTOMAKE([-Wall foreign dist-bzip2])

AC_CONFIG_SRCDIR([src/xjackfreak1.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES([jack], [jack >= 0.116.1])
PKG_CHECK_MODULES([x11], [x11 >= 1.3.4])
PKG_CHECK_MODULES([xext], [xext >= 1.1.2])
AC_SEARCH_LIBS([pthread_create],[pthread],,[exit],)
AC_SEARCH_LIBS([rint],[m],,[exit],)
AC_SEARCH_LIBS([mq_open],[rt],,[exit],)

# Checks for header files.
AC_PATH_X
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/ioctl.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gettimeofday memset pow select sqrt strstr])

AC_CONFIG_FILES([Makefile
                 conf/Makefile
                 doc/Makefile
                 src/Makefile])
AC_OUTPUT
