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

#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <dbus/dbus-glib-bindings.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-io.h>


#define DEFAULT_ERROR_DOMAIN	"HildonThumbnailerGdkPixbuf"
#define DEFAULT_ERROR		g_quark_from_static_string (DEFAULT_ERROR_DOMAIN)

#include "utils.h"
#include "gdkpixbuf-plugin.h"

#include <hildon-thumbnail-plugin.h>

#define HAVE_OSSO

#ifdef HAVE_OSSO
#define MAX_SIZE	(1024*1024*5)
#define MAX_PIX		(5000000)
#define MAX_W		(5000)
#define MAX_H		(5000)
#else
#define MAX_SIZE	(1024*1024*100)
#define MAX_PIX		(5000000*(100/5))
#define MAX_W		(10000)
#define MAX_H		(10000)
#endif

GdkPixbuf *
my_gdk_pixbuf_new_from_stream_at_scale (GInputStream  *stream,
				     gint	    width,
				     gint 	    height,
				     gboolean       preserve_aspect_ratio,
				     GCancellable  *cancellable,
		  	    	     GError       **error);

GdkPixbuf *
my_gdk_pixbuf_new_from_stream (GInputStream  *stream,
			    GCancellable  *cancellable,
			       guint max_pix,
			       guint max_w, guint max_h,
			    GError       **error);

static gchar **supported = NULL;
static gboolean do_cropped = TRUE;
static GFileMonitor *monitor = NULL;

const gchar** 
hildon_thumbnail_plugin_supported (void)
{
	if (!supported) {
		GSList *formats = gdk_pixbuf_get_formats (), *copy;
		GPtrArray *types_support = g_ptr_array_new ();
		guint i;
		copy = formats;
		while (copy) {
			gchar **mime_types = gdk_pixbuf_format_get_mime_types (copy->data);
			i = 0;
			while (mime_types[i] != NULL) {
				g_ptr_array_add (types_support, mime_types[i]);
				i++;
			}
			copy = g_slist_next (copy);
		}
		supported = (gchar **) g_malloc0 (sizeof (gchar *) * (types_support->len + 1 + 1));
		for (i = 0 ; i < types_support->len; i++)
			supported[i] =  g_strdup (g_ptr_array_index (types_support, i));
		g_ptr_array_free (types_support, TRUE);

		/* Maemo specific */
		supported[i] = g_strdup ("sketch/png");

		g_slist_free (formats);
	}

	return (const gchar**) supported;
}

static GdkPixbuf*
crop_resize (GdkPixbuf *src, int width, int height) {
	return hildon_thumbnail_crop_resize (src, width, height);
}

static gboolean
is_animated_gif (const gchar *filename)
{
	guint frame_count = 0;
	FILE *gif_file = fopen (filename, "r");

	if (gif_file) {
		while (!feof (gif_file) && frame_count < 2) {
			gchar buffer[1024];
			size_t read;
			size_t t;

			read = fread (buffer, 1, 1024, gif_file);
			for (t = 0; t < read; t++) {
//				if (buffer[t]   == 0x00 && buffer[t+1] == 0x21 &&
//				    buffer[t+2] == 0xF9 && buffer[t+3] == 0x04) {
//					    frame_count++;
//				}
				if (buffer[t]   == 0x00 && buffer[t+1] == 0x2C) {
					    frame_count++;
				}

			}
		}

		fclose (gif_file);
	}

	return (frame_count > 1);
}


void
hildon_thumbnail_plugin_create (GStrv uris, gchar *mime_hint, GStrv *failed_uris, GError **error)
{
	guint i = 0;
	GString *errors = NULL;
	GList *failed = NULL;

	while (uris[i] != NULL) {
		GError *nerror = NULL;
		GFileInfo *info;
		GFile *file;
		GFileInputStream *stream=NULL;
		gchar *uri = uris[i];
		GdkPixbuf *pixbuf_large;
		GdkPixbuf *pixbuf_normal;
		GdkPixbuf *pixbuf, *pixbuf_cropped;
		guint64 mtime, msize;
		const guchar *rgb8_pixels;
		guint width; guint height;
		guint rowstride; 
		gboolean err_file = FALSE;
		gchar *path; 

		file = g_file_new_for_uri (uri);

		path = g_file_get_path (file);

		if (path) {
			gchar *up = g_utf8_strup (path, -1);
			if (g_str_has_suffix (up, "GIF")) {
				if (is_animated_gif (path)) {
					g_set_error (&nerror, DEFAULT_ERROR, 0,
						     "Animated GIF (%s) is not supported",
						     uri);
				}
			}
			g_free (path);
			g_free (up);
		}
		
		info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED ","
					        G_FILE_ATTRIBUTE_STANDARD_SIZE,
					  G_FILE_QUERY_INFO_NONE,
					  NULL, &nerror);

		if (nerror)
			goto nerror_handler;

		msize = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_STANDARD_SIZE);
		
		if (msize > MAX_SIZE) {
			g_set_error (&nerror, DEFAULT_ERROR, 0, "%s is too large",
				     uri);
			goto nerror_handler;
		}

		mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

		if (
#ifdef LARGE_THUMBNAILS
		    !hildon_thumbnail_outplugins_needs_out (HILDON_THUMBNAIL_PLUGIN_OUTTYPE_LARGE, mtime, uri, &err_file) &&
#endif
#ifdef NORMAL_THUMBNAILS
		    !hildon_thumbnail_outplugins_needs_out (HILDON_THUMBNAIL_PLUGIN_OUTTYPE_NORMAL, mtime, uri, &err_file) &&
#endif
		    !hildon_thumbnail_outplugins_needs_out (HILDON_THUMBNAIL_PLUGIN_OUTTYPE_CROPPED, mtime, uri, &err_file))
			goto nerror_handler;

		stream = g_file_read (file, NULL, &nerror);

		if (nerror)
			goto nerror_handler;

