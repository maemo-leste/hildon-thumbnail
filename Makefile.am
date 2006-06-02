SUBDIRS = thumbs tests

EXTRA_DIST = debian/changelog debian/control debian/copyright debian/rules \
        debian/osso-thumbnail.install \
        debian/osso-thumbnail0.install \
        debian/osso-thumbnail0-dbg.install \
        debian/osso-thumbnail-dev.install \
        osso-thumbnail.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = osso-thumbnail.pc

# ... Hack ...
# --trace-children=yes
test: tests tests/osso-thumbnail-tester
# -rm -r ~/.thumbnails/
	LD_LIBRARY_PATH="$LD_LIBRARY_PATH:thumbs/.libs" \
	valgrind --leak-check=yes --db-attach=yes --num-callers=10 -q tests/.libs/osso-thumbnail-tester
#  --suppressions=tests/suppress.txt
