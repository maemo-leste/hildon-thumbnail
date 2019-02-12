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

executable('hildon-thumbnailer-plugin-runner',
    sources: plugin_runner_sources,
    dependencies: daemon_deps,
    include_directories: daemon_includes,
    install : true,
    install_dir : get_option('libexecdir')
)

hildon_thumbnailerd_sources = [
    plugin_stuff,
    'hildon-thumbnail-daemon.c',
    'thumbnailer.c',
    'thumbnail-manager.c',
    'dbus-utils.c',
    'albumart.c',
    'thumb-hal.c',
    'albumart-manager.c',
    marshal_c_gen.process('thumbnailer-marshal.list', 'albumart-marshal.list'),
    marshal_h_gen.process('thumbnailer-marshal.list', 'albumart-marshal.list'),
    glue_gen.process('manager.xml', 'thumbnailer.xml', 'albumart.xml')
]

executable('hildon-thumbnailerd',
    sources: hildon_thumbnailerd_sources,
    dependencies: daemon_deps,
    include_directories: daemon_includes,
    c_args: daemon_defines,
    link_with: libshared,
    install : true,
    install_dir : get_option('libexecdir')
)

# Install .service files
# Similar construct in thumbnailers/gst-video-thumbnailer/meson.build
service_conf_data = configuration_data({
    'libexecdir': join_paths(get_option('prefix'), get_option('libexecdir'))
})

service_files = [
    'org.freedesktop.thumbnailer.service',
    'com.nokia.albumart.service'
]

foreach service_file : service_files
    configure_file(input : service_file + '.in',
                   output : service_file,
                   configuration : service_conf_data,
                   install: true,
                   install_dir: join_paths(get_option('datadir'), 'dbus-1', 'services'))
endforeach

# Install .xml files
install_data('albumart.xml', 'manager.xml', 'thumbnailer.xml',
             install_dir: join_paths(get_option('datadir'), 'hildon-thumbnail'))
install_headers('hildon-thumbnail-plugin.h',
                subdir : 'hildon-thumbnail')

subdir('plugins')
