/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

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

G_BEGIN_DECLS

typedef enum {
	HILDON_THUMBNAIL_PLUGIN_OUTTYPE_LARGE,
	HILDON_THUMBNAIL_PLUGIN_OUTTYPE_NORMAL,
	HILDON_THUMBNAIL_PLUGIN_OUTTYPE_CROPPED,
} HildonThumbnailPluginOutType;

typedef void (*hildon_thumbnail_register_func)    (gpointer self, 
						   const gchar *mime_type, 
						   GModule *module, 
						   const GStrv uri_schemes, 
						   gint priority);

GModule *   hildon_thumbnail_plugin_load          (const gchar *module_name);
GStrv       hildon_thumbnail_plugin_get_supported (GModule *module);
void        hildon_thumbnail_plugin_do_init       (GModule *module, 
						   gboolean *cropping,
						   hildon_thumbnail_register_func func,
						   gpointer self,
						   GError **error);
void        hildon_thumbnail_plugin_do_create     (GModule *module, 
						   GStrv uris, 
						   gchar *mime_hint,
						   GStrv *failed_uris, 
						   GError **error);
void        hildon_thumbnail_plugin_do_stop       (GModule *module);


GModule*    hildon_thumbnail_outplugin_load       (const gchar *module_name);
void        hildon_thumbnail_outplugin_unload     (GModule *module);
void        hildon_thumbnail_outplugins_do_out    (const guchar *rgb8_pixmap, 
						   guint width, guint height,
						   guint rowstride, guint bits_per_sample,
						   gboolean has_alpha,
						   HildonThumbnailPluginOutType type,
						   guint64 mtime, 
						   const gchar *uri, 
						   GError **error);
gboolean    hildon_thumbnail_outplugins_needs_out (HildonThumbnailPluginOutType type,
						   guint64 mtime, 
						   const gchar *uri);
gchar *     hildon_thumbnail_outplugins_get_orig  (const gchar *path);

void        hildon_thumbnail_outplugins_cleanup   (const gchar *uri_match, 
						   guint since);


G_END_DECLS

#endif
