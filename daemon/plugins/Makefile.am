plugin_flags = -module -avoid-version -no-undefined
pluginsdir = $(libdir)/hildon-thumbnailer/plugins

INCLUDES = -I. -I$(top_srcdir)/daemon \
	$(PKG_CFLAGS) \
	-I$(top_srcdir)/thumbs \
	$(DBUS_CFLAGS) \
	$(GLIB_CFLAGS) 

plugins_LTLIBRARIES = libhildon-thumbnailer-default.la

%-glue.h: $(top_srcdir)/daemon/plugins/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = image-png-glue.h
CLEANFILES = $(BUILT_SOURCES)
configdir = $(datadir)/hildon-thumbnail
config_DATA = image-png.xml
EXTRA_DIST = $(BUILT_SOURCES) $(config_DATA)
 
libhildon_thumbnailer_default_la_SOURCES = default.c image-png-glue.h default.h
libhildon_thumbnailer_default_la_LDFLAGS = $(plugin_flags)
libhildon_thumbnailer_default_la_LIBADD = \
	$(PGK_LIBS) \
	$(top_builddir)/thumbs/libhildonthumbnail.la \
        $(GMODULE_LIBS) \
        $(GLIB_LIBS)




