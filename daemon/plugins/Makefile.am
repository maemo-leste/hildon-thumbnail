plugin_flags = -module -avoid-version -no-undefined
pluginsdir = $(libdir)/hildon-thumbnailer/plugins

outputpluginsdir = $(libdir)/hildon-thumbnailer/output-plugins

outputplugins_LTLIBRARIES = libhildon-thumbnailer-jpeg.la

if HAVE_PNG
outputplugins_LTLIBRARIES += libhildon-thumbnailer-png.la
endif

plugins_LTLIBRARIES = libhildon-thumbnailer-gdkpixbuf.la \
	              libhildon-thumbnailer-exec.la

if HAVE_EPEG
plugins_LTLIBRARIES += libhildon-thumbnailer-epeg.la
endif

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

libhildon_thumbnailer_jpeg_la_SOURCES = gdkpixbuf-jpeg-out-plugin.c
libhildon_thumbnailer_jpeg_la_LDFLAGS = $(plugin_flags)
libhildon_thumbnailer_jpeg_la_CFLAGS = $(libhildon_thumbnailer_gdkpixbuf_la_CFLAGS) \
	$(SQLITE3_CFLAGS)
libhildon_thumbnailer_jpeg_la_LIBADD = $(libhildon_thumbnailer_gdkpixbuf_la_LIBADD) \
	$(SQLITE3_LIBS)

libhildon_thumbnailer_png_la_SOURCES = gdkpixbuf-png-out-plugin.c
libhildon_thumbnailer_png_la_LDFLAGS = $(plugin_flags)
libhildon_thumbnailer_png_la_CFLAGS = $(libhildon_thumbnailer_gdkpixbuf_la_CFLAGS) \
	$(PNG_CFLAGS)
libhildon_thumbnailer_png_la_LIBADD = $(libhildon_thumbnailer_gdkpixbuf_la_LIBADD) \
	$(PNG_LIBS)

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

libhildon_thumbnailer_epeg_la_SOURCES = epeg-plugin.c epeg-plugin.h
libhildon_thumbnailer_epeg_la_LDFLAGS = $(plugin_flags)
libhildon_thumbnailer_epeg_la_CFLAGS = \
	-I. -I$(top_srcdir)/daemon \
	-I$(top_srcdir)/thumbs \
	$(DBUS_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(EPEG_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS)
libhildon_thumbnailer_epeg_la_LIBADD = \
        $(top_builddir)/daemon/libshared.la \
        $(GMODULE_LIBS) \
        $(GLIB_LIBS) \
	$(EPEG_LIBS) \
	$(GDK_PIXBUF_LIBS)

