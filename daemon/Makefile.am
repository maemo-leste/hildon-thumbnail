SUBDIRS = . plugins

INCLUDES = -I. \
	$(DBUS_CFLAGS) \
	$(GLIB_CFLAGS) $(GDK_PIXBUF_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(GIO_CFLAGS) $(HAL_CFLAGS) \
	$(LIBOSSO_CFLAGS) \
	-DTHUMBNAILERS_DIR=\""$(datadir)/thumbnailers"\" \
	-DALBUMARTERS_DIR=\""$(datadir)/albumart-providers"\" \
	-DPLUGINS_DIR=\""$(libdir)/hildon-thumbnailer/plugins"\" \
	-DOUTPUTPLUGINS_DIR=\""$(libdir)/hildon-thumbnailer/output-plugins"\" 

# Make some directories so that we have something to watch.  Without
# these directories existing, GIO file watching will poll instead of using
# inotify.
#
install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(datadir)/thumbnailers
	$(mkinstalldirs) $(DESTDIR)$(datadir)/albumart-providers

noinst_LTLIBRARIES = libshared.la

libhildonthumbnailplugindir=$(includedir)/hildon-thumbnail
libhildonthumbnailplugin_HEADERS = hildon-thumbnail-plugin.h

libshared_la_SOURCES = utils.h utils.c

libexec_PROGRAMS = hildon-thumbnailerd hildon-thumbnailer-plugin-runner

plugin_stuff = hildon-thumbnail-plugin.h hildon-thumbnail-plugin.c

thumbnailer-marshal.h: thumbnailer-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=thumbnailer_marshal --header > $@

thumbnailer-marshal.c: thumbnailer-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=thumbnailer_marshal --body > $@

albumart-marshal.h: albumart-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=albumart_marshal --header > $@

albumart-marshal.c: albumart-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=albumart_marshal --body > $@

hildon_thumbnailer_plugin_runner_SOURCES = \
	plugin-runner.c \
	$(plugin_stuff) \
	plugin-runner-glue.h

hildon_thumbnailer_plugin_runner_LDADD = \
        $(DBUS_LIBS) $(GLIB_LIBS) $(GMODULE_LIBS) $(GIO_LIBS)

hildon_thumbnailerd_SOURCES = \
	$(plugin_stuff) \
	hildon-thumbnail-daemon.c \
	thumbnailer.c \
	thumbnailer.h \
	thumbnailer-marshal.c \
	thumbnailer-marshal.h \
	thumbnail-manager.c \
	thumbnail-manager.h \
	manager-glue.h \
	thumbnailer-glue.h \
	dbus-utils.h \
	dbus-utils.c \
	albumart.c \
	albumart.h \
	thumb-hal.c \
	thumb-hal.h \
	albumart-marshal.c \
	albumart-marshal.h \
	albumart-manager.c \
	albumart-manager.h

hildon_thumbnailerd_LDADD = \
	$(top_builddir)/daemon/libshared.la \
	$(DBUS_LIBS) $(LIBOSSO_LIBS) \
	$(GLIB_LIBS) \
	$(GMODULE_LIBS) $(GDK_PIXBUF_LIBS) \
	$(GIO_LIBS) $(HAL_LIBS)

# Services
org.freedesktop.thumbnailer_servicedir = $(datadir)/dbus-1/services/
org.freedesktop.thumbnailer_service_DATA = org.freedesktop.thumbnailer.service

com.nokia.albumart_servicedir = $(datadir)/dbus-1/services/
com.nokia.albumart_service_DATA = com.nokia.albumart.service

%.service: %.service.in
	sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

%-glue.h: %.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = manager-glue.h thumbnailer-glue.h plugin-runner-glue.h \
	thumbnailer-marshal.c thumbnailer-marshal.h albumart-glue.h \
	albumart-marshal.c albumart-marshal.h

configdir = $(datadir)/hildon-thumbnail
config_DATA = manager.xml thumbnailer.xml albumart.xml

EXTRA_DIST = $(BUILT_SOURCES) $(config_DATA) \
        org.freedesktop.thumbnailer.service.in \
        com.nokia.albumart.service.in \
	thumbnailer-marshal.list \
	albumart-marshal.list

CLEANFILES = $(BUILT_SOURCES)				\
	$(org.freedesktop.thumbnailer_service_DATA)	\
	$(com.nokia.albumart_service_DATA)
