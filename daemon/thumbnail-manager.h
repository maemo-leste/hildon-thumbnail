#ifndef __THUMBNAIL_MANAGER_H__
#define __THUMBNAIL_MANAGER_H__

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

#define MANAGER_SERVICE      "org.freedesktop.thumbnailer"
#define MANAGER_PATH         "/org/freedesktop/thumbnailer/Manager"
#define MANAGER_INTERFACE    "org.freedesktop.thumbnailer.Manager"

#define TYPE_THUMBNAIL_MANAGER             (thumbnail_manager_get_type())
#define THUMBNAIL_MANAGER(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_THUMBNAIL_MANAGER, ThumbnailManager))
#define THUMBNAIL_MANAGER_CLASS(c)         (G_TYPE_CHECK_CLASS_CAST ((c), TYPE_THUMBNAIL_MANAGER, ThumbnailManagerClass))
#define THUMBNAIL_MANAGER_GET_CLASS(o)     (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_THUMBNAIL_MANAGER, ThumbnailManagerClass))

typedef struct ThumbnailManager ThumbnailManager;
typedef struct ThumbnailManagerClass ThumbnailManagerClass;

struct ThumbnailManager {
	GObject parent;
};

struct ThumbnailManagerClass {
	GObjectClass parent;
};

GType thumbnail_manager_get_type (void);

#define manager_register thumbnail_manager_register
#define manager_get_supported thumbnail_manager_get_supported

void thumbnail_manager_register (ThumbnailManager *object, gchar *uri_scheme, gchar *mime_type, DBusGMethodInvocation *context);
void thumbnail_manager_get_supported (ThumbnailManager *object, DBusGMethodInvocation *context);

void thumbnail_manager_i_have (ThumbnailManager *object, const gchar *mime_type);
DBusGProxy* thumbnail_manager_get_handler (ThumbnailManager *object, const gchar *uri_scheme, const gchar *mime_type);

void thumbnail_manager_do_stop (void);
void thumbnail_manager_do_init (DBusGConnection *connection, ThumbnailManager **thumbnail_manager, GError **error);


#endif
