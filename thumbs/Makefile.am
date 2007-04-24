# TODO: remove THUMBER_DIR flag, use something better
# TODO: move common stuff to library?

CFLAGS += -Wall -std=c99 -pedantic '-DTHUMBER_DIR="$(shell pwd)"' '-DBIN_DIR="$(bindir)"'

INCLUDES = $(PKG_CFLAGS) -DLOCALEDIR=\"$(localedir)\" \
    -I$(srcdir)/..

# Binaries
bin_PROGRAMS = hildon-thumb-gdk-pixbuf hildon-thumber-register

hildon_thumb_gdk_pixbuf_SOURCES = thumber-gdk-pixbuf.c
hildon_thumb_gdk_pixbuf_LDADD = $(PKG_LIBS) libhildonthumbnail.la

hildon_thumber_register_SOURCES = thumber-register.c
hildon_thumber_register_LDADD = $(PKG_LIBS) libhildonthumbnail.la

# Library
lib_LTLIBRARIES = libhildonthumbnail.la

libhildonthumbnail_la_LIBADD = $(PKG_LIBS)
libhildonthumbnail_la_LDFLAGS = -version-info 0:3:0
libhildonthumbnail_la_SOURCES = \
    hildon-thumbnail-factory.c hildon-thumbnail-factory.h thumbs-common.c thumbs-private.h \
    thumber-common.c \
    md5.c md5.h

libhildonthumbnaildir=$(includedir)/hildon-thumbnail
libhildonthumbnail_HEADERS = hildon-thumbnail-factory.h hildon-thumber-common.h

install-exec-local:
	rm -f $(DESTDIR)$(bindir)/osso-thumb-gdk-pixbuf
	ln -s hildon-thumb-gdk-pixbuf $(DESTDIR)$(bindir)/osso-thumb-gdk-pixbuf
	rm -f $(DESTDIR)$(bindir)/osso-thumber-register
	ln -s hildon-thumber-register $(DESTDIR)$(bindir)/osso-thumber-register
