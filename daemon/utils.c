/*
 * This file is part of hildon-thumbnail package
 *
 * Copyright (C) 2005 Nokia Corporation.  All Rights reserved.
 *
 * Contact: Marius Vollmer <marius.vollmer@nokia.com>
 * Author: Philip Van Hoof <pvanhoof@gnome.org>
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

#include <string.h>
#include "utils.h"


void
hildon_thumbnail_util_get_thumb_paths (const gchar *uri, gchar **large, gchar **normal, gchar **cropped)
{
	gchar *ascii_digest;
	gchar *thumb_filename;
	gchar *cropped_filename;

	static gchar *large_dir = NULL;
	static gchar *normal_dir = NULL;
	static gchar *cropped_dir = NULL;

	/* I know we leak, but it's better than doing memory fragementation on 
	 * these strings ... */

	if (!large_dir)
		large_dir = g_build_filename (g_get_home_dir (), ".thumbnails", "large", NULL);

	if (!normal_dir)
		normal_dir = g_build_filename (g_get_home_dir (), ".thumbnails", "normal", NULL);

	if (!cropped_dir)
		cropped_dir = g_build_filename (g_get_home_dir (), ".thumbnails", "cropped", NULL);

	*large = NULL;
	*normal = NULL;
	*cropped = NULL;

	if(!g_file_test (large_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (large_dir, 0770);
	if(!g_file_test (normal_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (normal_dir, 0770);
	if(!g_file_test (cropped_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (cropped_dir, 0770);

	ascii_digest = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);
	thumb_filename = g_strdup_printf ("%s.png", ascii_digest);
	cropped_filename = g_strdup_printf ("%s.jpeg", ascii_digest);

	*large = g_build_filename (large_dir, thumb_filename, NULL);
	*normal = g_build_filename (normal_dir, thumb_filename, NULL);
	*cropped = g_build_filename (cropped_dir, cropped_filename, NULL);

	g_free (thumb_filename);
	g_free (cropped_filename);
	g_free (ascii_digest);
}


void
hildon_thumbnail_util_get_albumart_path (const gchar *artist, const gchar *album, const gchar *uri, gchar **path)
{
	gchar *art_filename, *str;
	static gchar *dir = NULL;
	gchar *down;
	
	if (album && artist) {
		gchar *_tmp14, *_tmp13;
		down = g_utf8_strdown (_tmp14 = (g_strconcat ((_tmp13 = g_strconcat (artist, " ", NULL)), album, NULL)),-1);
		g_free (_tmp14);
		g_free (_tmp13);
	} else if (uri)
		down = g_strdup (uri);
	else {
		*path = NULL;
		return;
	}

	/* I know we leak, but it's better than doing memory fragementation on 
	 * these strings ... */

	if (!dir)
		dir = g_build_filename (g_get_home_dir (), ".album_art", NULL);

	*path = NULL;

	if(!g_file_test (dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (dir, 0770);

	str = g_compute_checksum_for_string (G_CHECKSUM_MD5, down, -1);

	art_filename = g_strdup_printf ("%s.jpeg", str);

	*path = g_build_filename (dir, art_filename, NULL);

	g_free (str);
	g_free (art_filename);
	g_free (down);
}
