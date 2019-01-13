if gstreamer.found()
    gst_video_thumbnailerd_sources = [
        'gst-thumb-main.c',
        'gst-thumb-main.h',
        'gst-thumb-thumber.c',
        'gst-thumb-thumber.h',
        'gst-thumb-pipe.c',
        'gst-thumb-pipe.h',
        marshal_h_gen.process('gst-video-thumbnailer-marshal.list'),
        marshal_c_gen.process('gst-video-thumbnailer-marshal.list'),
        glue_gen.process('gst-video-thumbnailer.xml')
    ]

    executable('gst_video_thumbnailerd',
        sources: gst_video_thumbnailerd_sources,
        dependencies: [dbus, dbus_glib, glib, gmodule, gio, gstreamer, gdk_pixbuf, playback],
        include_directories: include_directories('../..')
    )
endif
