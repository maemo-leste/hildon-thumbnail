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


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <dbus/dbus-glib-bindings.h>

#include <Epeg.h>

#define EPEG_ERROR_DOMAIN	"HildonThumbnailerEpeg"
#define EPEG_ERROR		g_quark_from_static_string (EPEG_ERROR_DOMAIN)

#include "utils.h"
#include "epeg-plugin.h"
#include "hildon-thumbnail-plugin.h"


static gchar **supported = NULL;
static gboolean do_cropped = TRUE;
static GFileMonitor *monitor = NULL;

const gchar** 
hildon_thumbnail_plugin_supported (void)
{
	if (!supported) {
		supported = (gchar **) g_malloc0 (sizeof (gchar *) * 2);
		supported[0] = g_strdup ("image/jpeg");
		supported[1] = NULL;
	}

	return (const gchar**) supported;
}


#define HILDON_THUMBNAIL_OPTION_PREFIX "tEXt::Thumb::"
#define HILDON_THUMBNAIL_APPLICATION "hildon-thumbnail"
#define URI_OPTION HILDON_THUMBNAIL_OPTION_PREFIX "URI"
#define MTIME_OPTION HILDON_THUMBNAIL_OPTION_PREFIX "MTime"
#define SOFTWARE_OPTION "tEXt::Software"

static gboolean 
save_thumb_file_meta (GdkPixbuf *pixbuf, gchar *file, guint64 mtime, const gchar *uri, GError **error)
{
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
	    HILDON_THUMBNAIL_APPLICATION "-" VERSION,
	    NULL
	};

	g_sprintf(mtime_str, "%lu", mtime);

	ret = gdk_pixbuf_savev (pixbuf, file, "png", 
				(char **) default_keys, 
				(char **) default_values, 
				error);

	return ret;
}



static gboolean 
save_thumb_file_cropped (GdkPixbuf *pixbuf, gchar *file, guint64 mtime, const gchar *uri, GError **error)
{
	gboolean ret;

	ret = gdk_pixbuf_save (pixbuf, file, "jpeg", error, NULL);

	return ret;
}