#ifdef LARGE_THUMBNAILS
		if (hildon_thumbnail_outplugins_needs_out (HILDON_THUMBNAIL_PLUGIN_OUTTYPE_LARGE, mtime, uri, &err_file)) {

			GdkPixbuf *pixbuf_large1 = my_gdk_pixbuf_new_from_stream_at_scale (G_INPUT_STREAM (stream),
									    256, 256,
									    TRUE,
									    NULL,
									    &nerror);

			if (nerror) {
				if (pixbuf_large1)
					g_object_unref (pixbuf_large1);
				goto nerror_handler;
			}

			pixbuf_large = gdk_pixbuf_apply_embedded_orientation (pixbuf_large1);

			rgb8_pixels = gdk_pixbuf_get_pixels (pixbuf_large);
			width = gdk_pixbuf_get_width (pixbuf_large);
			height = gdk_pixbuf_get_height (pixbuf_large);
			rowstride = gdk_pixbuf_get_rowstride (pixbuf_large);

			hildon_thumbnail_outplugins_do_out (rgb8_pixels, 
							    width,
							    height,
							    rowstride,
							    gdk_pixbuf_get_bits_per_sample (pixbuf_large),
							    gdk_pixbuf_get_has_alpha (pixbuf_large),
							    HILDON_THUMBNAIL_PLUGIN_OUTTYPE_LARGE,
							    mtime, 
							    uri, 
							    &nerror);

			g_object_unref (pixbuf_large);
			g_object_unref (pixbuf_large1);

			if (nerror)
				goto nerror_handler;

			g_seekable_seek (G_SEEKABLE (stream), 0, G_SEEK_SET, NULL, &nerror);

			if (nerror)
				goto nerror_handler;

		}

#endif

#ifdef NORMAL_THUMBNAILS

		if (hildon_thumbnail_outplugins_needs_out (HILDON_THUMBNAIL_PLUGIN_OUTTYPE_NORMAL, mtime, uri, &err_file)) {

			GdkPixbuf *pixbuf_normal1 = my_gdk_pixbuf_new_from_stream_at_scale (G_INPUT_STREAM (stream),
									     128, 128,
									     TRUE,
									     NULL,
									     &nerror);

			if (nerror) {
				if (pixbuf_normal1)
					g_object_unref (pixbuf_normal1);
				goto nerror_handler;
			}

			pixbuf_normal = gdk_pixbuf_apply_embedded_orientation (pixbuf_normal1);

			rgb8_pixels = gdk_pixbuf_get_pixels (pixbuf_normal);
			width = gdk_pixbuf_get_width (pixbuf_normal);
			height = gdk_pixbuf_get_height (pixbuf_normal);
			rowstride = gdk_pixbuf_get_rowstride (pixbuf_normal);

			hildon_thumbnail_outplugins_do_out (rgb8_pixels, 
							    width,
							    height,
							    rowstride,
							    gdk_pixbuf_get_bits_per_sample (pixbuf_normal),
							    gdk_pixbuf_get_has_alpha (pixbuf_normal),
							    HILDON_THUMBNAIL_PLUGIN_OUTTYPE_NORMAL,
							    mtime, 
							    uri, 
							    &nerror);

			g_object_unref (pixbuf_normal);
			g_object_unref (pixbuf_normal1);

			if (nerror)
				goto nerror_handler;

			g_seekable_seek (G_SEEKABLE (stream), 0, G_SEEK_SET, NULL, &nerror);

			if (nerror)
				goto nerror_handler;

		}

