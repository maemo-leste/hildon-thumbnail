#ifndef __THUMBNAILER_H__
#define __THUMBNAILER_H__

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

#include <gmodule.h>

#include "thumbnail-manager.h"

#define THUMBNAILER_SERVICE      "org.freedesktop.thumbnailer"
#define THUMBNAILER_PATH         "/org/freedesktop/thumbnailer/Generic"
#define THUMBNAILER_INTERFACE    "org.freedesktop.thumbnailer.Generic"
#define SPECIALIZED_INTERFACE    "org.freedesktop.thumbnailer.Thumbnailer"

#define TYPE_THUMBNAILER             (thumbnailer_get_type())
#define THUMBNAILER(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_THUMBNAILER, Thumbnailer))
#define THUMBNAILER_CLASS(c)         (G_TYPE_CHECK_CLASS_CAST ((c), TYPE_THUMBNAILER, ThumbnailerClass))
#define THUMBNAILER_GET_CLASS(o)     (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_THUMBNAILER, ThumbnailerClass))

typedef struct Thumbnailer Thumbnailer;
typedef struct ThumbnailerClass ThumbnailerClass;

struct Thumbnailer {
	GObject parent;
};

struct ThumbnailerClass {
	GObjectClass parent;

	void (*finished) (Thumbnailer *object, guint handle);
	void (*started) (Thumbnailer *object, guint handle);
	void (*ready) (Thumbnailer *object, GStrv uris);
	void (*error) (Thumbnailer *object, guint handle, gchar *reason);
};

GType thumbnailer_get_type (void);

void thumbnailer_queue (Thumbnailer *object, GStrv urls, GStrv mime_hints, guint handle_to_unqueue, DBusGMethodInvocation *context);
void thumbnailer_unqueue (Thumbnailer *object, guint handle, DBusGMethodInvocation *context);
void thumbnailer_move (Thumbnailer *object, GStrv from_urls, GStrv to_urls, DBusGMethodInvocation *context);
void thumbnailer_copy (Thumbnailer *object, GStrv from_urls, GStrv to_urls, DBusGMethodInvocation *context);
void thumbnailer_delete (Thumbnailer *object, GStrv urls, DBusGMethodInvocation *context);

void thumbnailer_register_plugin (Thumbnailer *object, const gchar *mime_type, GModule *plugin, gboolean overwrite);
void thumbnailer_unregister_plugin (Thumbnailer *object, GModule *plugin);

void thumbnailer_do_stop (void);
void thumbnailer_do_init (DBusGConnection *connection, ThumbnailManager *manager, Thumbnailer **thumbnailer, GError **error);

#endif
