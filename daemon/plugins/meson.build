pluginsdir = join_paths(get_option('libdir'), 'hildon-thumbnailer', 'plugins')
outputpluginsdir = join_paths(get_option('libdir'), 'hildon-thumbnailer', 'output-plugins')

# libhildon-thumbnailer-jpeg
jpeg_sources = [
    'gdkpixbuf-jpeg-out-plugin.c'
]

shared_module('hildon-thumbnailer-jpeg',
    sources: jpeg_sources,
    dependencies: [dbus, gmodule, glib, gdk_pixbuf],
    include_directories: daemon_includes,
    link_with: libshared,
    install: true,
    install_dir: outputpluginsdir
)

if png.found()
    # libhildon-thumbnailer-png
    png_sources = [
        'gdkpixbuf-png-out-plugin.c'
    ]

    shared_module('hildon-thumbnailer-png',
        sources: png_sources,
        dependencies: [dbus, gmodule, glib, gdk_pixbuf],
        include_directories: daemon_includes,
        link_with: libshared,
        install: true,
        install_dir: outputpluginsdir
    )
endif

# libhildon-thumbnailer-gdkpixbuf
gdkpixbuf_sources = [
    'gdkpixbuf-plugin.c',
    'pixbuf-io-loader.c'
]

shared_module('hildon-thumbnailer-gdkpixbuf',
    sources: gdkpixbuf_sources,
    dependencies: [dbus, gmodule, glib, gdk_pixbuf],
    include_directories: daemon_includes,
    link_with: libshared,
    install: true,
    install_dir: pluginsdir
)

# libhildon-thumbnailer-exec
exec_sources = [
    'exec-plugin.c'
]

shared_module('hildon-thumbnailer-exec',
    sources: exec_sources,
    dependencies: [dbus, gmodule, glib, gdk_pixbuf],
    include_directories: daemon_includes,
    link_with: libshared,
    install: true,
    install_dir: pluginsdir
)

if epeg.found()
    # libhildon-thumbnailer-epeg
    epeg_sources = [
        'epeg-plugin.c'
    ]

    shared_module('hildon-thumbnailer-epeg',
        sources: png_sources,
        dependencies: [dbus, gmodule, glib, gdk_pixbuf],
        include_directories: daemon_includes,
        link_with: libshared,
        install: true,
        install_dir: pluginsdir
    )
endif

# TODO: Maybe some libs have to be linked against sqlite3, etc?
