/*
 * This file is part of osso-thumbnail package
 *
 * Copyright (C) 2005 Nokia Corporation.  All Rights reserved.
 *
 * Contact: Marius Vollmer <marius.vollmer@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include "osso-thumbnail-factory.h"
#include "osso-thumber-common.h"
#include "thumbs-private.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libgnomevfs/gnome-vfs.h>

GdkPixbuf* osso_thumber_create_empty_pixbuf()
{
    return create_empty_pixbuf();
}

int osso_thumber_main(
    int *argc_p, char ***argv_p,
    OssoThumberCreateThumb create_thumb
)
{
    int argc;
    char **argv;

    guint width, height;
    OssoThumbnailFlags flags;
    gchar *uri, *file, *mime_type, *local_file;
    //gchar *fail_file, *final_file;

    time_t mtime = 0;
    GdkPixbuf *pixbuf;
    GnomeVFSURI *vfs_uri;
    GError *error = NULL;
    int status = 0;

    //const char *meta;
    gchar **keys = NULL, **values = NULL;

    argc = *argc_p;
    argv = *argv_p;

    if(argc != 6+1) {
        printf("Usage: osso-thumb-gdk-pixbuf"
               " source_uri mime_type dest_file flags thumb_width thumb_height\n");

        g_warning("Thumber invalid arguments");
        return 2;
    }

    uri = argv[1];
    mime_type = argv[2];
    file = argv[3];
    flags = atoi(argv[4]);
    width = atoi(argv[5]);
    height = atoi(argv[6]);

    g_type_init();

    gnome_vfs_init();

    vfs_uri = gnome_vfs_uri_new(uri);
    if(!vfs_uri) {
        g_warning("Thumber failed to create URI from: %s", uri);
        return 4;
    }

    local_file = gnome_vfs_get_local_path_from_uri(uri);

    //if(gnome_vfs_uri_is_local(vfs_uri)) {
    if(local_file && strlen(local_file)) {
        /*
        if(!local_file || strlen(local_file) == 0) {
            g_warning("Failed to get local file for uri: %s", uri);
            return 4;
        }
        */
        mtime = get_file_mtime(local_file);
    } else {
        gchar *file_uri;
        GnomeVFSURI *src_uri, *dest_uri;

        // Reuse output file as temporary file
        file_uri = gnome_vfs_get_uri_from_local_path(file);

        src_uri = gnome_vfs_uri_new(uri);
        dest_uri = gnome_vfs_uri_new(file_uri);

        gnome_vfs_xfer_uri(src_uri, dest_uri, GNOME_VFS_XFER_DEFAULT,
                           GNOME_VFS_XFER_ERROR_MODE_ABORT,
                           GNOME_VFS_XFER_OVERWRITE_MODE_ABORT,
                           NULL, NULL);

        mtime = get_uri_mtime(uri);

        gnome_vfs_uri_unref(src_uri);
        gnome_vfs_uri_unref(dest_uri);

        g_free(file_uri);
    }

    gnome_vfs_uri_unref(vfs_uri);

    //g_message("thumber from %s to %s", local_file, file);

    pixbuf = create_thumb(local_file, mime_type,
        width, height, flags, &keys, &values, &error);

    if(!pixbuf) {
        g_warning("Thumbnail creation failed: %s", uri);

        pixbuf = create_empty_pixbuf();

        status = 10;
    }

    if(!save_thumb_file_meta(pixbuf, file, mtime, uri,
        (const gchar **) keys, (const gchar **) values))
        g_warning("Thumbnail save failed: %s", file);


    if(keys) g_strfreev(keys);
    if(values) g_strfreev(values);

    //g_message("Saved %s to %s", uri, final_file);

    gdk_pixbuf_unref(pixbuf);

    g_free(local_file);

    return status;
}
