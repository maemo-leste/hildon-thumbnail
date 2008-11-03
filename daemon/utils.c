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

#include <gio/gio.h>
#include <string.h>
#include "utils.h"


void
hildon_thumbnail_util_get_thumb_paths (const gchar *uri, gchar **large, gchar **normal, gchar **cropped, gchar **local_large, gchar **local_normal, gchar **local_cropped, gboolean as_png)
{
	gchar *ascii_digest, *filename = NULL;
	gchar *lascii_digest = NULL;
	gchar *thumb_filename, *uri_t = NULL;
	gchar *cropped_filename, *ptr;
	static gchar *large_dir = NULL;
	static gchar *normal_dir = NULL;
	static gchar *cropped_dir = NULL;
	gchar *local_dir = NULL;
	GFile *file; GFileInfo *info;
	gboolean local = (local_large || local_normal || local_cropped);

	if (local) {
		uri_t = g_strdup (uri);
		ptr = strrchr (uri_t, '/');

		if (ptr) {
			*ptr = '\0';
			local_dir = g_strdup_printf ("%s/.thumblocal", uri_t);
		}

		g_free (uri_t);

		file = g_file_new_for_uri (uri);
		info = g_file_query_info (file,
					  G_FILE_ATTRIBUTE_STANDARD_NAME,
					  G_FILE_QUERY_INFO_NONE,
					  NULL, NULL);

		if (info) {
			filename = g_strdup (g_file_info_get_name (info));
			g_object_unref (info);
		}

		g_object_unref (file);
	}

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

	if (as_png)
		thumb_filename = g_strdup_printf ("%s.png", ascii_digest);
	else
		thumb_filename = g_strdup_printf ("%s.jpeg", ascii_digest);

	cropped_filename = g_strdup_printf ("%s.jpeg", ascii_digest);


	*large = g_build_filename (large_dir, thumb_filename, NULL);
	*normal = g_build_filename (normal_dir, thumb_filename, NULL);
	*cropped = g_build_filename (cropped_dir, cropped_filename, NULL);

	if (local) {
		if (filename && local_dir) {
			gchar *lthumb_filename;
			gchar *lcropped_filename;

			lascii_digest = g_compute_checksum_for_string (G_CHECKSUM_MD5, filename, -1);
			if (as_png)
				lthumb_filename = g_strdup_printf ("%s.png", lascii_digest);
			else
				lthumb_filename = g_strdup_printf ("%s.jpeg", lascii_digest);
			lcropped_filename = g_strdup_printf ("%s.jpeg", lascii_digest);

			if (local_large)
				*local_large = g_build_filename (local_dir, "large", lthumb_filename, NULL);
			if (local_normal)
				*local_normal = g_build_filename (local_dir, "normal", lthumb_filename, NULL);
			if (local_cropped)
				*local_cropped = g_build_filename (local_dir, "cropped", lcropped_filename, NULL);

			g_free (lthumb_filename);
			g_free (lcropped_filename);

		} else {
			if (local_large)
				*local_large = g_strdup ("");
			if (local_normal)
				*local_normal = g_strdup ("");
			if (local_cropped)
				*local_cropped = g_strdup ("");
		}

		g_free (filename);
		g_free (lascii_digest);
		g_free (local_dir);
	}

	g_free (thumb_filename);
	g_free (cropped_filename);
	g_free (ascii_digest);
}



static gchar*
strip_characters (const gchar *original)
{
	const gchar *foo = "()[]<>{}_!@#$^&*+=|\\/\"'?~";
	guint osize = strlen (original);
	gchar *retval = (gchar *) g_malloc0 (sizeof (gchar *) * osize);
	guint i = 0, y = 0;

	while (i < osize) {

		/* Remove (anything) */

		if (original[i] == '(') {
			gchar *loc = strchr (original+i, ')');
			if (loc) {
				i = loc - original + 1;
				continue;
			}
		}

		/* Remove [anything] */

		if (original[i] == '[') {
			gchar *loc = strchr (original+i, ']');
			if (loc) {
				i = loc - original + 1;
				continue;
			}
		}

		/* Remove {anything} */

		if (original[i] == '{') {
			gchar *loc = strchr (original+i, '}');
			if (loc) {
				i = loc - original + 1;
				continue;
			}
		}

		/* Remove <anything> */

		if (original[i] == '<') {
			gchar *loc = strchr (original+i, '>');
			if (loc) {
				i = loc - original + 1;
				continue;
			}
		}

		/* Remove double whitespaces */

		if ((y > 0) &&
		    (original[i] == ' ' || original[i] == '\t') &&
		    (retval[y-1] == ' ' || retval[y-1] == '\t')) {
			i++;
			continue;
		}

		/* Remove strange characters */

		if (!strchr (foo, original[i])) {
			retval[y] = original[i]!='\t'?original[i]:' ';
			y++;
		}

		i++;
	}

	retval[y] = 0;

	return retval;
}

void
hildon_thumbnail_util_get_albumart_path (const gchar *a, const gchar *b, const gchar *prefix, gchar **path)
{
	gchar *art_filename;
	gchar *dir;
	gchar *str;
	gchar *down;
	gchar *f_a = NULL, *f_b = NULL;

	*path = NULL;

	if (!a && !b) {
		return;
	}

	if (a)
		f_a = strip_characters (a);

	if (b)
		f_b = strip_characters (b);

	str = g_strconcat (a ? f_a : "", 
			   " ", 
			   b ? f_b : "", 
			   NULL);

	g_free (f_a);
	g_free (f_b);

	down = g_utf8_strdown (str, -1);
	g_free (str);

	dir = g_build_filename (g_get_user_cache_dir (), "media-art", NULL);

	if (!g_file_test (dir, G_FILE_TEST_EXISTS)) {
		g_mkdir_with_parents (dir, 0770);
	}

	str = g_compute_checksum_for_string (G_CHECKSUM_MD5, down, -1);
	g_free (down);

	art_filename = g_strdup_printf ("%s-%s.jpeg", prefix?prefix:"album", str);
	g_free (str);

	*path = g_build_filename (dir, art_filename, NULL);
	g_free (dir);
	g_free (art_filename);
}
