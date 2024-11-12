bin_PROGRAMS = cplay crecord caccel

cplay_SOURCES = cplay.c wave.c
crecord_SOURCES = crecord.c wave.c
caccel_SOURCES = caccel.c wave.c alsa_wrap.c

cplay_CFLAGS = -I$(top_srcdir)/include
crecord_CFLAGS = -I$(top_srcdir)/include
caccel_CFLAGS = -I$(top_srcdir)/include


cplay_LDADD = $(top_builddir)/src/lib/libtinycompress.la
crecord_LDADD = $(top_builddir)/src/lib/libtinycompress.la
caccel_LDADD = $(top_builddir)/src/lib/libtinycompress.la -lasound
