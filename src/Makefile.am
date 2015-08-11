AM_CPPFLAGS = -I$(top_builddir)/gl -I$(top_srcdir)/gl -DLOCALEDIR=\"$(localedir)\"

LIBS += @LTLIBMULTITHREAD@ @LTLIBINTL@

bin_PROGRAMS = 13amp
13amp_SOURCES = 13amp.c 13amp_ops.c 13amp_log.c 13amp_util.c 13amp_conv.c
13amp_LDFLAGS = $(ZLIB_LDFLAGS) $(HTSLIB_LDFLAGS) $(FUSE_LDFLAGS) -static
13amp_CFLAGS = $(ZLIB_CFLAGS) $(HTSLIB_CFLAGS) $(FUSE_CFLAGS)
13amp_LDADD = $(top_builddir)/gl/lib13amp.la 

noinst_HEADERS = 13amp.h 13amp_ops.h 13amp_log.h 13amp_util.h 13amp_conv.h
