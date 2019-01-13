thumbs_include = include_directories('../thumbs')

thumbnail_tester_sources = [
    'tests.c'
]

e = executable('hildon-thumbnail-tester',
    sources: thumbnail_tester_sources,
    dependencies: [gdk_pixbuf],
    link_with: libhildonthumbnail,
    include_directories: thumbs_include
)
test('hildon thumbnail tester', e, workdir: meson.source_root())

thumbnail_daemon_plugin_test_sources = [
    'daemon.c',
    glue_gen.process('daemon.xml')
]

e = executable('hildon-thumbnail-daemon-plugin-test',
    sources: thumbnail_daemon_plugin_test_sources,
    dependencies: [dbus, dbus_glib, glib, gmodule, gio],
    link_with: libhildonthumbnail
)
test('hildon thumbnail daemon plugin test', e)

if gtk.found()
    artist_art_tester_sources = [
        'artist-art-test.c'
    ]

    e = executable('artist-art-tester',
        sources: artist_art_tester_sources,
        dependencies: gtk,
        link_with: libhildonthumbnail,
        include_directories: thumbs_include
    )
    test('artist art tester', e)

    test_paths_sources = [
        'testpaths.c'
    ]

    e = executable('test-paths',
        sources: test_paths_sources,
        dependencies: gdk_pixbuf,
        link_with: libhildonthumbnail,
        include_directories: thumbs_include
    )
    test('test paths', e)
endif
