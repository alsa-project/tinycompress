#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([tinycompress], [1.2.13])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE(1.10)
LT_INIT(disable-static)

AC_ARG_ENABLE(fcplay,
  AS_HELP_STRING([--enable-fcplay], [enable the fcplay component]),
  [build_fcplay="$enableval"], [build_fcplay="no"])
AC_ARG_ENABLE(pcm,
  AS_HELP_STRING([--enable-pcm], [enable PCM compress playback support(used for debugging)]),
  [enable_pcm="$enableval"], [enable_pcm="no"])

AM_CONDITIONAL([BUILD_FCPLAY], [test x$build_fcplay = xyes])
AM_CONDITIONAL([ENABLE_PCM], [test x$enable_pcm = xyes])

#if test "$build_fcplay" = "yes"; then
#  AC_DEFINE([BUILD_FCPLAY], "1", [Build Fcplay component])
#fi

if test "$enable_pcm" = "yes"; then
  AC_DEFINE([ENABLE_PCM], 1, [Enable PCM compress playback support (used for debugging)])
fi

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_PROG_CC_C_O
PKG_INSTALLDIR

# Checks for libraries.
AS_IF([test "x$build_fcplay" = "xyes"], [
  PKG_CHECK_MODULES([AVCODEC], [libavcodec >= 3.0.7])
  PKG_CHECK_MODULES([AVFORMAT], [libavformat >= 3.0.7])
  PKG_CHECK_MODULES([AVUTIL], [libavutil >= 3.0.7])
])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([
Makefile
include/Makefile
src/Makefile
src/lib/Makefile
src/utils/Makefile
src/utils-lgpl/Makefile
tinycompress.pc])
AC_OUTPUT
