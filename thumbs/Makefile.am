generic_includes = \
	-DLOCALEDIR=\"$(localedir)\" \
	-I$(top_srcdir) -I. \
	-I$(top_srcdir)/daemon \
	$(DBUS_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS)

generic_libs = \
	$(DBUS_LIBS) \
	$(GLIB_LIBS) \
	$(GIO_LIBS)


bin_SCRIPTS = hildon-thumbnailer-wrap.sh

if ENABLE_OSSO
bin_PROGRAMS = hildon-thumb-gdk-pixbuf hildon-thumber-register

hildon_thumb_gdk_pixbuf_CFLAGS = $(generic_includes)  $(LIBOSSO_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS)
hildon_thumb_gdk_pixbuf_SOURCES = thumber-gdk-pixbuf.c thumber-common.c
hildon_thumb_gdk_pixbuf_LDADD = libhildonthumbnail.la $(generic_libs) \
	$(top_builddir)/daemon/libshared.la $(GDK_PIXBUF_LIBS) \
	$(LIBOSSO_LIBS)

hildon_thumber_register_CFLAGS = $(generic_includes) $(LIBOSSO_CFLAGS) \
	-DBIN_PATH=\"$(prefix)/bin\"
hildon_thumber_register_SOURCES = thumber-register.c
hildon_thumber_register_LDADD = libhildonthumbnail.la $(generic_libs) \
	$(top_builddir)/daemon/libshared.la $(LIBOSSO_LIBS)
endif

lib_LTLIBRARIES = libhildonthumbnail.la

libhildonthumbnail_la_CFLAGS = $(generic_includes) $(GDK_PIXBUF_CFLAGS)
libhildonthumbnail_la_LIBADD = $(generic_libs) $(GDK_PIXBUF_LIBS) \
	$(top_builddir)/daemon/libshared.la 
libhildonthumbnail_la_LDFLAGS = -version-info 0:4:0
libhildonthumbnail_la_SOURCES = hildon-thumbnail-factory.c \
	hildon-thumbnail-factory.h thumbnailer-client.h \
	pixbuf-io-loader.c hildon-albumart-factory.c \
	hildon-albumart-factory.h albumart-client.h \
	hildon-thumbnail-obj.c hildon-albumart-obj.c

%-client.h: $(top_srcdir)/daemon/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-client --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = thumbnailer-client.h albumart-client.h

libhildonthumbnaildir=$(includedir)/hildon-thumbnail
libhildonthumbnail_HEADERS = hildon-thumbnail-factory.h hildon-thumber-common.h \
	hildon-albumart-factory.h

install-exec-local:
	rm -f $(DESTDIR)$(bindir)/osso-thumb-gdk-pixbuf
	ln -s hildon-thumb-gdk-pixbuf $(DESTDIR)$(bindir)/osso-thumb-gdk-pixbuf
	rm -f $(DESTDIR)$(bindir)/osso-thumber-register
	ln -s hildon-thumber-register $(DESTDIR)$(bindir)/osso-thumber-register


