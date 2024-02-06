# SPDX-License-Identifier: (LGPL-2.1-only OR BSD-3-Clause)

bin_PROGRAMS = cplay crecord

cplay_SOURCES = cplay.c wave.c
crecord_SOURCES = crecord.c wave.c

cplay_CFLAGS = -I$(top_srcdir)/include
crecord_CFLAGS = -I$(top_srcdir)/include


cplay_LDADD = $(top_builddir)/src/lib/libtinycompress.la
crecord_LDADD = $(top_builddir)/src/lib/libtinycompress.la
