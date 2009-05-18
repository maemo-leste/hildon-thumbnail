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
 * MERCHANTABILITY or FITNESS FOR  PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/types.h>
#include <utime.h>

#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <dbus/dbus-glib-bindings.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-io.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <png.h>


#include "utils.h"

#include <hildon-thumbnail-plugin.h>

static gboolean had_init = FALSE;
static gboolean is_active = FALSE;
static GFileMonitor *monitor = NULL;

#define HILDON_THUMBNAIL_OPTION_PREFIX "tEXt::Thumb::"
#define HILDON_THUMBNAIL_APPLICATION "hildon-thumbnail"
#define URI_OPTION HILDON_THUMBNAIL_OPTION_PREFIX "URI"
#define MTIME_OPTION HILDON_THUMBNAIL_OPTION_PREFIX "MTime"
#define SOFTWARE_OPTION "tEXt::Software"



gchar *
hildon_thumbnail_outplugin_get_orig (const gchar *path)
{
	gint	     fd_png;
	FILE	    *png;
	png_structp  png_ptr;
	png_infop    info_ptr;
	gint	     num_text;
	png_textp    text_ptr;
	gchar       *retval = NULL;

#if defined(__linux__)
	if ((fd_png = g_open (path, (O_RDONLY | O_NOATIME))) == -1) {
#else
	if ((fd_png = g_open (path, O_RDONLY)) == -1) {
#endif
		return NULL;
	}

	if ((png = fdopen (fd_png, "r"))) {
		png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING,
						  NULL,
						  NULL,
						  NULL);
		if (!png_ptr) {
			fclose (png);
			return NULL;
		}

		info_ptr = png_create_info_struct (png_ptr);
		if (!info_ptr) {
			png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
			fclose (png);
			return NULL;
		}

		png_init_io (png_ptr, png);
		png_read_info (png_ptr, info_ptr);

		if (png_get_text (png_ptr, info_ptr, &text_ptr, &num_text) > 0) {
			gint i;

			for (i = 0; i < num_text; i++) {
				if (!text_ptr[i].key) {
					continue;
				}
				if (strcasecmp ("Thumb::URI", text_ptr[i].key) != 0) {
					continue;
				}
				if (text_ptr[i].text && text_ptr[i].text[0] != '\0') {
					retval = g_strdup (text_ptr[i].text);
					break;
				}
			}
		}
		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		fclose (png);
	} else {
		close (fd_png);
	}

	return retval;
}


static void
cleanup (GDir *dir, const gchar *dirname, const gchar *uri_match, guint since)
{
	const gchar *filen;
	for (filen = g_dir_read_name (dir); filen; filen = g_dir_read_name (dir)) {
		if (g_str_has_suffix (filen, "png")) {
			gchar *fulln = g_build_filename (dirname, filen, NULL);
			gchar *orig = hildon_thumbnail_outplugin_get_orig (fulln);
			if (orig && g_str_has_prefix (orig, uri_match)) {
				struct stat st;
				g_stat (fulln, &st);
				if (st.st_mtime <= (gint) since) {
					g_unlink (fulln);
				}
				g_free (orig);
			}
			g_free (fulln);
		}
	}
}

void
hildon_thumbnail_outplugin_cleanup (const gchar *uri_match, guint max_mtime)
{
	GDir *dir;
	gchar *dirname;

	dirname = g_build_filename (g_get_home_dir (), ".thumbnails", "large", NULL);
	dir = g_dir_open (dirname, 0, NULL);
	if (dir) {
		cleanup (dir, dirname, uri_match, max_mtime);
		g_dir_close (dir);
	}
	g_free (dirname);

	dirname = g_build_filename (g_get_home_dir (), ".thumbnails", "normal", NULL);
	dir = g_dir_open (dirname, 0, NULL);
	if (dir) {
		cleanup (dir, dirname, uri_match, max_mtime);
		g_dir_close (dir);
	}
	g_free (dirname);

	dirname = g_build_filename (g_get_home_dir (), ".thumbnails", "cropped", NULL);
	dir = g_dir_open (dirname, 0, NULL);
	if (dir) {
		cleanup (dir, dirname, uri_match, max_mtime);
		g_dir_close (dir);
	}
	g_free (dirname);

}

