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

#ifndef __THUMBER_COMMON_H__
#define __THUMBER_COMMON_H__

#include <gdk-pixbuf/gdk-pixbuf.h>

/**
 * OssoThumberCreateThumb:
 * @local_file: File to create thumbnail from
 * @width: Required thumbnail width
 * @height: Required thumbnail height
 * @flags: Flags passed to thumbnailer
 * @opt_keys: Pointer to variable in which to store gdk_pixbuf_savev keys array pointer
 *  The thumbnailer program will free the array and all strings in it.
 *  Use g_strdup if you have constant strings.
 * @opt_values: Pointer to variable in which to store gdk_pixbuf_savev values array pointer
 *  The thumbnailer program will free the array and all strings in it.
 *  Use g_strdup if you have constant strings.
 * @error: Set this if an error occurs
 *
 * Function called by the main function to create a thumbnail for the given file.
 * Returns: %NULL if thumbnail can't be created, pixbuf with thumbnail otherwise.
 */
typedef GdkPixbuf * (*OssoThumberCreateThumb)(const gchar *local_file, 
    const gchar *mime_type,
    guint width, guint height, OssoThumbnailFlags flags,
    gchar ***opt_keys, gchar ***opt_values, GError **error);

/**
 * osso_thumber_main:
 * @argc_p: Pointer to argc
 * @argv_p: Pointer to argv
 * @create_thumb: Function to create thumbnail for specified local file,
 *    with specified width and height
 *
 * Utility function used in thumbnailers. Usually called from thumbnailer main.
 * Passed a function that does the thumbnailing work. Error handling etc. is provided
 * automatically by this function
 */
int osso_thumber_main(
    int *argc_p, char ***argv_p, OssoThumberCreateThumb create_thumb
);

/**
 * osso_thumber_create_empty_pixbuf:
 *
 * Returns: An empty pixbuf for saving metadata only, eg. for MP3 files
 */
GdkPixbuf* osso_thumber_create_empty_pixbuf();

#endif