#endif

		if (do_cropped && hildon_thumbnail_outplugins_needs_out (HILDON_THUMBNAIL_PLUGIN_OUTTYPE_CROPPED, mtime, uri, &err_file)) {
			int a, b;

			GdkPixbuf *pixbuf1 = my_gdk_pixbuf_new_from_stream (G_INPUT_STREAM (stream), 
									    NULL, MAX_PIX, 
									    MAX_W, MAX_H, &nerror);

			if (nerror) {
				if (pixbuf1)
					g_object_unref (pixbuf1);
				goto nerror_handler;
			}

			pixbuf = gdk_pixbuf_apply_embedded_orientation (pixbuf1);

			g_object_unref (pixbuf1);

			a = gdk_pixbuf_get_width (pixbuf);
			b = gdk_pixbuf_get_height (pixbuf);

			/* Changed in NB#118963 comment #38 */

			if (a < 124 || b < 124) {
				int a_wanted, b_wanted;


				/* For items where either x or y is smaller than 124, 
				 * the image is taken aspect ratio retained by scaling
				 * the dimension which is greater than 124 to the size
				 * 124. */

				if ((double)b * (double)124 > (double)a * (double)124) {
					a_wanted = 0.5 + (double)a * (double)124 / (double)b;
					b_wanted = 124;
				} else {
					b_wanted = 0.5 + (double)b * (double)124 / (double)a;
					a_wanted = 124;
				}

				pixbuf_cropped = gdk_pixbuf_scale_simple (pixbuf,
									  a_wanted > 0 ? a_wanted : 1,
									  b_wanted > 0 ? b_wanted : 1,
									  GDK_INTERP_BILINEAR);

			} else {

				/* For items where x and y are both larger than 124, the 
				 * thumbnail is taken from the largest square in the 
				 * middle of the image and scaled down to size 124x124. */

				pixbuf_cropped = crop_resize (pixbuf, 124, 124);
			}

			g_object_unref (pixbuf);

			rgb8_pixels = gdk_pixbuf_get_pixels (pixbuf_cropped);
			width = gdk_pixbuf_get_width (pixbuf_cropped);
			height = gdk_pixbuf_get_height (pixbuf_cropped);
			rowstride = gdk_pixbuf_get_rowstride (pixbuf_cropped);
				
			hildon_thumbnail_outplugins_do_out (rgb8_pixels, 
							    width,
							    height,
							    rowstride,
							    gdk_pixbuf_get_bits_per_sample (pixbuf_cropped),
							    gdk_pixbuf_get_has_alpha (pixbuf_cropped),
							    HILDON_THUMBNAIL_PLUGIN_OUTTYPE_CROPPED,
							    mtime, 
							    uri, 
							    &nerror);

			g_object_unref (pixbuf_cropped);

			if (nerror)
				goto nerror_handler;

		}

		nerror_handler:

		if (stream)
			g_input_stream_close (G_INPUT_STREAM (stream), NULL, NULL);

		if (nerror || err_file) {
			if (!errors)
				errors = g_string_new ("");
			g_string_append_printf (errors, "[`%s': %s] ", 
						uri, nerror ? nerror->message:"Had error before");

			if (!err_file) {
				GFile *file;
				GFileInfo *info;
				file = g_file_new_for_uri (uri);
				info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED,
							  G_FILE_QUERY_INFO_NONE,
							  NULL, NULL);
				if (info) {
					guint64 mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
					hildon_thumbnail_outplugins_put_error (mtime, uri, nerror);
					g_object_unref (info);
				}

				g_object_unref (file);
			}

			failed = g_list_prepend (failed, g_strdup (uri));
			if (nerror)
				g_error_free (nerror);
			nerror = NULL;
		}

		if (stream)
			g_object_unref (stream);

		if (info)
			g_object_unref (info);
		if (file)
			g_object_unref (file);


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

		g_set_error (error, DEFAULT_ERROR, 0,
			     "%s", errors->str);

		g_string_free (errors, TRUE);
	}

	return;
}

gboolean 
hildon_thumbnail_plugin_stop (void)
{
	if (supported)
		g_strfreev (supported);
	if (monitor)
		g_object_unref (monitor);
	supported = NULL;
	return FALSE;
}

static void
reload_config (const gchar *config)
{
	GKeyFile *keyfile;
	GError *error = NULL;

	keyfile = g_key_file_new ();

	if (!g_key_file_load_from_file (keyfile, config, G_KEY_FILE_NONE, NULL)) {
		do_cropped = TRUE;
		g_key_file_free (keyfile);
		return;
	}

	do_cropped = g_key_file_get_boolean (keyfile, "Hildon Thumbnailer", "DoCropping", &error);

	if (error) {
		do_cropped = TRUE;
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

void 
hildon_thumbnail_plugin_init (gboolean *cropping, hildon_thumbnail_register_func func, gpointer thumbnailer, GModule *module, GError **error)
{
	gchar *config = g_build_filename (g_get_user_config_dir (), "hildon-thumbnailer", "gdkpixbuf-plugin.conf", NULL);
	GFile *file = g_file_new_for_path (config);
	guint i = 0;
	const gchar *uri_schemes[8] = { "file", "http", "https", 
					"smb", "nfs", "ftp", 
					"ftps", NULL };

	monitor =  g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);

	g_signal_connect (G_OBJECT (monitor), "changed", 
					  G_CALLBACK (on_file_changed), NULL);

	g_object_unref (file);

	reload_config (config);

	*cropping = do_cropped;

	if (func) {
		supported = (gchar **) hildon_thumbnail_plugin_supported ();
		if (supported) {
			while (supported[i] != NULL) {
				func (thumbnailer, supported[i], module, (const GStrv) uri_schemes, 0);
				i++;
			}
		}
	}

	g_free (config);

}
