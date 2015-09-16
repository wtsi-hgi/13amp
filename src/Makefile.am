AM_CPPFLAGS = -I$(top_builddir)/gl -I$(top_srcdir)/gl -DLOCALEDIR=\"$(localedir)\"

LIBS += @LTLIBMULTITHREAD@ @LTLIBINTL@

bin_PROGRAMS = 13amp
13amp_SOURCES = 13amp.c fs.c log.c util.c conv.c cache.c
13amp_LDFLAGS = $(ZLIB_LDFLAGS) $(HTSLIB_LDFLAGS) $(FUSE_LDFLAGS) -static
13amp_CFLAGS = $(ZLIB_CFLAGS) $(HTSLIB_CFLAGS) $(FUSE_CFLAGS)
13amp_LDADD = $(top_builddir)/gl/lib13amp.la 

noinst_HEADERS = 13amp.h fs.h log.h util.h conv.h cache.h
