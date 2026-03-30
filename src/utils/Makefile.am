bin_PROGRAMS = cplay crecord sofprobeclient

cplay_SOURCES = cplay.c wave.c
crecord_SOURCES = crecord.c wave.c
sofprobeclient_SOURCES = sofprobeclient.c wave.c

cplay_CFLAGS = -I$(top_srcdir)/include
crecord_CFLAGS = -I$(top_srcdir)/include
sofprobeclient_CFLAGS = -I$(top_srcdir)/include


cplay_LDADD = $(top_builddir)/src/lib/libtinycompress.la
crecord_LDADD = $(top_builddir)/src/lib/libtinycompress.la
sofprobeclient_LDADD = $(top_builddir)/src/lib/libtinycompress.la
