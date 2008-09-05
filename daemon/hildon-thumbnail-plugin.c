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

#include "hildon-thumbnail-plugin.h"


GModule *
hildon_thumbnail_plugin_load (const gchar *module_name)
{
	gchar *full_name, *path;
	GModule *module;

	g_return_val_if_fail (module_name != NULL, NULL);

	full_name = g_strdup_printf ("libhildon-thumbnailer-%s", module_name);
	path = g_build_filename (PLUGINS_DIR, full_name, NULL);

	module = g_module_open (path, G_MODULE_BIND_LOCAL);

	if (!module) {
		g_warning ("Could not load thumbnailer module '%s', %s\n", 
			   module_name, 
			   g_module_error ());
	} else {
		g_module_make_resident (module);
	}

	g_free (full_name);
	g_free (path);

	return module;
}

typedef GStrv (*SupportedFunc) (void);

GStrv
hildon_thumbnail_plugin_get_supported (GModule *module) 
{
	GStrv supported = NULL;
	SupportedFunc supported_func;

	if (g_module_symbol (module, "hildon_thumbnail_plugin_supported", (gpointer *) &supported_func)) {
		supported = (supported_func) ();
	}

	return supported;
}

typedef void (*InitFunc) (GError **error);

void
hildon_thumbnail_plugin_do_init (GModule *module, GError **error)
{
	InitFunc func;

	if (g_module_symbol (module, "hildon_thumbnail_plugin_init", (gpointer *) &func)) {
		(func) (error);
	}
}

typedef void (*CreateFunc) (GStrv uris, GError **error);

void 
hildon_thumbnail_plugin_do_create (GModule *module, GStrv uris, GError **error)
{
	CreateFunc func;
	if (g_module_symbol (module, "hildon_thumbnail_plugin_create", (gpointer *) &func)) {
		(func) (uris, error);
	}
}

typedef void (*StopFunc) (void);

void
hildon_thumbnail_plugin_do_stop (GModule *module)
{
	StopFunc func;

	if (g_module_symbol (module, "hildon_thumbnail_plugin_stop", (gpointer *) &func)) {
		(func) ();
	}
}
