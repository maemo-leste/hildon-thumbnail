/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

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

static gchar *
my_compute_checksum_for_data (GChecksumType  checksum_type,
                              const guchar  *data,
                              gsize          length)
{
  GChecksum *checksum;
  gchar *retval;

  checksum = g_checksum_new (checksum_type);
  if (!checksum)
    return NULL;

  g_checksum_update (checksum, data, length);
  retval = g_strdup (g_checksum_get_string (checksum));
  g_checksum_free (checksum);

  return retval;
}

GdkPixbuf*
hildon_thumbnail_crop_resize (GdkPixbuf *src, int width, int height) {

	int x = width, y = height;
	int a = gdk_pixbuf_get_width(src);
	int b = gdk_pixbuf_get_height(src);

	GdkPixbuf *dest;

	// This is the automagic cropper algorithm 
	// It is an optimized version of a system of equations
	// Basically it maximizes the final size while minimizing the scale

	int nx, ny;
	double na, nb;
	double offx = 0, offy = 0;
	double scax, scay;

	na = a;
	nb = b;

	if(a < x && b < y) {
		//nx = a;
		//ny = b;
		g_object_ref(src);
		return src;
	} else {
		int u, v;

		nx = u = x;
		ny = v = y;

		if(a < x) {
			nx = a;
			u = a;
		}

		if(b < y) {
			ny = b;
		 	v = b;
		}

		if(a * y < b * x) {
			nb = (double)a * v / u;
			// Center
			offy = (double)(b - nb) / 2;
		} else {
			na = (double)b * u / v;
			// Center
			offx = (double)(a - na) / 2;
		}
	}

	// gdk_pixbuf_scale has crappy inputs
	scax = scay = (double) nx / na;

	offx = -offx * scax;
	offy = -offy * scay;

	dest = gdk_pixbuf_new (gdk_pixbuf_get_colorspace(src),
			       gdk_pixbuf_get_has_alpha(src), 
			       gdk_pixbuf_get_bits_per_sample(src), 
			       nx, ny);

	gdk_pixbuf_scale (src, dest, 0, 0, nx, ny, offx, offy, scax, scay,
			  GDK_INTERP_BILINEAR);

	return dest;
}

