project('hildon-thumbnail', 'c',
  version : '3.1.2',
  meson_version : '>=0.47.0', # "feature" options
  default_options : ['warning_level=3'])

glib = dependency('glib-2.0', version: '>=2.4.0') # TODO: What about gthread-2.0
xml = dependency('libxml-2.0')
dbus = dependency('dbus-1')
dbus_glib = dependency('dbus-glib-1')

dbusbindingtool = find_program('dbus-binding-tool')
glib_genmarshal = find_program(glib.get_pkgconfig_variable('glib_genmarshal'))
prefix_wrapper = find_program('generator_prefix_wrapper.py')

gmodule = dependency('gmodule-2.0')
playback = dependency('libplayback-1', required: get_option('libplayback'))

gtk = dependency('gtk+-2.0', required: false)
gio = dependency('gio-2.0')
gdk_pixbuf = dependency('gdk-pixbuf-2.0', version: '>=2.4.0')
gstreamer = dependency('gstreamer-1.0', required : get_option('gstreamer'))
png = dependency('libpng', version: '>=1.2', required: get_option('libpng'))
epeg = dependency('epeg', version: '>=0.9.0', required: get_option('epeg'))
sqlite3 = dependency('sqlite3', required: get_option('sqlite3'))

# TODO: gtk-doc Docs stuff

libosso = dependency('libosso', required: get_option('libosso'))
libexif = dependency('libexif', version: '>=0.6', required: get_option('libexif'))

# TODO AC_CONFIG_LINKS(thumbs/pixbuf-io-loader.c:daemon/plugins/pixbuf-io-loader.c)

# Generate config.h
compiler = meson.get_compiler('c')
conf_data = configuration_data()
conf_data.set('HAVE_LIBEXIF', libexif.found())
conf_data.set('HAVE_MALLOC_H', compiler.has_header('malloc.h'))
conf_data.set('HAVE_MALLOPT', compiler.has_function('mallopt', prefix : '#include <malloc.h>'))
conf_data.set('HAVE_OSSO', libosso.found())
conf_data.set('HAVE_PLAYBACK', playback.found())
conf_data.set('HAVE_SQLITE3', sqlite3.found())
conf_data.set_quoted('PACKAGE_NAME', meson.project_name())
conf_data.set_quoted('VERSION', meson.project_version())
configure_file(output : 'config.h',
               configuration : conf_data)

subdir('daemon')
subdir('thumbs')
subdir('thumbnailers')
subdir('tests')

pkg = import('pkgconfig')
#pkg.generate(libraries : libs,
#             subdirs : h,
#             description : 'LGPL-licensed thumbs and albumart library')
