/*
 * This file is part of osso-thumbnail package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Luc Pionchon <luc.pionchon@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
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

#ifndef __LIBTHUMBS_PRIVATE_H__
#define __LIBTHUMBS_PRIVATE_H__

#include "config.h"

#include <unistd.h>
#include <sys/types.h>

#define DEFAULT_MAX_PROCESSES 1
#define THUMBS_GCONF_DIR "/apps/osso/osso/thumbnailers"

#define URI_OPTION OSSO_THUMBNAIL_OPTION_PREFIX "URI"
#define MTIME_OPTION OSSO_THUMBNAIL_OPTION_PREFIX "MTime"

#define MP3_OPTION OSSO_THUMBNAIL_OPTION_PREFIX "MP3"
#define MP3_NOIMAGE_OPTION OSSO_THUMBNAIL_OPTION_PREFIX "Noimage"
#define MP3_TITLE_OPTION OSSO_THUMBNAIL_OPTION_PREFIX "Title"
#define MP3_ARTIST_OPTION OSSO_THUMBNAIL_OPTION_PREFIX "Artist"
#define MP3_ALBUM_OPTION OSSO_THUMBNAIL_OPTION_PREFIX "Album"

#define SOFTWARE_OPTION "tEXt::Software"
#define META_OPTION OSSO_THUMBNAIL_OPTION_PREFIX "Meta"

char *get_conf_cmd_path(const char *dirname);
void unquote_mime_dir(char *mime_type);

time_t get_uri_mtime(const gchar* uri);
time_t get_file_mtime(const gchar *file);
int get_file_size(const gchar *file);

gboolean save_thumb_file(GdkPixbuf *pixbuf, gchar *file, time_t mtime, const gchar *uri);

gboolean save_thumb_file_meta(GdkPixbuf *pixbuf, gchar *file, time_t mtime,
    const gchar *uri, const gchar **opt_keys, const gchar **opt_values);

GdkPixbuf* create_empty_pixbuf();

#endif
