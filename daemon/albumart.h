#ifndef __ALBUMART_H__
#define __ALBUMART_H__

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

#include "albumart-manager.h"

#define ALBUMART_SERVICE         "com.nokia.albumart"
#define ALBUMART_PATH            "/com/nokia/albumart/Requester"
#define ALBUMART_INTERFACE       "com.nokia.albumart.Requester"
#define PROVIDER_INTERFACE       "com.nokia.albumart.Provider"

#define TYPE_ALBUMART             (albumart_get_type())
#define ALBUMART(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_ALBUMART, Albumart))
#define ALBUMART_CLASS(c)         (G_TYPE_CHECK_CLASS_CAST ((c), TYPE_ALBUMART, AlbumartClass))
#define ALBUMART_GET_CLASS(o)     (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_ALBUMART, AlbumartClass))

typedef struct Albumart Albumart;
typedef struct AlbumartClass AlbumartClass;

struct Albumart {
	GObject parent;
};

struct AlbumartClass {
	GObjectClass parent;

	void (*finished) (Albumart *object, guint handle);
	void (*started) (Albumart *object, guint handle);
	void (*ready) (Albumart *object, gchar *albumartist, gchar *path);
	void (*error) (Albumart *object, guint handle, gchar *reason);
};

GType albumart_get_type (void);

void albumart_queue (Albumart *object, gchar *artist_or_title, gchar *album, gchar *kind, guint handle_to_unqueue, DBusGMethodInvocation *context);
void albumart_unqueue (Albumart *object, guint handle, DBusGMethodInvocation *context);
void albumart_delete (Albumart *object, gchar *artist_or_title, gchar *album, gchar *kind, DBusGMethodInvocation *context);

void albumart_do_stop (void);
void albumart_do_init (DBusGConnection *connection, AlbumartManager *manager, Albumart **albumart, GError **error);

#endif
