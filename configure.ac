AC_INIT(Makefile.am)
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE(hildon-thumbnail, 3.0.3)
AM_CONFIG_HEADER(config.h)

AC_PROG_CC
AM_PROG_CC_C_O

AC_CANONICAL_HOST

AC_PROG_INSTALL
AC_PROG_LIBTOOL

AC_HEADER_STDC

PKG_CHECK_MODULES(XML, libxml-2.0)
AC_SUBST(XML_CFLAGS)
AC_SUBST(XML_LIBS)

PKG_CHECK_MODULES(DBUS, dbus-1 dbus-glib-1)
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

# Check we have the DBUS binding tool we need
AC_PATH_PROG(DBUSBINDINGTOOL, dbus-binding-tool)
if test -z $DBUSBINDINGTOOL; then
   AC_MSG_ERROR([Could not find 'dbus-binding-tool'])
fi

GLIB_GENMARSHAL=`$PKG_CONFIG glib-2.0 --variable=glib_genmarshal`
AC_SUBST(GLIB_GENMARSHAL)

# Check for gmodule 2.0
PKG_CHECK_MODULES(GMODULE, gmodule-2.0)
AC_SUBST(GMODULE_CFLAGS)
AC_SUBST(GMODULE_LIBS)

PKG_CHECK_MODULES(GTK, gtk+-2.0, [have_gtk=yes], [have_gtk=no])
AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

AM_CONDITIONAL(HAVE_MGTK, test "$have_gtk" = "yes")

####################################################################
# DBus Service
####################################################################

dnl DBus services dir
AC_ARG_WITH([session_bus_services_dir],
            AS_HELP_STRING([--with-session-bus-services-dir],
                           [Path to DBus services directory]))

if test "x$with_session_bus_services_dir" = "x" ; then
   services_dir="`pkg-config --variable session_bus_services_dir dbus-1`"
else
   services_dir="$with_session_bus_services_dir"
fi

DBUS_SERVICES_DIR="$services_dir"
AC_SUBST(DBUS_SERVICES_DIR)


PKG_CHECK_MODULES(GIO, gio-2.0)
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

GLIB_CHECK="glib-2.0 >= 2.4.0 gthread-2.0"
PKG_CHECK_MODULES(GLIB, $GLIB_CHECK)
AC_SUBST(GLIB_LIBS)
AC_SUBST(GLIB_CFLAGS)


GDK_PIXBUF_CHECK="gdk-pixbuf-2.0 >= 2.4.0"
PKG_CHECK_MODULES(GDK_PIXBUF, $GDK_PIXBUF_CHECK)
AC_SUBST(GDK_PIXBUF_LIBS)
AC_SUBST(GDK_PIXBUF_CFLAGS)

AC_ARG_ENABLE(gstreamer, 
	      AS_HELP_STRING([--disable-gstreamer],
			     [Disable gstreamer based thumbnailers]),,
	      [enable_gstreamer=yes])

if test "x$enable_gstreamer" = "xyes"; then
   PKG_CHECK_MODULES(GSTREAMER,[gstreamer-0.10 >= 0.10.0],
   		[have_gstreamer=yes],
		[have_gstreamer=no])
   AC_SUBST(GSTREAMER_CFLAGS)
   AC_SUBST(GSTREAMER_LIBS)
fi

AM_CONDITIONAL(HAVE_GSTREAMER, test "$have_gstreamer" = "yes")

##################################################
# Check for gtk-doc.
##################################################

AC_ARG_WITH(html-dir, [  --with-html-dir=PATH    path to installed docs ])

if test "x$with_html_dir" = "x" ; then
  HTML_DIR='${datadir}/gtk-doc/html'
else
  HTML_DIR=$with_html_dir
fi

AC_CHECK_PROG(GTKDOC, gtkdoc-mkdb, true, false)

AC_PATH_PROG(VALAC, valac, valac)

gtk_doc_min_version=0.10
if $GTKDOC ; then
    gtk_doc_version=`gtkdoc-mkdb --version`
    AC_MSG_CHECKING([gtk-doc version ($gtk_doc_version) >= $gtk_doc_min_version]
)
    if perl <<EOF ; then
      exit (("$gtk_doc_version" =~ /^[[0-9]]+\.[[0-9]]+$/) &&
            ("$gtk_doc_version" >= "$gtk_doc_min_version") ? 0 : 1);
EOF
      AC_MSG_RESULT(yes)
   else
      AC_MSG_RESULT(no)
      GTKDOC=false
   fi
fi

dnl Let people disable the gtk-doc stuff.
AC_ARG_ENABLE(gtk-doc, [  --enable-gtk-doc        use gtk-doc to build documenta
tion [default=no]], enable_gtk_doc="$enableval", enable_gtk_doc=no)

if test x$enable_gtk_doc = xauto ; then
  if test x$GTKDOC = xtrue ; then
    enable_gtk_doc=yes
  else
    enable_gtk_doc=no
  fi
fi

AM_CONDITIONAL(ENABLE_GTK_DOC, test x$enable_gtk_doc = xyes)

localedir=${datadir}/locale

AC_ARG_ENABLE(doc-dir, [ --with-doc-dir=PATH path to installed docs ])
if test "x$with_doc_dir" = "x" ; then
  docdir='${datadir}/doc'
else
  docdir=$with_doc_dir 
fi

HTML_DIR=${docdir}/html

AC_SUBST(docdir)
AC_SUBST(localedir)
#AC_SUBST(outomoduledir)
AC_SUBST(HTML_DIR)


LIBOSSO_CHECK="libosso >= 0.9.19"
PKG_CHECK_MODULES(LIBOSSO, $LIBOSSO_CHECK, [have_osso=yes], [have_osso=no])
AC_SUBST(LIBOSSO_LIBS)
AC_SUBST(LIBOSSO_CFLAGS)

AM_CONDITIONAL(ENABLE_OSSO, test x$have_osso = xyes)


# --- Output ---

AC_CONFIG_LINKS(thumbs/pixbuf-io-loader.c:daemon/plugins/pixbuf-io-loader.c)

AC_OUTPUT(Makefile \
	  doc/Makefile \
	  tests/Makefile \
	  daemon/Makefile \
	  daemon/plugins/Makefile \
	  thumbs/Makefile \
          albumart-providers/Makefile \
          albumart-providers/googleimages/Makefile \
          hildon-thumbnail.pc)
