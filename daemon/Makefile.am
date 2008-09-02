SUBDIRS = plugins

INCLUDES = \
	$(DBUS_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GMODULE_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GNOMEVFS_CFLAGS) \
	-DPLUGINS_DIR=\""$(libdir)/hildon-thumbnailer/plugins"\"


libexec_PROGRAMS = hildon-thumbnailerd

include_HEADERS = hildon-thumbnail-plugin.h

hildon_thumbnailerd_SOURCES = \
	$(include_HEADERS) \
	hildon-thumbnail-plugin.c \
	hildon-thumbnail-daemon.c \
	thumbnailer.c \
	thumbnailer.h \
	manager.c \
	manager.h \
	dbus-utils.h \
	dbus-utils.c

hildon_thumbnailerd_LDADD = \
	$(DBUS_LIBS) \
	$(GLIB_LIBS) \
	$(GMODULE_LIBS) \
	$(GIO_LIBS) \
	$(GNOMEVFS_LIBS)

# Services
org.freedesktop.thumbnailer_servicedir = $(DBUS_SERVICES_DIR)
org.freedesktop.thumbnailer_service_DATA = org.freedesktop.thumbnailer.service

%.service: %.service.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

%-glue.h: $(top_srcdir)/daemon/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = manager-glue.h thumbnailer-glue.h
configdir = $(datadir)/hildon-thumbnail
config_DATA = manager.xml thumbnailer.xml

EXTRA_DIST = $(BUILT_SOURCES) $(config_DATA) \
	$(org.freedesktop.thumbnailer_service_DATA)


CLEANFILES = $(BUILT_SOURCES) \
	$(org.freedesktop.thumbnailer_service_DATA)



