INCLUDES = -I$(srcdir)/../thumbs -I$(srcdir)/.. $(PKG_CFLAGS) \
	$(DBUS_CFLAGS) $(GLIB_CFLAGS) $(GMODULE_CFLAGS) $(GIO_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) $(GTK_CFLAGS) -ggdb -O0

BUILT_SOURCES = daemon-glue.h

bin_PROGRAMS = hildon-thumbnail-tester hildon-thumbnail-daemon-plugin-test $(instart)

if HAVE_MGTK
bin_PROGRAMS += artist-art-tester test-paths

artist_art_tester_SOURCES = artist-art-test.c
artist_art_tester_LDADD = $(top_builddir)/thumbs/libhildonthumbnail.la \
	$(GTK_LIBS) 
endif


hildon_thumbnail_daemon_plugin_test_SOURCES = daemon.c daemon-glue.h
hildon_thumbnail_daemon_plugin_test_LDADD = $(top_builddir)/thumbs/libhildonthumbnail.la \
	$(DBUS_LIBS) $(GLIB_LIBS) $(GMODULE_LIBS) $(GIO_LIBS)


test_paths_SOURCES = testpaths.c
test_paths_LDADD = $(top_builddir)/thumbs/libhildonthumbnail.la $(PKG_LIBS) \
	$(GDK_PIXBUF_LIBS)

hildon_thumbnail_tester_SOURCES = tests.c
hildon_thumbnail_tester_LDADD = $(top_builddir)/thumbs/libhildonthumbnail.la $(PKG_LIBS) \
	$(GDK_PIXBUF_LIBS)

%-glue.h: $(top_srcdir)/tests/%.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

EXTRA_DIST = $(BUILT_SOURCES) daemon.xml

CLEANFILES = $(BUILT_SOURCES)


