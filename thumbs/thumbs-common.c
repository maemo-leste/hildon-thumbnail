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
#include "thumbs-private.h"

#include <sys/stat.h>

#include <glib/gprintf.h>
#include <libgnomevfs/gnome-vfs.h>

#include <string.h>

/** Return path to gconf key that contains thumber command in specified
    MIME type directory
*/
char *get_conf_cmd_path(const char *dirname)
{
    return g_strconcat(dirname, "/command", NULL);
}

/** Unquote mime type for gconf dirname */
void unquote_mime_dir(char *mime_type)
{
    gchar *at_pos;

    // Substitute first @ with /
    at_pos = strchr(mime_type, '@');
    if(at_pos) {
        *at_pos = '/';
    }
}

time_t get_uri_mtime(const gchar* uri)
{
    GnomeVFSFileInfo *info = gnome_vfs_file_info_new();
    time_t mtime;
    GnomeVFSResult result;

    result = gnome_vfs_get_file_info(uri, info, GNOME_VFS_FILE_INFO_DEFAULT |
            GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

    if(result == GNOME_VFS_OK) {
        mtime = info->mtime;
    } else {
        mtime = FALSE;
        g_warning("mtime get failed for uri: %s", uri);
    }

    gnome_vfs_file_info_unref(info);

    return mtime;
}

time_t get_file_mtime(const gchar *file)
{
    struct stat buf;

    if(stat(file, &buf)) {
        return 0;
    }

    return buf.st_mtime;
}

int get_file_size(const gchar *file)
{
    struct stat buf;

    if(stat(file, &buf)) {
        return 0;
    }

    return buf.st_size;
}

/* Get length of string array */
int str_arr_len(const char **arr)
{
    int i = 0;
    while(arr && *arr++) i++;
    return i;
}

/* Copy string array src to dest */
char **str_arr_copy(const char **src, char **dest)
{
    while(src && *src) {
        *dest++ = *(char **)src++;
    }

    return dest;
}

gboolean save_thumb_file_meta(GdkPixbuf *pixbuf, gchar *file, time_t mtime,
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
        OSSO_THUMBNAIL_APPLICATION "-" VERSION,
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

gboolean save_thumb_file(GdkPixbuf *pixbuf, gchar *file, time_t mtime,
    const gchar *uri)
{
    return save_thumb_file_meta(pixbuf, file, mtime, uri, NULL, NULL);
}

GdkPixbuf* create_empty_pixbuf()
{
    return gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 1, 1);
}

