daemon_deps = [
    dbus,
    dbus_glib,
    glib,
    gdk_pixbuf,
    gmodule,
    gio,
    libosso
]

daemon_includes = [
    include_directories('.')
]

glue_gen = generator(dbusbindingtool_wrapper,
                output  : '@BASENAME@-glue.h',
                arguments : ['--mode=glib-server',
                             '--output=@OUTPUT@',
                             # Replace dashes with underscores in @BASENAME@
                             '--prefix=' + '_'.join('@BASENAME@'.split('-')),
                             '@INPUT@'])

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
# TODO: Enable
#subdir('plugins')
