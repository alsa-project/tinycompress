SUBDIRS=sofprobeclient

bin_PROGRAMS = cplay crecord ccaps

cplay_SOURCES = cplay.c wave.c
crecord_SOURCES = crecord.c wave.c
ccaps_SOURCES = ccaps.c

cplay_CFLAGS = -I$(top_srcdir)/include
crecord_CFLAGS = -I$(top_srcdir)/include
ccaps_CFLAGS = -I$(top_srcdir)/include


cplay_LDADD = $(top_builddir)/src/lib/libtinycompress.la
crecord_LDADD = $(top_builddir)/src/lib/libtinycompress.la
