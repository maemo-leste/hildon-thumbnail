# TODO: remove THUMBER_DIR flag, use something better
# TODO: move common stuff to library?

CFLAGS += -Wall -std=c99 -pedantic '-DTHUMBER_DIR="$(shell pwd)"' '-DBIN_DIR="$(bindir)"'

INCLUDES = $(PKG_CFLAGS) -DLOCALEDIR=\"$(localedir)\" \
    -I$(srcdir)/..

# Binaries
bin_PROGRAMS = osso-thumb-gdk-pixbuf osso-thumber-register
# osso-thumb-libid3

osso_thumb_gdk_pixbuf_SOURCES = thumber-gdk-pixbuf.c
osso_thumb_gdk_pixbuf_LDADD = $(PKG_LIBS) libossothumbnail.la

osso_thumber_register_SOURCES = thumber-register.c
osso_thumber_register_LDADD = $(PKG_LIBS) libossothumbnail.la

#osso_thumb_libid3_SOURCES = thumber-libid3.c thumber-common.c \
#    thumbs-common.c
#osso_thumb_libid3_LDADD = $(PKG_LIBS)  \
#    $(top_builddir)/libid3/lib/libid3.la

# Library
lib_LTLIBRARIES = libossothumbnail.la

libossothumbnail_la_LIBADD = $(PKG_LIBS)
libossothumbnail_la_LDFLAGS = -version-info 0:3:0
libossothumbnail_la_SOURCES = \
    osso-thumbnail-factory.c osso-thumbnail-factory.h thumbs-common.c thumbs-private.h \
    thumber-common.c \
    md5.c md5.h

libossothumbnaildir=$(includedir)/osso-thumbnail
libossothumbnail_HEADERS = osso-thumbnail-factory.h osso-thumber-common.h

#libossothumbnailincludeinstdir=$(includedir)/ossothumbnail
#libossothumbnailincludeinst_DATA = \
#        osso-thumbnail-factory.h
