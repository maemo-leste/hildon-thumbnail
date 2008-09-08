INCLUDES = \
	-DLOCALEDIR=\"$(localedir)\" \
	-I$(top_srcdir) -I. \
	-I$(top_srcdir)/daemon \
	$(DBUS_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS)

# Library
lib_LTLIBRARIES = libhildonthumbnail.la

libhildonthumbnail_la_LIBADD = \
	$(top_builddir)/daemon/libshared.la \
	$(DBUS_LIBS) $(GLIB_LIBS) $(GIO_LIBS) \
	$(GDK_PIXBUF_LIBS)

libhildonthumbnail_la_LDFLAGS = -version-info 0:4:0
libhildonthumbnail_la_SOURCES = \
	hildon-thumbnail-factory.c hildon-thumbnail-factory.h \
	thumbnailer-client.h pixbuf-io-loader.c

%-client.h: $(top_srcdir)/daemon/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = thumbnailer-client.h

libhildonthumbnaildir=$(includedir)/hildon-thumbnail
libhildonthumbnail_HEADERS = hildon-thumbnail-factory.h hildon-thumber-common.h


