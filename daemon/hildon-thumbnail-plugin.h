#ifndef __HILDON_THUMBNAIL_PLUGIN_H__
#define __HILDON_THUMBNAIL_PLUGIN_H__

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
#include <gmodule.h>
#include <dbus/dbus-glib-bindings.h>

#include "thumbnailer.h"

G_BEGIN_DECLS

typedef void (*create_cb) (GStrv thumb_paths, GError *error, gpointer user_data);

GModule * hildon_thumbnail_plugin_load      (const gchar *module_name);

void      hildon_thumbnail_plugin_do_init   (GModule *module, 
					     Thumbnailer *thumbnailer, 
					     GError **error);
void      hildon_thumbnail_plugin_do_create (GModule *module, GStrv uris, 
					     create_cb callback, 
					     gpointer user_data);
void      hildon_thumbnail_plugin_do_stop   (GModule *module,
					     Thumbnailer *thumbnailer);

G_END_DECLS

#endif
