bin_PROGRAMS = sofprobeclient

noinst_HEADERS = probe_dma_frame.h probes_demux.h probes_wave.h

sofprobeclient_SOURCES = sofprobeclient.c probes_demux.c

sofprobeclient_CFLAGS = -I$(top_srcdir)/include

sofprobeclient_LDADD = $(top_builddir)/src/lib/libtinycompress.la
