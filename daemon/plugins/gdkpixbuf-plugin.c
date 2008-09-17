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
#include "hildon-thumbnail-plugin.h"

#ifndef gdk_pixbuf_new_from_stream_at_scale
/* It's implemented in pixbuf-io-loader.c in this case */
GdkPixbuf* gdk_pixbuf_new_from_stream_at_scale (GInputStream *stream, gint width,
			gint height, gboolean preserve_aspect_ratio,
			GCancellable *cancellable, GError **error);
#endif

#ifndef gdk_pixbuf_new_from_stream
/* It's implemented in pixbuf-io-loader.c in this case */
GdkPixbuf * gdk_pixbuf_new_from_stream (GInputStream  *stream,
			    GCancellable  *cancellable,
			    GError       **error);
#endif 

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
		supported = (gchar **) g_malloc0 (sizeof (gchar *) * (types_support->len + 1));
		for (i = 0 ; i < types_support->len; i++)
			supported[i] =  g_strdup (g_ptr_array_index (types_support, i));
		g_ptr_array_free (types_support, TRUE);
		g_slist_free (formats);
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

void
hildon_thumbnail_plugin_create (GStrv uris, GError **error)
{
	guint i = 0;
	GString *errors = NULL;

	while (uris[i] != NULL) {
		GError *nerror = NULL;
		GFileInfo *info;
		GFile *file;
		GFileInputStream *stream;
		gchar *uri = uris[i];
		GdkPixbuf *pixbuf_large;
		GdkPixbuf *pixbuf_normal;
		GdkPixbuf *pixbuf, *pixbuf_cropped;
		guint64 mtime;
		gchar *large = NULL, 
		      *normal = NULL, 
		      *cropped = NULL;
		gboolean just_crop;

		//g_print ("%s\n", uri);

		hildon_thumbnail_util_get_thumb_paths (uri, &large, &normal, &cropped, &nerror);


		if (nerror)
			goto nerror_handler;

		just_crop = (g_file_test (large, G_FILE_TEST_EXISTS) && 
			     g_file_test (normal, G_FILE_TEST_EXISTS) && 
			     !g_file_test (cropped, G_FILE_TEST_EXISTS));

		if (just_crop && !do_cropped) {
			g_free (cropped);
			g_free (normal);
			g_free (large);
			continue;
		}

		//g_print ("L %s\n", large);
		//g_print ("N %s\n", normal);

		if (nerror)
			goto nerror_handler;

		file = g_file_new_for_uri (uri);

		info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED,
					  G_FILE_QUERY_INFO_NONE,
					  NULL, &nerror);

		if (nerror)
			goto nerror_handler;

		mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

		stream = g_file_read (file, NULL, &nerror);

		if (nerror)
			goto nerror_handler;

		pixbuf_large = gdk_pixbuf_new_from_stream_at_scale (G_INPUT_STREAM (stream),
								    256, 256,
								    TRUE,
								    NULL,
								    &nerror);

		if (nerror)
			goto nerror_handler;

		save_thumb_file_meta (pixbuf_large, large, mtime, uri, &nerror);

		g_object_unref (pixbuf_large);

		if (nerror)
			goto nerror_handler;

		g_seekable_seek (G_SEEKABLE (stream), 0, G_SEEK_SET, NULL, &nerror);

		if (nerror)
			goto nerror_handler;

		pixbuf_normal = gdk_pixbuf_new_from_stream_at_scale (G_INPUT_STREAM (stream),
								     128, 128,
								     TRUE,
								     NULL,
								     &nerror);

		if (nerror)
			goto nerror_handler;

		save_thumb_file_meta (pixbuf_normal, normal, mtime, uri, &nerror);

		g_object_unref (pixbuf_normal);

		if (nerror)
			goto nerror_handler;

		g_seekable_seek (G_SEEKABLE (stream), 0, G_SEEK_SET, NULL, &nerror);

		if (nerror)
			goto nerror_handler;

		pixbuf = gdk_pixbuf_new_from_stream (G_INPUT_STREAM (stream),
						     NULL,
						     &nerror);

		if (nerror)
			goto nerror_handler;

		pixbuf_cropped = crop_resize (pixbuf, 124, 124);

		g_object_unref (pixbuf);

		save_thumb_file_cropped (pixbuf_cropped, cropped, mtime, uri, &nerror);

		g_object_unref (pixbuf_cropped);

		nerror_handler:

		if (stream)
			g_input_stream_close (G_INPUT_STREAM (stream), NULL, NULL);

		if (nerror) {
			if (!errors)
				errors = g_string_new ("");
			g_string_append_printf (errors, "[`%s': %s] ", 
						uri,
						nerror->message);
		}

		if (stream)
			g_object_unref (stream);

		if (info)
			g_object_unref (info);
		if (file)
			g_object_unref (file);

		g_free (large);
		g_free (normal);
		g_free (cropped);

		i++;
	}

	if (errors) {
		g_set_error (error, DEFAULT_ERROR, 0,
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
	if (monitor)
		g_object_unref (monitor);
	supported = NULL;
}

static void
reload_config (const gchar *config)
{
	GKeyFile *keyfile;

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
	gchar *config = g_build_filename (g_get_user_config_dir (), "hildon-thumbnailer", "gdkpixbuf-plugin.conf", NULL);
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
				func (thumbnailer, supported[i], module);
				i++;
			}
		}
	}

	g_free (config);

}
