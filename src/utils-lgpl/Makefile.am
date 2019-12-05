bin_PROGRAMS = fcplay

fcplay_SOURCES = fcplay.c

fcplay_CFLAGS = -I$(top_srcdir)/include \
		    $(AVCODEC_CFLAGS) \
		    $(AVFORMAT_CFLAGS) \
		    $(AVUTIL_CFLAGS) \
		    $(LIBDRM_CFLAGS)

fcplay_LDADD = $(top_builddir)/src/lib/libtinycompress.la \
		    $(AVCODEC_LIBS) \
		    $(AVFORMAT_LIBS) \
		    $(AVUTIL_LIBS) \
		    $(LIBDRM_LIBS)
