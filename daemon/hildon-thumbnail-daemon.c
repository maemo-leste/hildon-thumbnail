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
		GDir        *dir;
		const gchar *plugin;

		manager_do_init (connection, &manager, &error);
		thumbnailer_do_init (connection, manager, &thumbnailer, &error);

		manager_proxy = dbus_g_proxy_new_for_name (connection, 
					   MANAGER_SERVICE,
					   MANAGER_PATH,
					   MANAGER_INTERFACE);

		dir = g_dir_open (PLUGINS_DIR, 0, &error);

		if (!dir) {
			g_error ("Error opening modules directory: %s", error->message);
			g_error_free (error);
			return;
		}

		while ((plugin = g_dir_read_name (dir)) != NULL) {

			if (!g_str_has_suffix (plugin, "." G_MODULE_SUFFIX)) {
				continue;
			}
			
			module = hildon_thumbnail_plugin_load (plugin);

			hildon_thumbnail_plugin_do_init (module, &cropping,
							 (register_func) thumbnailer_register_plugin,
							 thumbnailer,
							 &error);
			y++;
		}

		g_dir_close (dir);

		main_loop = g_main_loop_new (NULL, FALSE);
		g_main_loop_run (main_loop);

		thumbnailer_unregister_plugin (thumbnailer, module);
		hildon_thumbnail_plugin_do_stop (module);

		manager_do_stop ();
		thumbnailer_do_stop ();

		g_main_loop_unref (main_loop);
	}


	return 0;
}
