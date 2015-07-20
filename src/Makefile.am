AM_CPPFLAGS = -I$(top_builddir)/gl -I$(top_srcdir)/gl -DLOCALEDIR=\"$(localedir)\"

LIBS += @LTLIBMULTITHREAD@ @LTLIBINTL@

bin_PROGRAMS = cramp
cramp_SOURCES = 13amp.c 13amp_ops.c
cramp_LDFLAGS = $(ZLIB_LDFLAGS) $(HTSLIB_LDFLAGS) $(FUSE_LDFLAGS) -static
cramp_CFLAGS = $(ZLIB_CFLAGS) $(HTSLIB_CFLAGS) $(FUSE_CFLAGS)
cramp_LDADD = $(top_builddir)/gl/lib13amp.la 

noinst_HEADERS = 13amp.h 13amp_ops.h 
