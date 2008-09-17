INCLUDES = -I$(srcdir)/../thumbs -I$(srcdir)/.. $(PKG_CFLAGS) \
	$(DBUS_CFLAGS) $(GLIB_CFLAGS) $(GMODULE_CFLAGS) $(GIO_CFLAGS)

bin_PROGRAMS = hildon-thumbnail-tester hildon-thumbnail-daemon-plugin-test

hildon_thumbnail_daemon_plugin_test_SOURCES = daemon.c daemon-glue.h
hildon_thumbnail_daemon_plugin_test_LDADD = $(top_builddir)/thumbs/libhildonthumbnail.la
	$(DBUS_LIBS) $(GLIB_LIBS) $(GMODULE_LIBS) $(GIO_LIBS)

hildon_thumbnail_tester_SOURCES = tests.c
hildon_thumbnail_tester_LDADD = $(top_builddir)/thumbs/libhildonthumbnail.la $(PKG_LIBS)

EXTRA_DIST = $(wildcard $(srcdir)/images/*)


%-glue.h: $(top_srcdir)/tests/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

BUILT_SOURCES = daemon-glue.h

EXTRA_DIST = $(BUILT_SOURCES) daemon.xml
