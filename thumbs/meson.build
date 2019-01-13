generic_deps = [
    dbus,
    dbus_glib,
    glib,
    gio
]

generic_includes = [
    include_directories('..') # for config.h
]

hildonthumbnail_sources = [
    'hildon-thumbnail-factory.c',
    'hildon-thumbnail-factory.h',
    '../daemon/plugins/pixbuf-io-loader.c',
    'hildon-albumart-factory.c',
    'hildon-albumart-factory.h',
    'hildon-thumbnail-obj.c',
    'hildon-albumart-obj.c',
    client_gen.process('../daemon/thumbnailer.xml', '../daemon/albumart.xml'),
    marshal_h_gen.process('thumbnailer-marshal.list'),
    marshal_c_gen.process('thumbnailer-marshal.list')
]

libhildonthumbnail = library('hildonthumbnail',
        sources: hildonthumbnail_sources,
        dependencies: [generic_deps, gdk_pixbuf],
        link_with: [libshared]
)

if libosso.found()
    thumb_gdk_pixbuf_sources = [
        'thumber-gdk-pixbuf.c',
        'thumber-common.c'
    ]

    executable('hildon-thumb-gdk-pixbuf',
        sources: thumb_gdk_pixbuf_sources,
        dependencies: [generic_deps, gthread, gdk_pixbuf, libosso],
        link_with: [libshared, libhildonthumbnail],
        include_directories: generic_includes
    )

    thumber_register_sources = [
        'thumber-register.c'
    ]

    executable('hildon-thumber-register',
        sources: thumber_register_sources,
        dependencies: [generic_deps, libosso],
        link_with: [libshared, libhildonthumbnail],
        c_args: ['-DBIN_PATH="@0@/bin"'.format(get_option('prefix'))]
    )
endif
