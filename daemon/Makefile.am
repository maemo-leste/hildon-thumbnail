SUBDIRS = . plugins

INCLUDES = \
	$(DBUS_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(GIO_CFLAGS) \
	-DTHUMBNAILERS_DIR=\""/usr/share/thumbnailers"\" \
	-DPLUGINS_DIR=\""$(libdir)/hildon-thumbnailer/plugins"\"


noinst_LTLIBRARIES = libshared.la

libshared_la_SOURCES = md5.c md5.h utils.h utils.c

libexec_PROGRAMS = hildon-thumbnailerd hildon-thumbnailer-plugin-runner

plugin_stuff = hildon-thumbnail-plugin.h hildon-thumbnail-plugin.c

thumbnailer-marshal.h: thumbnailer-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=thumbnailer_marshal --header > $@

thumbnailer-marshal.c: thumbnailer-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=thumbnailer_marshal --body > $@

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
	manager.c \
	manager.h \
	manager-glue.h \
	thumbnailer-glue.h \
	dbus-utils.h \
	dbus-utils.c

hildon_thumbnailerd_LDADD = \
	$(top_builddir)/daemon/libshared.la \
	$(DBUS_LIBS) \
	$(GLIB_LIBS) \
	$(GMODULE_LIBS) \
	$(GIO_LIBS) 

# Services
org.freedesktop.thumbnailer_servicedir = $(DBUS_SERVICES_DIR)
org.freedesktop.thumbnailer_service_DATA = org.freedesktop.thumbnailer.service

%.service: %.service.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

%-glue.h: $(top_srcdir)/daemon/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = manager-glue.h thumbnailer-glue.h plugin-runner-glue.h \
	thumbnailer-marshal.c thumbnailer-marshal.h
configdir = $(datadir)/hildon-thumbnail
config_DATA = manager.xml thumbnailer.xml

EXTRA_DIST = $(BUILT_SOURCES) $(config_DATA) \
	$(org.freedesktop.thumbnailer_service_DATA) \
	thumbnailer-marshal.list


CLEANFILES = $(BUILT_SOURCES) \
	$(org.freedesktop.thumbnailer_service_DATA)



