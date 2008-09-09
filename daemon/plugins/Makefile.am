plugin_flags = -module -avoid-version -no-undefined
pluginsdir = $(libdir)/hildon-thumbnailer/plugins

plugins_LTLIBRARIES = libhildon-thumbnailer-gdkpixbuf.la libhildon-thumbnailer-exec.la

libhildon_thumbnailer_gdkpixbuf_la_SOURCES = gdkpixbuf-plugin.c gdkpixbuf-plugin.h pixbuf-io-loader.c
libhildon_thumbnailer_gdkpixbuf_la_LDFLAGS = $(plugin_flags)
libhildon_thumbnailer_gdkpixbuf_la_CFLAGS = \
	-I. -I$(top_srcdir)/daemon \
	-I$(top_srcdir)/thumbs \
	$(DBUS_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS)
libhildon_thumbnailer_gdkpixbuf_la_LIBADD = \
	$(top_builddir)/daemon/libshared.la \
        $(GMODULE_LIBS) \
        $(GLIB_LIBS) \
	$(GDK_PIXBUF_LIBS)

libhildon_thumbnailer_exec_la_CFLAGS = \
	-I. -I$(top_srcdir)/daemon \
	-I$(top_srcdir)/thumbs \
	$(DBUS_CFLAGS) \
	$(GLIB_CFLAGS)
libhildon_thumbnailer_exec_la_SOURCES = exec-plugin.c exec-plugin.h
libhildon_thumbnailer_exec_la_LDFLAGS = $(plugin_flags)
libhildon_thumbnailer_exec_la_LIBADD = \
	$(top_builddir)/daemon/libshared.la \
        $(GMODULE_LIBS) \
        $(GLIB_LIBS)
