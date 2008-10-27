#ifndef __UTILS_H__
#define __UTILS_H__

/*
 * This file is part of hildon-thumbnail package
 *
 * Copyright (C) 2005 Nokia Corporation.  All Rights reserved.
 *
 * Contact: Marius Vollmer <marius.vollmer@nokia.com>
 * Author: Philip Van Hoof <philip@codeminded.be>
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

#include <glib.h>
#include <gio/gio.h>

void hildon_thumbnail_util_get_thumb_paths (const gchar *uri, gchar **large, gchar **normal, gchar **cropped, gchar **local_large, gchar **local_normal, gchar **local_cropped);
void hildon_thumbnail_util_get_albumart_path (const gchar *a, const gchar *b, const gchar *prefix, gchar **path);

#ifndef g_sprintf
gint g_sprintf (gchar *string, gchar const *format, ...);
#endif

#ifndef g_unlink
int g_unlink (const gchar *filename);
#endif

#endif
