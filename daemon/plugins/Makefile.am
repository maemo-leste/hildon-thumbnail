plugin_flags = -module -avoid-version -no-undefined
pluginsdir = $(libdir)/hildon-thumbnailer/plugins

INCLUDES = -I. -I$(top_srcdir)/daemon \
	$(PKG_CFLAGS) \
	-I$(top_srcdir)/thumbs \
	$(DBUS_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS)

plugins_LTLIBRARIES = libhildon-thumbnailer-default.la

libhildon_thumbnailer_default_la_SOURCES = default.c default.h
libhildon_thumbnailer_default_la_LDFLAGS = $(plugin_flags)
libhildon_thumbnailer_default_la_LIBADD = \
	$(PGK_LIBS) \
	$(top_builddir)/daemon/libshared.la \
        $(GMODULE_LIBS) \
        $(GLIB_LIBS) \
	$(GDK_PIXBUF_LIBS)




