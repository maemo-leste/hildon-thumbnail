SUBDIRS = .

INCLUDES = -I$(top_srcdir)/../thumbs -I$(top_srcdir) -I. -I.. -I$(top_srcdir)/.. $(PKG_CFLAGS) \
	$(DBUS_CFLAGS) $(GLIB_CFLAGS) $(GMODULE_CFLAGS) $(GIO_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS)

NULL = 

BUILT_SOURCES = my.vala.stamp                     \
                google-images-art-downloader.c    

EXTRA_DIST = my.vala.stamp                     \
             google-images-art-downloader.vala \
             com.nokia.albumart.GoogleImages.service.in \
             com.nokia.albumart.AlbumartProvider.GoogleImages.service

CLEANFILES = $(com.nokia.albumart.GoogleImages_service_DATA) 

libexec_PROGRAMS = hildon-google-images-art-downloaderd

hildon_google_images_art_downloaderd_VALASOURCES = \
	google-images-art-downloader.vala

hildon_google_images_art_downloaderd_SOURCES = \
	my.vala.stamp \
	$(hildon_google_images_art_downloaderd_VALASOURCES:.vala=.c) \
	$(NULL)

my.vala.stamp: $(hildon_google_images_art_downloaderd_VALASOURCES)
	$(VALAC) -C --basedir $(top_srcdir) \
	--pkg=gio-2.0 \
	--pkg=dbus-glib-1 \
	$^
	touch $@

hildon_google_images_art_downloaderd_LDADD = \
	$(DBUS_LIBS) $(GLIB_LIBS) $(GMODULE_LIBS) $(GIO_LIBS)

# Services
com.nokia.albumart.GoogleImages_servicedir = $(datadir)/dbus-1/services
com.nokia.albumart.GoogleImages_service_DATA = com.nokia.albumart.GoogleImages.service

albartdowndir = $(datadir)/albumart-providers
albartdown_DATA = com.nokia.albumart.AlbumartProvider.GoogleImages.service

%.service: %.service.in
	sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@
