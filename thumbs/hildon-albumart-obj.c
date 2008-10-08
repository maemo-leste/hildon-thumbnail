/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * This file is part of hildon-albumart package
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

#include "hildon-albumart-factory.h"


#define REQUEST_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HILDON_TYPE_ALBUMART_REQUEST, HildonAlbumartRequestPrivate))
#define FACTORY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HILDON_TYPE_ALBUMART_FACTORY, HildonAlbumartFactoryPrivate))

typedef struct {

} HildonAlbumartRequestPrivate;

typedef struct {

} HildonAlbumartFactoryPrivate;

static void
hildon_albumart_factory_init (HildonAlbumartFactory *self)
{
}

static void
hildon_albumart_request_init (HildonAlbumartRequest *self)
{
}


static void
hildon_albumart_factory_finalize (GObject *object)
{
}

static void
hildon_albumart_request_finalize (GObject *object)
{
}

static void
hildon_albumart_request_class_init (HildonAlbumartRequestClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = hildon_albumart_request_finalize;

	g_type_class_add_private (object_class, sizeof (HildonAlbumartRequestPrivate));
}

static void
hildon_albumart_factory_class_init (HildonAlbumartFactoryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = hildon_albumart_factory_finalize;

	g_type_class_add_private (object_class, sizeof (HildonAlbumartFactoryPrivate));
}

HildonAlbumartRequest*
hildon_albumart_factory_queue (HildonAlbumartFactory *self,
			       const gchar *artist, const gchar *album, const gchar *uri,
			       HildonAlbumartRequestCallback callback,
			       gpointer user_data,
			       GDestroyNotify destroy)
{
	HildonAlbumartRequest *request = g_object_new (HILDON_TYPE_ALBUMART_REQUEST, NULL);
	return request;
}

void 
hildon_albumart_factory_join (HildonAlbumartFactory *self)
{
}

void 
hildon_albumart_request_unqueue (HildonAlbumartRequest *self)
{
}

void 
hildon_albumart_request_join (HildonAlbumartRequest *self)
{
}

G_DEFINE_TYPE (HildonAlbumartFactory, hildon_albumart_factory, G_TYPE_OBJECT)

G_DEFINE_TYPE (HildonAlbumartRequest, hildon_albumart_request, G_TYPE_OBJECT)
