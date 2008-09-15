/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#include <glib.h>
#include <dbus/dbus-glib-bindings.h>

#include "hildon-thumbnail-plugin.h"

#include "thumbnailer.h"
#include "manager.h"

static gboolean do_shut_down_next_time = TRUE;

void
keep_alive (void) 
{
	do_shut_down_next_time = FALSE;
}

static gboolean
shut_down_after_timeout (gpointer user_data)
{
	GMainLoop *main_loop =  user_data;
	gboolean shut = FALSE;

	if (do_shut_down_next_time) {
		g_main_loop_quit (main_loop);
		shut = TRUE;
	} else
		do_shut_down_next_time = TRUE;

	return shut;
}

int 
main (int argc, char **argv) 
{
	DBusGConnection *connection;
	GModule *module;
	GError *error = NULL;
	
	g_type_init ();

	if (!g_thread_supported ())
		g_thread_init (NULL);

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!connection)
		g_critical ("Could not connect to the DBus session bus, %s",
			    error ? error->message : "no error given.");
	else {
		GMainLoop *main_loop;
		GError *error = NULL;
		Manager *manager;
		Thumbnailer *thumbnailer;
		DBusGProxy *manager_proxy;
		guint y = 0;
		gboolean cropping;
		GDir *dir;
		const gchar *plugin;
		GHashTable *registrations;
		GHashTableIter iter;
		gpointer key, value;

		manager_do_init (connection, &manager, &error);
		thumbnailer_do_init (connection, manager, &thumbnailer, &error);

		manager_proxy = dbus_g_proxy_new_for_name (connection, 
					   MANAGER_SERVICE,
					   MANAGER_PATH,
					   MANAGER_INTERFACE);

		registrations = g_hash_table_new_full (g_str_hash, g_str_equal,
						       (GDestroyNotify) g_free, 
						       (GDestroyNotify) NULL);

		dir = g_dir_open (PLUGINS_DIR, 0, &error);

		if (dir) {
		  while ((plugin = g_dir_read_name (dir)) != NULL) {

			if (!g_str_has_suffix (plugin, "." G_MODULE_SUFFIX)) {
				continue;
			}
			
			module = hildon_thumbnail_plugin_load (plugin);
			hildon_thumbnail_plugin_do_init (module, &cropping,
							 (register_func) thumbnailer_register_plugin,
							 thumbnailer,
							 &error);
			g_hash_table_replace (registrations, g_strdup (plugin),
					      module);
			y++;
		  }
		  g_dir_close (dir);
		}

		main_loop = g_main_loop_new (NULL, FALSE);

		g_timeout_add_seconds (600, 
				       shut_down_after_timeout,
				       main_loop);

		g_main_loop_run (main_loop);

		g_hash_table_iter_init (&iter, registrations);

		while (g_hash_table_iter_next (&iter, &key, &value))  {
			thumbnailer_unregister_plugin (thumbnailer, value);
			hildon_thumbnail_plugin_do_stop (value);
		}

		g_hash_table_unref (registrations);

		thumbnailer_do_stop ();
		manager_do_stop ();

		g_main_loop_unref (main_loop);
	}


	return 0;
}
