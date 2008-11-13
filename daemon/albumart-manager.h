/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

#ifndef __ALBUMART_MANAGER_H__
#define __ALBUMART_MANAGER_H__

/*
 * This file is part of hildon-albumart package
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

#define TYPE_ALBUMART_MANAGER             (albumart_manager_get_type())
#define ALBUMART_MANAGER(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_ALBUMART_MANAGER, AlbumartManager))
#define ALBUMART_MANAGER_CLASS(c)         (G_TYPE_CHECK_CLASS_CAST ((c), TYPE_ALBUMART_MANAGER, AlbumartManagerClass))
#define ALBUMART_MANAGER_GET_CLASS(o)     (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_ALBUMART_MANAGER, AlbumartManagerClass))

typedef struct AlbumartManager AlbumartManager;
typedef struct AlbumartManagerClass AlbumartManagerClass;

struct AlbumartManager {
	GObject parent;
};

struct AlbumartManagerClass {
	GObjectClass parent;
};

GType albumart_manager_get_type (void);

GList* albumart_manager_get_handlers (AlbumartManager *object);

void albumart_manager_do_stop (void);
void albumart_manager_do_init (DBusGConnection *connection, AlbumartManager **albumart_manager, GError **error);


#endif