gboolean
hildon_thumbnail_outplugin_needs_out (HildonThumbnailPluginOutType type, guint64 mtime, const gchar *uri, gboolean *err_file)
{
	gboolean retval, check = FALSE, f = FALSE;
	gchar *large, *normal, *cropped, *filen, *filenp;
	GFile *parent, *file, *fail_file, *fail_dir;

	hildon_thumbnail_util_get_thumb_paths (uri, &large, &normal, &cropped,
					       NULL, NULL, NULL, TRUE);

	switch (type) {
		case HILDON_THUMBNAIL_PLUGIN_OUTTYPE_LARGE:
			filen = large;
		break;
		case HILDON_THUMBNAIL_PLUGIN_OUTTYPE_NORMAL:
			filen = normal;
		break;
		case HILDON_THUMBNAIL_PLUGIN_OUTTYPE_CROPPED:
			filen = cropped;
		break;
	}

	retval = TRUE;

	file = g_file_new_for_path (filen);
	filenp = g_file_get_basename (file);

	parent = g_file_get_parent (file); /* ~/.thumbnails/large */
	fail_dir = g_file_get_parent (parent); /* ~/.thumbnails/ */
	g_object_unref (parent);

	parent = g_file_get_child (fail_dir, "fail"); /* ~/.thumbnails/fail */
	g_object_unref (fail_dir);

	fail_dir = g_file_get_child (parent, PACKAGE_NAME "-" PACKAGE_VERSION); /* ~/.thumbnails/large/hild-thum-version/ */
	g_object_unref (parent);

	fail_file = g_file_get_child (fail_dir, filenp);
	g_object_unref (fail_dir);
	g_free (filenp);

	if (g_file_query_exists (fail_file, NULL)) {
		g_object_unref (file);
		file = g_object_ref (fail_file);
		check = TRUE;
		f = TRUE;
	} else if (g_file_query_exists (file, NULL)) {
		check = TRUE;
	}

	if (check) {
		GFileInfo *info;
		info = g_file_query_info (file, 
					   G_FILE_ATTRIBUTE_TIME_MODIFIED,
					   G_FILE_QUERY_INFO_NONE, NULL, NULL);
		if (info) {
			guint64 fmtime;
			fmtime = g_file_info_get_attribute_uint64 (info, 
								   G_FILE_ATTRIBUTE_TIME_MODIFIED);
			if (fmtime == (guint64) mtime) {
				if (err_file && f)
					*err_file = TRUE;
				retval = FALSE;
			}
			g_object_unref (info);
		}
	}

	g_object_unref (fail_file);
	g_object_unref (file);

	g_free (normal);
	g_free (large);
	g_free (cropped);

	return retval;
}


