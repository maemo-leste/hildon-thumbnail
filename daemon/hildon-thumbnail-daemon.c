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

#include <glib.h>
#include <dbus/dbus-glib-bindings.h>

#include "hildon-thumbnail-plugin.h"

#include "generic.h"

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

		/* TODO: dynamically load plugins, and detect when new ones get
		 * dropped, and removed ones get removed (and therefore must
		 * shut down) */

		generic_do_init (connection, &error);

		module = hildon_thumbnail_plugin_load ("default");
		hildon_thumbnail_plugin_do_init (module, connection, &error);

		main_loop = g_main_loop_new (NULL, FALSE);
		g_main_loop_run (main_loop);

		hildon_thumbnail_plugin_do_stop (module);

		generic_do_stop ();

		g_main_loop_unref (main_loop);
	}


	return 0;
}
