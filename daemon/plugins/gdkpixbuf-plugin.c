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
#include <dbus/dbus-glib-bindings.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-io.h>


#define DEFAULT_ERROR_DOMAIN	"HildonThumbnailerGdkPixbuf"
#define DEFAULT_ERROR		g_quark_from_static_string (DEFAULT_ERROR_DOMAIN)

#include "utils.h"
#include "gdkpixbuf-plugin.h"
#include "hildon-thumbnail-plugin.h"

static gchar **supported = NULL;

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
		g_slist_free (formats);
	}

	return (const gchar**) supported;
}

#define HILDON_THUMBNAIL_OPTION_PREFIX "tEXt::Thumb::"
#define HILDON_THUMBNAIL_APPLICATION "hildon-thumbnail"
#define URI_OPTION HILDON_THUMBNAIL_OPTION_PREFIX "URI"
#define MTIME_OPTION HILDON_THUMBNAIL_OPTION_PREFIX "MTime"
#define SOFTWARE_OPTION "tEXt::Software"
#define META_OPTION HILDON_THUMBNAIL_OPTION_PREFIX "Meta"


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
		guint64 mtime;
		gchar *large = NULL, *normal = NULL;

		//g_print ("%s\n", uri);

		hildon_thumbnail_util_get_thumb_paths (uri, &large, &normal, &nerror);

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

		g_input_stream_close (G_INPUT_STREAM (stream), NULL, NULL);

		if (nerror)
			goto nerror_handler;

		save_thumb_file_meta (pixbuf_normal, normal, mtime, uri, &nerror);


		nerror_handler:

		if (nerror) {
			if (!errors)
				errors = g_string_new ("");
			g_string_append_printf (errors, "[`%s': %s] ", 
						uri,
						nerror->message);
		}

		if (stream)
			g_object_unref (stream);

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
	supported = NULL;
}


void 
hildon_thumbnail_plugin_init (GError **error)
{
}
