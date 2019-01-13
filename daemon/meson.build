daemon_deps = [
    dbus,
    dbus_glib,
    glib,
    gdk_pixbuf,
    gmodule,
    gio,
    libosso
]

daemon_defines_templ = [
    '-DTHUMBNAILERS_DIR="@0@/thumbnailers"',
    '-DALBUMARTERS_DIR="@0@/albumart-providers"',
    '-DPLUGINS_DIR="@1@/hildon-thumbnailer/plugins"',
    '-DOUTPUTPLUGINS_DIR="@1@/hildon-thumbnailer/output-plugins"'
]
daemon_defines = []
foreach t : daemon_defines_templ
    daemon_defines += t.format(join_paths(get_option('prefix'), get_option('datadir')),
                               join_paths(get_option('prefix'), get_option('libdir')))
endforeach

daemon_includes = [
    include_directories('.'),
    include_directories('..') # for config.h
]

libshared_sources = [
    'utils.c'
]

libshared = library('shared',
    sources: libshared_sources,
    dependencies: daemon_deps
)

plugin_stuff = [
    'hildon-thumbnail-plugin.c'
]

plugin_runner_sources = [
    'plugin-runner.c',
    glue_gen.process('plugin-runner.xml'),
    plugin_stuff
]

executable('hildon_thumbnailer_plugin_runner',
    sources: plugin_runner_sources,
    dependencies: daemon_deps,
    include_directories: daemon_includes
)

hildon_thumbnailerd_sources = [
    plugin_stuff,
    'hildon-thumbnail-daemon.c',
    'thumbnailer.c',
    'thumbnailer.h',
    'thumbnail-manager.c',
    'thumbnail-manager.h',
    'dbus-utils.h',
    'dbus-utils.c',
    'albumart.c',
    'albumart.h',
    'thumb-hal.c',
    'thumb-hal.h',
    'albumart-manager.c',
    'albumart-manager.h',
    marshal_c_gen.process('thumbnailer-marshal.list', 'albumart-marshal.list'),
    marshal_h_gen.process('thumbnailer-marshal.list', 'albumart-marshal.list'),
    glue_gen.process('manager.xml', 'thumbnailer.xml', 'albumart.xml')
]

executable('hildon_thumbnailerd',
    sources: hildon_thumbnailerd_sources,
    dependencies: daemon_deps,
    include_directories: daemon_includes,
    c_args: daemon_defines,
    link_with: libshared
)

# TODO: Install .service files

subdir('plugins')