void
hildon_thumbnail_util_get_thumb_paths (const gchar *uri, gchar **large, gchar **normal, gchar **cropped, gchar **local_large, gchar **local_normal, gchar **local_cropped, gboolean as_png)
{
	gchar *ascii_digest, *filename = NULL;
	gchar *lascii_digest = NULL;
	gchar *thumb_filename;
	gchar *cropped_filename;
	static gchar *large_dir = NULL;
	static gchar *normal_dir = NULL;
	static gchar *cropped_dir = NULL;
	gchar *local_dir = NULL;
	gboolean local = (local_large || local_normal || local_cropped);

	if (local) {
		GFileInfo *info;
		GFile *file = g_file_new_for_uri (uri);
		GFile *dir_file = g_file_get_parent (file);
		GFile *thumb_file = g_file_get_child (dir_file, ".thumblocal");

		local_dir = g_file_get_uri (thumb_file);

		info = g_file_query_info (file,
					  G_FILE_ATTRIBUTE_STANDARD_NAME,
					  G_FILE_QUERY_INFO_NONE,
					  NULL, NULL);

		if (info) {
			filename = g_strdup (g_file_info_get_name (info));
			g_object_unref (info);
		}

		g_object_unref (file);
		g_object_unref (dir_file);
		g_object_unref (thumb_file);
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

	ascii_digest = my_compute_checksum_for_data (G_CHECKSUM_MD5, (const guchar *) uri, strlen (uri));

	if (as_png)
		thumb_filename = g_strdup_printf ("%s.png", ascii_digest);
	else
		thumb_filename = g_strdup_printf ("%s.jpeg", ascii_digest);

	if (as_png)
		cropped_filename = g_strdup_printf ("%s.png", ascii_digest);
	else
		cropped_filename = g_strdup_printf ("%s.jpeg", ascii_digest);

	*large = g_build_filename (large_dir, thumb_filename, NULL);
	*normal = g_build_filename (normal_dir, thumb_filename, NULL);
	*cropped = g_build_filename (cropped_dir, cropped_filename, NULL);

	if (local) {
		int slen = filename ? strlen (filename) : 0;
		if (filename && slen > 0 && local_dir) {
			gchar *lthumb_filename;
			gchar *lcropped_filename;

			lascii_digest = my_compute_checksum_for_data (G_CHECKSUM_MD5, (const guchar *) filename, slen);

			if (as_png)
				lthumb_filename = g_strdup_printf ("%s.png", lascii_digest);
			else
				lthumb_filename = g_strdup_printf ("%s.jpeg", lascii_digest);

			if (as_png)
				lcropped_filename = g_strdup_printf ("%s.png", lascii_digest);
			else
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

		g_free (lascii_digest);
		g_free (local_dir);
	}

	g_free (filename);

	g_free (thumb_filename);
	g_free (cropped_filename);
	g_free (ascii_digest);
}


static gboolean
strip_find_next_block (const gchar    *original,
		       const gunichar  open_char,
		       const gunichar  close_char,
		       gint           *open_pos,
		       gint           *close_pos)
{
	const gchar *p1, *p2;

	if (open_pos) {
		*open_pos = -1;
	}

	if (close_pos) {
		*close_pos = -1;
	}

	p1 = g_utf8_strchr (original, -1, open_char);
	if (p1) {
		if (open_pos) {
			*open_pos = p1 - original;
		}

		p2 = g_utf8_strchr (g_utf8_next_char (p1), -1, close_char);
		if (p2) {
			if (close_pos) {
				*close_pos = p2 - original;
			}
			
			return TRUE;
		}
	}

	return FALSE;
}

static gchar *
strip_characters (const gchar *original)
{
	GString         *str_no_blocks;
	gchar          **strv;
	gchar           *str;
	gboolean         blocks_done = FALSE;
	const gchar     *p;
	const gchar     *invalid_chars = "()[]<>{}_!@#$^&*+=|\\/\"'?~";
	const gchar     *invalid_chars_delimiter = "*";
	const gchar     *convert_chars = "\t";
	const gchar     *convert_chars_delimiter = " ";
	const gunichar   blocks[5][2] = {
		{ '(', ')' },
		{ '{', '}' }, 
		{ '[', ']' }, 
		{ '<', '>' }, 
		{  0,   0  }
	};

	str_no_blocks = g_string_new ("");

	p = original;

	while (!blocks_done) {
		gint pos1, pos2, i;

		pos1 = -1;
		pos2 = -1;
	
		for (i = 0; blocks[i][0] != 0; i++) {
			gint start, end;
			
			/* Go through blocks, find the earliest block we can */
			if (strip_find_next_block (p, blocks[i][0], blocks[i][1], &start, &end)) {
				if (pos1 == -1 || start < pos1) {
					pos1 = start;
					pos2 = end;
				}
			}
		}
		
		/* If either are -1 we didn't find any */
		if (pos1 == -1) {
			/* This means no blocks were found */
			g_string_append (str_no_blocks, p);
			blocks_done = TRUE;
		} else {
			/* Append the test BEFORE the block */
                        if (pos1 > 0) {
                                g_string_append_len (str_no_blocks, p, pos1);
                        }

                        p = g_utf8_next_char (p + pos2);

			/* Do same again for position AFTER block */
			if (*p == '\0') {
				blocks_done = TRUE;
			}
		}	
	}

	/* Now convert chars to lower case */
	str = g_utf8_strdown (str_no_blocks->str, -1);
	g_string_free (str_no_blocks, TRUE);

	/* Now strip invalid chars */
	g_strdelimit (str, invalid_chars, *invalid_chars_delimiter);
	strv = g_strsplit (str, invalid_chars_delimiter, -1);
	g_free (str);
        str = g_strjoinv (NULL, strv);
	g_strfreev (strv);

	/* Now convert chars */
	g_strdelimit (str, convert_chars, *convert_chars_delimiter);
	strv = g_strsplit (str, convert_chars_delimiter, -1);
	g_free (str);
        str = g_strjoinv (convert_chars_delimiter, strv);
	g_strfreev (strv);

        /* Now remove double spaces */
	strv = g_strsplit (str, "  ", -1);
	g_free (str);
        str = g_strjoinv (" ", strv);
	g_strfreev (strv);
        
        /* Now strip leading/trailing white space */
        g_strstrip (str);

	return str;
}

void
hildon_thumbnail_util_get_albumart_path (const gchar *a, const gchar *b, const gchar *prefix, gchar **path)
{
	gchar *art_filename;
	gchar *dir;
	gchar *down1, *down2;
	gchar *str1 = NULL, *str2 = NULL;
	gchar *f_a = NULL, *f_b = NULL;

	/* http://live.gnome.org/MediaArtStorageSpec */

	*path = NULL;

	if (!a && !b) {
		return;
	}

	if (!a) 
		f_a = g_strdup (" ");
	else
		f_a = strip_characters (a);

	if (!b)
		f_b = g_strdup (" ");
	else
		f_b = strip_characters (b);

	down1 = g_utf8_strdown (f_a, -1);
	down2 = g_utf8_strdown (f_b, -1);

	g_free (f_a);
	g_free (f_b);

	dir = g_build_filename (g_get_user_cache_dir (), "media-art", NULL);

	if (!g_file_test (dir, G_FILE_TEST_EXISTS)) {
		g_mkdir_with_parents (dir, 0770);
	}

	str1 = my_compute_checksum_for_data (G_CHECKSUM_MD5, (const guchar *) down1, strlen (down1));
	str2 = my_compute_checksum_for_data (G_CHECKSUM_MD5, (const guchar *) down2, strlen (down2));

	g_free (down1);
	g_free (down2);

	art_filename = g_strdup_printf ("%s-%s-%s.jpeg", prefix?prefix:"album", str1, str2);

	*path = g_build_filename (dir, art_filename, NULL);
	g_free (dir);
	g_free (art_filename);
	g_free (str1);
	g_free (str2);
}

