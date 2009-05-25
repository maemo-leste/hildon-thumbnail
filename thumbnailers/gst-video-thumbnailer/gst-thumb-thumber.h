/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * This file is part of hildon-thumbnail package
 *
 * Copyright (C) 2009 Nokia Corporation.  All Rights reserved.
 *
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

#ifndef __GST_THUMB_THUMBER_H__
#define __GST_THUMB_THUMBER_H__

#include <glib-object.h>
#include <glib.h>
#include <gio/gio.h>

#define TYPE_THUMBER         (thumber_get_type())
#define THUMBER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_THUMBER, Thumber))
#define THUMBER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TYPE_THUMBER, ThumberClass))
#define THUMBER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_THUMBER, ThumberClass))

typedef struct Thumber Thumber;
typedef struct ThumberClass ThumberClass;

struct Thumber {
	GObject parent;
};

struct ThumberClass {
	GObjectClass parent;

	void (*finished) (Thumber *object, guint handle);
	void (*started) (Thumber *object, guint handle);
	void (*ready) (Thumber *object, GStrv uris);
	void (*error) (Thumber *object, guint handle, gchar *reason);
};

GType		thumber_get_type	            (void) G_GNUC_CONST;
Thumber        *thumber_new                         (void);
gboolean        thumber_dbus_register               (Thumber     *thumber,
						     const gchar *bus_name,
						     const gchar *bus_path,
						     GError     **error);
void            thumber_start                       (Thumber     *thumber);

#endif
