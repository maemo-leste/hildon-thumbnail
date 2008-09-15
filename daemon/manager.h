#ifndef __MANAGER_H__
#define __MANAGER_H__

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

#define MANAGER_SERVICE      "org.freedesktop.thumbnailer"
#define MANAGER_PATH         "/org/freedesktop/thumbnailer/Manager"
#define MANAGER_INTERFACE    "org.freedesktop.thumbnailer.Manager"

#define TYPE_MANAGER             (manager_get_type())
#define MANAGER(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_MANAGER, Manager))
#define MANAGER_CLASS(c)         (G_TYPE_CHECK_CLASS_CAST ((c), TYPE_MANAGER, ManagerClass))
#define MANAGER_GET_CLASS(o)     (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_MANAGER, ManagerClass))

typedef struct Manager Manager;
typedef struct ManagerClass ManagerClass;

struct Manager {
	GObject parent;
};

struct ManagerClass {
	GObjectClass parent;
};

GType manager_get_type (void);

void manager_register (Manager *object, gchar *mime_type, DBusGMethodInvocation *context);
void manager_get_supported (Manager *object, DBusGMethodInvocation *context);

void manager_i_have (Manager *object, const gchar *mime_type);
DBusGProxy* manager_get_handler (Manager *object, const gchar *mime_type);

void manager_do_stop (void);
void manager_do_init (DBusGConnection *connection, Manager **manager, GError **error);


#endif
