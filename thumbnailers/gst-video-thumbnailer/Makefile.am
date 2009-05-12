SUBDIRS = .

INCLUDES = \
	-I$(top_srcdir)/../thumbs \
	-I$(top_srcdir) \
	-I. \
	-I.. \
	-I$(top_srcdir)/.. \
	$(PKG_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(GIO_CFLAGS) \
	$(GSTREAMER_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) \
	$(PLAYBACK_CFLAGS)

BUILT_SOURCES = \
	gst-video-thumbnailer-glue.h \
        gst-video-thumbnailer-marshal.c \
	gst-video-thumbnailer-marshal.h

EXTRA_DIST = \
	gst-thumb-main.c \
	gst-thumb-thumber.c \
	gst-thumb-pipe.c \
	com.nokia.thumbnailer.Gstreamer.service.in \
	com.nokia.thumbnailer.Gstreamer.service

CLEANFILES = $(com.nokia.Thumbnailer_service_DATA) 

libexec_PROGRAMS = gst-video-thumbnailerd

# Marshall
gst-video-thumbnailer-marshal.h: gst-video-thumbnailer-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=gst_video_thumbnailer_marshal --header > $@

gst-video-thumbnailer-marshal.c: gst-video-thumbnailer-marshal.list
	$(GLIB_GENMARSHAL) $< --prefix=gst_video_thumbnailer_marshal --body > $@

# Services
com.nokia.thumbnailer.Gstreamer_servicedir = $(DBUS_SERVICES_DIR)
com.nokia.thumbnailer.Gstreamer_service_DATA = com.nokia.thumbnailer.Gstreamer.service

com.nokia.thumbnailer.Gstreamer_thumbnailersdir = /usr/share/thumbnailers
com.nokia.thumbnailer.Gstreamer_thumbnailers_DATA = reg/com.nokia.thumbnailer.Gstreamer.service

%.service: %.service.in
	sed -e "s|@libexecdir[@]|${libexecdir}|" $< > $@

%-glue.h: %.xml
	$(DBUSBINDINGTOOL) --mode=glib-server --output=$@ --prefix=$(subst -,_,$*) $^

gst_video_thumbnailerd_SOURCES = \
	gst-thumb-main.c \
	gst-thumb-main.h \
	gst-thumb-thumber.c \
	gst-thumb-thumber.h \
	gst-thumb-pipe.c \
	gst-thumb-pipe.h \
	gst-video-thumbnailer-marshal.c \
	gst-video-thumbnailer-glue.h

gst_video_thumbnailerd_LDADD = \
	$(DBUS_LIBS) \
	$(GLIB_LIBS) \
	$(GMODULE_LIBS) \
	$(GIO_LIBS) \
	$(GSTREAMER_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	$(PLAYBACK_LIBS)