static GdkPixbuf*
crop_resize (GdkPixbuf *src, int width, int height) {
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
	scax = scay = (double)nx / na;

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

/* 
static void
get_thumbnail_paths (const gchar  *uri, 
				     gchar       **epeg_path,
					 gchar       **large_path,
					 gchar       **normal_path,
					 gchar       **cropped_path)
{
	gchar *filename;
	gchar *dir, *str;

	hildon_thumbnail_util_get_thumb_paths (uri, large_path, normal_path, 
										   cropped_path,
										   NULL, NULL, NULL);

	*epeg_path = NULL;

	dir = g_build_filename (g_get_home_dir (), ".thumbnails", "epeg", NULL);

	if (!g_file_test (dir, G_FILE_TEST_EXISTS)) {
		g_mkdir_with_parents (dir, 0770);
	}

	str = g_compute_checksum_for_string (G_CHECKSUM_MD5, uri, -1);

	filename = g_strdup_printf ("%s.jpeg", str);
	g_free (str);

	*epeg_path = g_build_filename (dir, filename, NULL);

	g_free (dir);
	g_free (filename);
}
*/

static void
destroy_pixbuf (guchar *pixels, gpointer data)
{
	epeg_pixels_free ((Epeg_Image *) data, pixels);
	epeg_close ((Epeg_Image *) data);
}

void
hildon_thumbnail_plugin_create (GStrv uris, gchar *mime_hint, GStrv *failed_uris, GError **error)
{
	guint i = 0;
	GString *errors = NULL;
	GList *failed = NULL;

	while (uris[i] != NULL) {
		Epeg_Image *im;
		gchar *uri = uris[i];
		GFile *file = NULL;
		gchar *path;
		gboolean had_err = FALSE;
		guchar *data;
		GdkPixbuf *pixbuf_large = NULL, 
				  *pixbuf_normal, 
				  *pixbuf_cropped;
		gchar *large = NULL, 
			  *normal = NULL, 
			  *cropped = NULL;
			  /* *epeg = NULL; */
		guint64 mtime;
		gboolean just_crop;
		GFileInfo *finfo = NULL;
		GError *nerror = NULL;

		/* get_thumbnail_paths (uri, &epeg, &large, &normal, &cropped); */

		hildon_thumbnail_util_get_thumb_paths (uri, &large, &normal, 
										   &cropped,
										   NULL, NULL, NULL);

		just_crop = (g_file_test (large, G_FILE_TEST_EXISTS) && 
					/* g_file_test (epeg, G_FILE_TEST_EXISTS) && */
					 g_file_test (normal, G_FILE_TEST_EXISTS) && 
					!g_file_test (cropped, G_FILE_TEST_EXISTS));

		if (just_crop && !do_cropped) {
			g_free (cropped);
			g_free (normal);
			g_free (large);
			/* g_free (epeg); */
			continue;
		}

		file = g_file_new_for_uri (uri);
		path = g_file_get_path (file);

		if (!path) {
			had_err = TRUE;
			goto nerror_handler;
		}

		finfo = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED,
					  G_FILE_QUERY_INFO_NONE,
					  NULL, &nerror);

		if (nerror) {
			had_err = TRUE;
			goto nerror_handler;
		}

		mtime = g_file_info_get_attribute_uint64 (finfo, G_FILE_ATTRIBUTE_TIME_MODIFIED);

		im = epeg_file_open (path);

		if (!im) {
			had_err = TRUE;
			goto nerror_handler;
		}

		epeg_decode_colorspace_set (im, EPEG_RGB8);
		epeg_decode_size_set (im, 256, 256);
		epeg_quality_set (im, 80);
		epeg_thumbnail_comments_enable (im, 0);

		data = (guchar *) epeg_pixels_get (im, 0, 0, 256, 256);

		pixbuf_large = gdk_pixbuf_new_from_data ((const guchar*) data, 
								  GDK_COLORSPACE_RGB, FALSE, 
								  8, 256, 256, 256*3,
								  destroy_pixbuf, im);

		/* epeg_file_output_set (im, epeg); 
		epeg_encode (im); */

		save_thumb_file_meta (pixbuf_large, large, mtime, uri, &nerror);

		if (nerror)
			goto nerror_handler;

		pixbuf_cropped = crop_resize (pixbuf_large, 124, 124);

		save_thumb_file_cropped (pixbuf_cropped, cropped, mtime, uri, &nerror);

		g_object_unref (pixbuf_cropped);

		if (nerror)
			goto nerror_handler;

		pixbuf_normal = gdk_pixbuf_scale_simple (pixbuf_large,
												 128, 128,
												 GDK_INTERP_HYPER);

		save_thumb_file_meta (pixbuf_normal, normal, mtime, uri, &nerror);

		g_object_unref (pixbuf_normal);

		if (nerror)
			goto nerror_handler;

		nerror_handler:

		if (had_err) {
			gchar *msg;
			if (nerror) {
				msg = g_strdup (nerror->message);
				g_error_free (nerror);
				nerror = NULL;
			} else
				msg = g_strdup_printf ("Can't open %s", uri);
			if (!errors)
				errors = g_string_new ("");
			g_string_append_printf (errors, "[`%s': %s] ", 
								    uri, msg);
			failed = g_list_prepend (failed, g_strdup (uri));
			g_free (msg);
		}

		if (pixbuf_large)
				g_object_unref (pixbuf_large);

		if (file)
			g_object_unref (file);
		if (finfo)
			g_object_unref (finfo);

		g_free (path);
		/* g_free (epeg); */
		g_free (large);
		g_free (normal);
		g_free (cropped);

		i++;
	}

	if (errors && failed) {
		guint t = 0;
		GStrv furis = (GStrv) g_malloc0 (sizeof (gchar*) * (g_list_length (failed) + 1));
		GList *copy = failed;

		t = 0;

		while (copy) {
			furis[t] = copy->data;
			copy = g_list_next (copy);
			t++;
		}
		furis[t] = NULL;

		*failed_uris = furis;

		g_list_free (failed);

		g_set_error (error, EPEG_ERROR, 0,
			     errors->str);
		g_string_free (errors, TRUE);
	}

	return;
}

void 
hildon_thumbnail_plugin_stop (void)
{
	if (supported)
		g_strfreev (supported);
	supported = NULL;
	if (monitor)
		g_object_unref (monitor);
}

static void
reload_config (const gchar *config) 
{
	GKeyFile *keyfile;
	GStrv mimetypes;
	guint i = 0, length;

	keyfile = g_key_file_new ();

	if (!g_key_file_load_from_file (keyfile, config, G_KEY_FILE_NONE, NULL)) {
		do_cropped = TRUE;
		return;
	}

	do_cropped = g_key_file_get_boolean (keyfile, "Hildon Thumbnailer", "DoCropping", NULL);

	g_key_file_free (keyfile);
}

static void 
on_file_changed (GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data)
{
	if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT || event_type == G_FILE_MONITOR_EVENT_CREATED) {
		gchar *config = g_file_get_path (file);
		reload_config (config);
		g_free (config);
	}
}

void 
hildon_thumbnail_plugin_init (gboolean *cropping, register_func func, gpointer thumbnailer, GModule *module, GError **error)
{
	gchar *config = g_build_filename (g_get_user_config_dir (), "hildon-thumbnailer", "epeg-plugin.conf", NULL);
	GFile *file = g_file_new_for_path (config);
	guint i = 0;
	const gchar **supported;

	monitor =  g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);

	g_signal_connect (G_OBJECT (monitor), "changed", 
			  G_CALLBACK (on_file_changed), NULL);

	reload_config (config);

	*cropping = do_cropped;

	if (func) {
		supported = hildon_thumbnail_plugin_supported ();
		if (supported) {
			while (supported[i] != NULL) {
				func (thumbnailer, supported[i], module, TRUE);
				i++;
			}
		}
	}

	g_free (config);
}
