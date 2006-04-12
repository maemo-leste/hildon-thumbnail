CFLAGS += -Wall -std=c99 -pedantic

INCLUDES = -I$(srcdir)/../thumbs -I$(srcdir)/.. $(PKG_CFLAGS)

bin_PROGRAMS = osso-thumbnail-tester

osso_thumbnail_tester_SOURCES = tests.c
osso_thumbnail_tester_LDADD = $(top_builddir)/thumbs/libossothumbnail.la $(PKG_LIBS)

EXTRA_DIST = $(wildcard $(srcdir)/images/*)
