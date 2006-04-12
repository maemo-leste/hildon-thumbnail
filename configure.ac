AC_INIT(Makefile.am)
AC_CONFIG_AUX_DIR(config)
AM_INIT_AUTOMAKE(osso-thumbnail, 0.4)
AM_CONFIG_HEADER(config.h)

AC_CANONICAL_HOST

# --- Programs ---
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LIBTOOL

AC_HEADER_STDC

# --- Conf ---
CFLAGS="$CFLAGS -DOSSOLOG_COMPILE -Wall"
AC_SUBST(CFLAGS)

# --- Libraries ---

GLIB_CHECK="glib-2.0 >= 2.4.0"
PKG_CHECK_MODULES(GLIB, $GLIB_CHECK)
AC_SUBST(GLIB_LIBS)
AC_SUBST(GLIB_CFLAGS)

GNOMEVFS_CHECK="gnome-vfs-2.0 >= 2.4.0"
PKG_CHECK_MODULES(GNOMEVFS, $GNOMEVFS_CHECK)
AC_SUBST(GNOMEVFS_LIBS)
AC_SUBST(GNOMEVFS_CFLAGS)

GDK_PIXBUF_CHECK="gdk-pixbuf-2.0 >= 2.4.0"
PKG_CHECK_MODULES(GDK_PIXBUF, $GDK_PIXBUF_CHECK)
AC_SUBST(GDK_PIXBUF_LIBS)
AC_SUBST(GDK_PIXBUF_CFLAGS)

GCONF_CHECK="gconf-2.0 >= 2.4.0"
PKG_CHECK_MODULES(GCONF, $GCONF_CHECK)
AC_SUBST(GCONF_LIBS)
AC_SUBST(GCONF_CFLAGS)

LIBOSSO_CHECK="libosso >= 0.9.19"
PKG_CHECK_MODULES(LIBOSSO, $LIBOSSO_CHECK)
AC_SUBST(LIBOSSO_LIBS)
AC_SUBST(LIBOSSO_CFLAGS)

#GTK_CHECK="gtk+-2.0 >= 2.4.4"
#PKG_CHECK_MODULES(GTK, $GTK_CHECK)
#AC_SUBST(GTK_LIBS)
#AC_SUBST(GTK_CFLAGS)

PKG_CHECK="$GLIB_CHECK, $GNOMEVFS_CHECK, $GDK_PIXBUF_CHECK, $GCONF_CHECK, $LIBOSSO_CHECK"
PKG_LIBS="$GLIB_LIBS $GNOMEVFS_LIBS $GDK_PIXBUF_LIBS $GCONF_LIBS $LIBOSSO_LIBS"
PKG_CFLAGS="$GLIB_CFLAGS $GNOMEVFS_CFLAGS $GDK_PIXBUF_CFLAGS $GCONF_CFLAGS $LIBOSSO_CFLAGS"

AC_SUBST(PKG_CHECK)
AC_SUBST(PKG_LIBS)
AC_SUBST(PKG_CFLAGS)

AC_ARG_WITH(max-image-size, [  --with-max-image-size=width,height Maximum source image size to thumbnail ])

# Dramatically increased the maxmimum size, we'll let oom-killer
# to kill thumbnailer if it runs out of memory...
if test "x$with_max_image_size" = "x" ; then
  MAX_SRC_WIDTH=2048
  MAX_SRC_HEIGHT=1536
else
  MAX_SRC_WIDTH=`echo "$with_max_image_size" | sed 's/,.*$//' | grep '^[[0-9]]\+$'`
  MAX_SRC_HEIGHT=`echo "$with_max_image_size" | sed 's/^.*,//' | grep '^[[0-9]]\+$'`
  if test "x$MAX_SRC_WIDTH" = "x" ||
     test "x$MAX_SRC_HEIGHT" = "x" ; then
    AC_ERROR([Invalid size parameter given to --with-max-image-size])
  fi
fi
AC_DEFINE_UNQUOTED(MAX_SRC_WIDTH, $MAX_SRC_WIDTH, [Maximum source image width])
AC_DEFINE_UNQUOTED(MAX_SRC_HEIGHT, $MAX_SRC_HEIGHT, [Maximum source image height])

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
tion [default=yes]], enable_gtk_doc="$enableval", enable_gtk_doc=yes)

if test x$enable_gtk_doc = xauto ; then
  if test x$GTKDOC = xtrue ; then
    enable_gtk_doc=yes
  else
    enable_gtk_doc=no
  fi
fi

AM_CONDITIONAL(ENABLE_GTK_DOC, test x$enable_gtk_doc = xyes)

localedir=${datadir}/locale
#outomoduledir=${libdir}/outo

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

# --- Output ---

AC_OUTPUT(Makefile \
          tests/Makefile \
          thumbs/Makefile \
	  doc/Makefile \
          osso-thumbnail.pc)
