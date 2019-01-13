/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Copyright (C) 2009 Nokia

 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <stdlib.h>
#include <gio/gio.h>

#include "gst-thumb-thumber.h"

#define DEFAULT_BUS_NAME       "com.nokia.thumbnailer.Gstreamer"
#define DEFAULT_BUS_PATH       "/com/nokia/thumbnailer/Gstreamer"

#define MANAGER_SERVICE        "org.freedesktop.thumbnailer"
#define MANAGER_PATH           "/org/freedesktop/thumbnailer/Manager"
#define MANAGER_INTERFACE      "org.freedesktop.thumbnailer.Manager"

static GMainLoop       *main_loop  = NULL;

static gboolean dynamic_register = FALSE;
static gchar   *bus_name;
static gchar   *bus_path;
static gboolean standard;
static gboolean cropped;
static gint     timeout = 600;

static GOptionEntry  config_entries[] = {
	{ "timeout", 't', 0, 
	  G_OPTION_ARG_INT, &timeout, 
	  "Timeout before the thumbnailer shuts down. ", 
	  NULL },
	{ "bus-name", 'b', 0, 
	  G_OPTION_ARG_STRING, &bus_name, 
	  "Busname to use (eg. com.company.Thumbnailer) ", 
	  NULL },
	{ "bus-path", 'p', 0, 
	  G_OPTION_ARG_STRING, &bus_path, 
	  "Buspath to use (eg. /com/company/Thumbnailer) ", 
	  NULL },
	{ "cropped", 'c', 0, 
	  G_OPTION_ARG_NONE, &cropped, 
	  "Disable cropped thumbnails ", 
	  NULL },
	{ "standard", 's', 0, 
	  G_OPTION_ARG_NONE, &standard, 
	  "Enable standard (normal/large) thumbnails ", 
	  NULL },
	{ "dynamic-register", 'd', 0, 
	  G_OPTION_ARG_NONE, &dynamic_register, 
	  "Dynamic registration using org.freedesktop.Thumbnailer.Manager", 
	  NULL },
  { NULL }
};

static void
mount_pre_unmount_cb (GVolumeMonitor *volume_monitor,
		      GMount         *mount,
		      Thumber        *thumber)
{
	/* FIXME We should instead cancel offending tasks and handle gracefully */

	exit (0);
}

gboolean
gst_thumb_main_quit (void)
{
	g_main_loop_quit (main_loop);
	return FALSE;
}

gint
main (gint argc, gchar *argv[])
{
	GError          *error          = NULL;
	GValue           val            = {0, };
	GOptionContext  *context        = NULL;
	GOptionGroup    *group          = NULL;
	Thumber         *thumber        = NULL;
	GVolumeMonitor  *volume_monitor = NULL;

	g_type_init ();

	if (!g_thread_supported ()) {
		g_thread_init (NULL);
	}

	context = g_option_context_new (" - Gstreamer based specialized video thumbnailer");

	group = g_option_group_new ("thumbnailer", 
				    "Thumbnailer Options",
				    "Show thumbnailer options", 
				    NULL, 
				    NULL);
	g_option_group_add_entries (group, config_entries);
	g_option_context_add_group (context, group);

	g_option_context_parse (context, &argc, &argv, &error);
	g_option_context_free (context);

	if (error) {
		g_printerr ("Invalid arguments, %s\n", error->message);
		g_error_free (error);
		return -1;
	}

	if (!bus_name) {
		bus_name = DEFAULT_BUS_NAME;
	}

	if (!bus_path) {
		bus_path = DEFAULT_BUS_PATH;
	}

  	g_print ("Initializing gstreamer video thumbnailer...\n");

	thumber = thumber_new ();

	thumber_dbus_register (thumber, bus_name, bus_path, &error);

	g_value_init (&val, G_TYPE_INT);
	g_value_set_int (&val, timeout);
	g_object_set_property (G_OBJECT(thumber), "timeout", &val);
	g_value_unset (&val);

	g_value_init (&val, G_TYPE_BOOLEAN);
	g_value_set_boolean (&val, standard);
	g_object_set_property (G_OBJECT(thumber), "standard", &val);
	g_value_unset (&val);
	
	g_value_init (&val, G_TYPE_BOOLEAN);
	g_value_set_boolean (&val, !cropped);
	g_object_set_property (G_OBJECT(thumber), "cropped", &val);
	g_value_unset (&val);

	/* Create volume monitor */
	volume_monitor = g_volume_monitor_get ();
	g_signal_connect (volume_monitor, "mount-pre-unmount",
			  G_CALLBACK (mount_pre_unmount_cb), thumber);
	
	g_message ("Starting...");
	thumber_start (thumber);

	main_loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (main_loop);

	g_message ("Shutting down...");

	/* Shut down volume monitor */
	g_signal_handlers_disconnect_by_func (volume_monitor,
					      mount_pre_unmount_cb,
					      thumber);
	g_object_unref (volume_monitor);

	g_object_unref (thumber);

	g_main_loop_unref (main_loop);

	g_message ("\nDone\n\n");

	return 0;
}
