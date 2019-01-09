# libhildon-thumbnailer-jpeg

if png.found()
    # libhildon-thumbnailer-png
endif

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

# libhildon-thumbnailer-gdkpixbuf
# libhildon-thumbnailer-exec

if epeg.found()
    # libhildon-thumbnailer-epeg
endif
