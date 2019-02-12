if gstreamer.found()
    gst_video_thumbnailerd_sources = [
        'gst-thumb-main.c',
        'gst-thumb-thumber.c',
        'gst-thumb-pipe.c',
        marshal_h_gen.process('gst-video-thumbnailer-marshal.list'),
        marshal_c_gen.process('gst-video-thumbnailer-marshal.list'),
        glue_gen.process('gst-video-thumbnailer.xml')
    ]

    executable('gst-video-thumbnailerd',
        sources: gst_video_thumbnailerd_sources,
        dependencies: [dbus, dbus_glib, glib, gmodule, gio, gstreamer, gdk_pixbuf, playback],
        include_directories: include_directories('../..'),
        install: true,
        install_dir: get_option('libexecdir')
    )
endif

# Similar construct in daemon/meson.build
service_conf_data = configuration_data({
    'libexecdir': join_paths(get_option('prefix'), get_option('libexecdir'))
})
configure_file(input : 'com.nokia.thumbnailer.Gstreamer.service.in',
               output : 'com.nokia.thumbnailer.Gstreamer.service',
               configuration : service_conf_data,
               install: true,
               install_dir: join_paths(get_option('datadir'), 'dbus-1', 'services'))

install_data('reg/com.nokia.thumbnailer.Gstreamer.service',
             install_dir: join_paths(get_option('datadir'), 'thumbnailers'))
