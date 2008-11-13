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

#include "utils.h"
#include "hildon-thumbnail-plugin.h"

static gboolean had_init = FALSE;
static gboolean is_active = TRUE;

void
hildon_thumbnail_outplugin_out (const guchar *rgb8_pixmap, 
						guint width, guint height,
						guint rowstride,
						OutType type,
						guint64 mtime, 
						const gchar *uri, 
						GError **error)
{
	GdkPixbuf *pixbuf;
	gchar *large, *normal, *cropped, *filen;

	hildon_thumbnail_util_get_thumb_paths (uri, &large, &normal, &cropped,
											   NULL, NULL, NULL, FALSE);

	switch (type) {
		case OUTTYPE_LARGE:
			filen = large;
		break;
		case OUTTYPE_NORMAL:
			filen = normal;
		break;
		case OUTTYPE_CROPPED:
			filen = cropped;
		break;
	}

	pixbuf = gdk_pixbuf_new_from_data ((const guchar*) rgb8_pixmap, 
									   GDK_COLORSPACE_RGB, FALSE, 
									   8, width, height, rowstride,
									   NULL, NULL);

	gdk_pixbuf_save (pixbuf, filen, "jpeg", 
					 error, NULL);

	g_object_unref (pixbuf);

	g_free (normal);
	g_free (large);
	g_free (cropped);

	return;
}



static void
reload_config (const gchar *config) 
{
	GKeyFile *keyfile;
	GStrv mimetypes;
	guint i = 0, length;
	GError *error = NULL;

	keyfile = g_key_file_new ();

	if (!g_key_file_load_from_file (keyfile, config, G_KEY_FILE_NONE, NULL)) {
		is_active = TRUE;
		g_key_file_free (keyfile);
		return;
	}

	is_active = g_key_file_get_boolean (keyfile, "Hildon Thumbnailer", "IsActive", &error);

	if (error) {
		is_active = TRUE;
		g_error_free (error);
	}

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

gboolean
hildon_thumbnail_outplugin_is_active (void) 
{
	if (!had_init) {
		gchar *config = g_build_filename (g_get_user_config_dir (), "hildon-thumbnailer", "gdkpixbuf-output-plugin.conf", NULL);
		GFile *file = g_file_new_for_path (config);
		GFileMonitor *monitor;

		monitor =  g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);

		g_signal_connect (G_OBJECT (monitor), "changed", 
						  G_CALLBACK (on_file_changed), NULL);

		/* g_object_unref (monitor); */
		g_object_unref (file);

		reload_config (config);

		had_init = TRUE;
	}

	return is_active;
}
