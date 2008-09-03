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
#include <hildon-thumbnail-factory.h>

#include "default.h"
#include "hildon-thumbnail-plugin.h"

static const gchar* supported[2] = { "image/png" , NULL };


const gchar** 
hildon_thumbnail_plugin_supported (void)
{
	return supported;
}

void
hildon_thumbnail_plugin_create (GStrv uris, GError **error)
{
	guint i = 0;
	while (uris[i] != NULL) {
		g_print ("%s\n", uris[i]);
		i++;
	}
	return;
}

void 
hildon_thumbnail_plugin_stop (void)
{
}


void 
hildon_thumbnail_plugin_init (GError **error)
{
}
