INCLUDES = -I$(srcdir)/../thumbs -I$(srcdir)/.. $(PKG_CFLAGS)

bin_PROGRAMS = hildon-thumbnail-tester

hildon_thumbnail_tester_SOURCES = tests.c
hildon_thumbnail_tester_LDADD = $(top_builddir)/thumbs/libhildonthumbnail.la $(PKG_LIBS)

EXTRA_DIST = $(wildcard $(srcdir)/images/*)
