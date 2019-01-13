# libhildon-thumbnailer-jpeg
jpeg_sources = [
    'gdkpixbuf-jpeg-out-plugin.c'
]

shared_module('hildon_thumbnailer_jpeg',
    sources: jpeg_sources,
    dependencies: [dbus, gmodule, glib, gdk_pixbuf],
    include_directories: daemon_includes,
    link_with: libshared,
    c_args: ['-DHAVE_CONFIG_H'] # TODO Remove
)

if png.found()
    # libhildon-thumbnailer-png
    png_sources = [
        'gdkpixbuf-png-out-plugin.c'
    ]

    shared_module('hildon_thumbnailer_png',
        sources: png_sources,
        dependencies: [dbus, gmodule, glib, gdk_pixbuf],
        include_directories: daemon_includes,
        link_with: libshared,
        c_args: ['-DHAVE_CONFIG_H'] # TODO Remove
    )
endif

# libhildon-thumbnailer-gdkpixbuf
gdkpixbuf_sources = [
    'gdkpixbuf-plugin.c',
    'gdkpixbuf-plugin.h',
    'pixbuf-io-loader.c'
]

shared_module('hildon_thumbnailer_gdkpixbuf',
    sources: gdkpixbuf_sources,
    dependencies: [dbus, gmodule, glib, gdk_pixbuf],
    include_directories: daemon_includes,
    link_with: libshared
)

# libhildon-thumbnailer-exec
exec_sources = [
    'exec-plugin.c'
]

shared_module('hildon_thumbnailer_exec',
    sources: exec_sources,
    dependencies: [dbus, gmodule, glib, gdk_pixbuf],
    include_directories: daemon_includes,
    link_with: libshared,
    c_args: ['-DHAVE_CONFIG_H'] # TODO Remove
)

if epeg.found()
    # libhildon-thumbnailer-epeg
    epeg_sources = [
        'epeg-plugin.c'
    ]

    shared_module('hildon_thumbnailer_epeg',
        sources: png_sources,
        dependencies: [dbus, gmodule, glib, gdk_pixbuf],
        include_directories: daemon_includes,
        link_with: libshared,
        c_args: ['-DHAVE_CONFIG_H'] # TODO Remove
    )
endif
