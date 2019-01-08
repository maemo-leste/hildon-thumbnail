project('hildon-thumbnail', 'c',
  version : '3.1.2',
  default_options : ['warning_level=3'])

glib = dependency('glib-2.0', version: '>=2.4.0') # TODO: What about gthread-2.0
xml = dependency('libxml-2.0')
dbus = dependency('dbus-1')
dbus_glib = dependency('dbus-glib-1')

dbusbindingtool = find_program('dbus-binding-tool')
dbusbindingtool_wrapper = find_program('dbusbindingtool_wrapper.py')
glib_genmarshal = glib.get_pkgconfig_variable('glib_genmarshal')

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
libexif = dependency('libexif', required: get_option('libexif'))

# TODO AC_CONFIG_LINKS(thumbs/pixbuf-io-loader.c:daemon/plugins/pixbuf-io-loader.c)

subdir('daemon')
subdir('thumbs')
subdir('thumbnailers')
subdir('tests')

pkg = import('pkgconfig')
#pkg.generate(libraries : libs,
#             subdirs : h,
#             description : 'LGPL-licensed thumbs and albumart library')
