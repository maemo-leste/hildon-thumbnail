/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

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

#include "hildon-thumbnail-factory.h"
#include "hildon-thumber-common.h"
//#include "thumbs-private.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gprintf.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>


static GdkPixbuf* create_empty_pixbuf ()
{
    return gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 1, 1);
}

#define HILDON_THUMBNAIL_OPTION_PREFIX "tEXt::Thumb::"
#define HILDON_THUMBNAIL_APPLICATION "hildon-thumbnail"
#define URI_OPTION HILDON_THUMBNAIL_OPTION_PREFIX "URI"
#define MTIME_OPTION HILDON_THUMBNAIL_OPTION_PREFIX "MTime"
#define SOFTWARE_OPTION "tEXt::Software"

/* Get length of string array */
static int str_arr_len(const char **arr)
{
    int i = 0;
    while(arr && *arr++) i++;
    return i;
}

/* Copy string array src to dest */
static char **str_arr_copy(const char **src, char **dest)
{
    while(src && *src) {
        *dest++ = *(char **)src++;
    }

    return dest;
}


static gboolean save_thumb_file_meta(GdkPixbuf *pixbuf, gchar *file, time_t mtime,
    const gchar *uri, const gchar **opt_keys, const gchar **opt_values)
{
    GError *error = NULL;
    gboolean ret;

    char mtime_str[64];

    const char *default_keys[] = {
        URI_OPTION,
        MTIME_OPTION,
        SOFTWARE_OPTION,
        NULL
    };

    const char *default_values[] = {
        uri,
        mtime_str,
        HILDON_THUMBNAIL_APPLICATION "-" "4.0.0",
        NULL
    };

    // Start pointers and iterators
    char **keys, **ikeys;
    char **values, **ivalues;

    /* Append optional keys, values to default keys, values */
    keys = ikeys = g_new0(char *,
        str_arr_len(default_keys) + str_arr_len(opt_keys) + 1);
    values = ivalues = g_new0(char *,
        str_arr_len(default_values) + str_arr_len(opt_values) + 1);

    ikeys = str_arr_copy(default_keys, ikeys);
    ivalues = str_arr_copy(default_values, ivalues);

    ikeys = str_arr_copy(opt_keys, ikeys);
    ivalues = str_arr_copy(opt_values, ivalues);

    g_sprintf(mtime_str, "%lu", mtime);

    /*
    for(int i = 0; keys[i]; i++) {
        g_print("Saving %s: %s\n", keys[i], values[i]);
    }
    */

    ret = gdk_pixbuf_savev(pixbuf, file, "png", keys, values, &error);

    if(error) {
        g_warning("Error saving pixbuf: %s", error->message);
        g_clear_error(&error);
    }

    g_free(keys);
    g_free(values);

    return ret;
}



int hildon_thumber_main(
    int *argc_p, char ***argv_p,
    HildonThumberCreateThumb create_thumb
)
{
    int argc;
    char **argv;

    guint width, height;
    HildonThumbnailFlags flags;
    gchar *uri, *file, *mime_type, *local_file;
    gboolean suc = FALSE;

    time_t mtime = 0;
    GdkPixbuf *pixbuf;
    GFile *filei;
    GError *error = NULL;
    int status = 0;

    gchar **keys = NULL, **values = NULL;

    argc = *argc_p;
    argv = *argv_p;

    if(argc != 6+1) {
        printf("Usage: hildon-thumb-gdk-pixbuf"
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

    g_type_init ();
    g_thread_init (NULL);

    filei = g_file_new_for_uri (uri);
    if(!g_file_query_exists (filei, NULL)) {
        g_object_unref (filei);
        g_warning("Thumber failed to create URI from: %s", uri);
        return 4;
    }

    local_file = g_file_get_path (filei);


    if(local_file && strlen(local_file)) {
        GFileInfo *info = g_file_query_info (filei, G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                             G_FILE_QUERY_INFO_NONE,
                                             NULL, &error);
        if (!error) {
		suc = TRUE;
		mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
        }
	g_object_unref (info);
    }

    if (!suc) {
 
	GFile *filed = g_file_new_for_path (file);
	g_file_copy (filei, filed, G_FILE_COPY_NONE, NULL, NULL, NULL, &error);

	if (!error) {
 		GFileInfo *info = g_file_query_info (filed, G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                             G_FILE_QUERY_INFO_NONE,
                                             NULL, &error);
		mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
		g_object_unref (info);
		if (local_file)
		    g_free (local_file);
		local_file = g_file_get_path (filei);
	}
	g_object_unref (filed);

    }

    g_object_unref (filei);

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

    gdk_pixbuf_unref(pixbuf);

    g_free(local_file);

    return status;
}
