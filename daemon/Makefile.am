SUBDIRS = plugins

INCLUDES = \
	$(DBUS_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GMODULE_CFLAGS) \
        -DPLUGINS_DIR=\""$(libdir)/hildon-thumbnailer/plugins"\"


libexec_PROGRAMS = hildon-thumbnailerd

include_HEADERS = hildon-thumbnail-plugin.h

hildon_thumbnailerd_SOURCES = \
	$(include_HEADERS) \
	hildon-thumbnail-plugin.c \
	hildon-thumbnail-daemon.c 

hildon_thumbnailerd_LDADD = \
	$(DBUS_LIBS) \
	$(GLIB_LIBS) \
	$(GMODULE_LIBS)

# Services
org.freedesktop.Thumbnailer_servicedir = $(DBUS_SERVICES_DIR)
org.freedesktop.Thumbnailer_service_DATA = org.freedesktop.Thumbnailer.service

%.service: %.service.in
	@sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@


EXTRA_DIST = \
	$(org.freedesktop.Thumbnailer_service_DATA)


CLEANFILES =  \
	$(org.freedesktop.Thumbnailer_service_DATA)