void
hildon_thumbnail_outplugin_put_error (guint64 mtime, const gchar *uri)
{
	gchar *large, *normal, *cropped, *filenp, *dirn;
	GFile *parent, *file, *fail_file, *fail_dir;
	GFileOutputStream *out;
	struct utimbuf buf;
	GError *error = NULL;

	hildon_thumbnail_util_get_thumb_paths (uri, &large, &normal, &cropped,
					       NULL, NULL, NULL, TRUE);

	file = g_file_new_for_path (large);
	filenp = g_file_get_basename (file);

	parent = g_file_get_parent (file); /* ~/.thumbnails/large */
	fail_dir = g_file_get_parent (parent); /* ~/.thumbnails/ */
	g_object_unref (parent);

	parent = g_file_get_child (fail_dir, "fail"); /* ~/.thumbnails/fail */
	g_object_unref (fail_dir);

	fail_dir = g_file_get_child (parent, PACKAGE_NAME "-" PACKAGE_VERSION); /* ~/.thumbnails/large/hild-thum-version/ */
	g_object_unref (parent);

	fail_file = g_file_get_child (fail_dir, filenp);
	dirn = g_file_get_path (fail_dir);
	g_mkdir_with_parents (dirn, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	g_free (dirn);

	g_object_unref (fail_dir);
	g_free (filenp);

	if (g_file_query_exists (fail_file, NULL))
		g_file_delete (fail_file, NULL, NULL);
	out = g_file_create (fail_file, 0, NULL, &error);

	if (out) {
		g_object_unref (out);
	}

	if (error) {
		g_debug ("%s\n", error->message);
		g_error_free (error);
	}

	filenp = g_file_get_path (fail_file);
	buf.actime = buf.modtime = mtime;
	utime (filenp, &buf);
	g_free (filenp);

	g_object_unref (fail_file);
	g_object_unref (file);

	g_free (normal);
	g_free (large);
	g_free (cropped);

}

void
hildon_thumbnail_outplugin_out (const guchar *rgb8_pixmap, 
				guint width, guint height,
				guint rowstride, guint bits_per_sample,
				gboolean has_alpha,
				HildonThumbnailPluginOutType type,
				guint64 mtime, 
				const gchar *uri, 
				GError **error)
{
	GdkPixbuf *pixbuf;
	gchar *large, *normal, *cropped, *filen, *temp;
	char mtime_str[64];
	struct utimbuf buf;
	GError *nerror = NULL;

	const char *default_keys[] = {
		URI_OPTION,
		MTIME_OPTION,
		SOFTWARE_OPTION,
		NULL
	};

	const char *default_values[] = {
		uri,
		mtime_str,
		HILDON_THUMBNAIL_APPLICATION "-" VERSION,
		NULL
	};

	hildon_thumbnail_util_get_thumb_paths (uri, &large, &normal, &cropped,
					       NULL, NULL, NULL, TRUE);

	switch (type) {
		case HILDON_THUMBNAIL_PLUGIN_OUTTYPE_LARGE:
			filen = large;
		break;
		case HILDON_THUMBNAIL_PLUGIN_OUTTYPE_NORMAL:
			filen = normal;
		break;
		case HILDON_THUMBNAIL_PLUGIN_OUTTYPE_CROPPED:
			filen = cropped;
		break;
	}

	pixbuf = gdk_pixbuf_new_from_data ((const guchar*) rgb8_pixmap, 
					   GDK_COLORSPACE_RGB, has_alpha, 
					   bits_per_sample, width, height, rowstride,
					   NULL, NULL);


	g_sprintf (mtime_str, "%Lu", mtime);

	temp = g_strdup_printf ("%s.tmp", filen);

	gdk_pixbuf_savev (pixbuf, temp, "png", 
			  (char **) default_keys, 
			  (char **) default_values, 
			  &nerror);

	g_object_unref (pixbuf);

	if (!nerror) {
		g_rename (temp, filen);
		buf.actime = buf.modtime = mtime;
		utime (filen, &buf);
	} else {
		hildon_thumbnail_outplugin_put_error (mtime, uri);
		g_propagate_error (error, nerror);
	}

	g_free (temp);


	g_free (normal);
	g_free (large);
	g_free (cropped);

	return;
}



static void
reload_config (const gchar *config) 
{
	GKeyFile *keyfile;
	GError *error = NULL;

	keyfile = g_key_file_new ();

	if (!g_key_file_load_from_file (keyfile, config, G_KEY_FILE_NONE, NULL)) {
		is_active = FALSE;
		g_key_file_free (keyfile);
		return;
	}

	is_active = g_key_file_get_boolean (keyfile, "Hildon Thumbnailer", "IsActive", &error);

	if (error) {
		is_active = FALSE;
		g_error_free (error);
	}

	g_key_file_free (keyfile);
}


static void 
on_file_changed (GFileMonitor *monitor_, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data)
{
	if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT || event_type == G_FILE_MONITOR_EVENT_CREATED) {
		gchar *config = g_file_get_path (file);
		reload_config (config);
		g_free (config);
	}
}

gboolean hildon_thumbnail_outplugin_stop (void) 
{
	if (monitor)
		g_object_unref (monitor);
	return FALSE;
}

gboolean
hildon_thumbnail_outplugin_is_active (void) 
{
	if (!had_init) {
		gchar *config = g_build_filename (g_get_user_config_dir (), "hildon-thumbnailer", "gdkpixbuf-png-output-plugin.conf", NULL);
		GFile *file = g_file_new_for_path (config);

		monitor =  g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);

		g_signal_connect (G_OBJECT (monitor), "changed", 
				  G_CALLBACK (on_file_changed), NULL);

		/* g_object_unref (monitor); */
		g_object_unref (file);

		reload_config (config);

		g_free (config);
		had_init = TRUE;
	}

	return is_active;
}
