/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
#include <gio/gio.h>
#include <glib/gfileutils.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "hildon-thumbnail-factory.h"
#include "thumbnailer-client.h"
#include "utils.h"

#define THUMBNAILER_SERVICE      "org.freedesktop.thumbnailer"
#define THUMBNAILER_PATH         "/org/freedesktop/thumbnailer/Generic"
#define THUMBNAILER_INTERFACE    "org.freedesktop.thumbnailer.Generic"

#define REQUEST_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HILDON_TYPE_THUMBNAIL_REQUEST, HildonThumbnailRequestPrivate))
#define FACTORY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HILDON_TYPE_THUMBNAIL_FACTORY, HildonThumbnailFactoryPrivate))

typedef struct {
	GStrv uris;
	gchar *key;
	gchar *paths[3];
	guint width, height;
	gboolean cropped;
	HildonThumbnailRequestCallback callback;
	GDestroyNotify destroy;
	HildonThumbnailFactory *factory;
	gpointer user_data;
} HildonThumbnailRequestPrivate;

typedef struct {
	DBusGProxy *proxy;
	GHashTable *tasks;
} HildonThumbnailFactoryPrivate;


#ifndef gdk_pixbuf_new_from_stream
/* It's implemented in pixbuf-io-loader.c in this case */
GdkPixbuf * gdk_pixbuf_new_from_stream (GInputStream  *stream,
			    GCancellable  *cancellable,
			    GError       **error);
#endif 


static void
create_pixbuf_and_callback (HildonThumbnailRequestPrivate *r_priv)
{
	GFile *filei = NULL;
	GInputStream *stream = NULL;
	GdkPixbuf *pixbuf = NULL;
	gchar *path;
	GError *error = NULL;

	/* Determine the exact type of thumbnail being requested */

	if (r_priv->cropped)
		path = r_priv->paths[2];
	else if (r_priv->width > 128)
		path = r_priv->paths[1];
	else
		path = r_priv->paths[0];

	/* Open the original thumbnail as a stream */
	filei = g_file_new_for_path (path);
	stream = G_INPUT_STREAM (g_file_read (filei, NULL, &error));

	if (error)
		goto error_handler;

	/* Read the stream as a pixbuf at the requested exact scale */
	pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream,
		r_priv->width, r_priv->height, TRUE, 
		NULL, &error);

	error_handler:

	/* Callback user function, passing the pixbuf and error */

	if (r_priv->callback)
		r_priv->callback (r_priv->factory, pixbuf, error, r_priv->user_data);

	if (r_priv->destroy)
		r_priv->destroy (r_priv->destroy);

	/* Cleanup */

	if (filei)
		g_object_unref (filei);

	if (stream) {
		g_input_stream_close (G_INPUT_STREAM (stream), NULL, NULL);
		g_object_unref (stream);
	}

	if (error)
		g_error_free (error);

	if (pixbuf)
		gdk_pixbuf_unref (pixbuf);
}


static void
on_task_finished (DBusGProxy *proxy,
		  guint       handle,
		  gpointer    user_data)
{
	HildonThumbnailFactory *self = user_data;
	HildonThumbnailFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (user_data);
	gchar *key = g_strdup_printf ("%d", handle);
	HildonThumbnailRequest *request = g_hash_table_lookup (f_priv->tasks, key);

	if (request) {
		HildonThumbnailRequestPrivate *r_priv = REQUEST_GET_PRIVATE (request);
		create_pixbuf_and_callback (r_priv);
		g_hash_table_remove (f_priv->tasks, key);
	}

	g_free (key);
}

static gboolean
have_all_for_request_cb (gpointer user_data)
{
	HildonThumbnailRequestPrivate *r_priv = REQUEST_GET_PRIVATE (user_data);
	create_pixbuf_and_callback (r_priv);
	return FALSE;
}

static void
hildon_thumbnail_factory_init (HildonThumbnailFactory *self)
{
	GError *error = NULL;
	DBusGConnection *connection;
	HildonThumbnailFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (self);

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (error) {
		g_critical ("Can't initialize HildonThumbnailer: %s\n", error->message);
		g_error_free (error);
		return;
	}

	f_priv->tasks = g_hash_table_new_full (g_str_hash, g_str_equal,
				       (GDestroyNotify) g_free,
				       (GDestroyNotify) g_object_unref);

	f_priv->proxy = dbus_g_proxy_new_for_name (connection, 
				   THUMBNAILER_SERVICE,
				   THUMBNAILER_PATH,
				   THUMBNAILER_INTERFACE);

	dbus_g_proxy_add_signal (f_priv->proxy, "Finished", 
				G_TYPE_UINT, G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (f_priv->proxy, "Finished",
			     G_CALLBACK (on_task_finished),
			     g_object_ref (self),
			     (GClosureNotify) g_object_unref);
}

static void
hildon_thumbnail_request_init (HildonThumbnailRequest *self)
{
	HildonThumbnailRequestPrivate *r_priv = REQUEST_GET_PRIVATE (self);

	r_priv->uris = NULL;
	r_priv->key = NULL;
	r_priv->cropped = FALSE;
	r_priv->paths[0] = NULL;
	r_priv->paths[1] = NULL;
	r_priv->paths[2] = NULL;
	r_priv->factory = NULL;
	r_priv->callback = NULL;
	r_priv->destroy = NULL;
	r_priv->user_data = NULL;
}


static void
hildon_thumbnail_factory_finalize (GObject *object)
{
	HildonThumbnailFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (object);

	g_object_unref (f_priv->proxy);
	g_hash_table_unref (f_priv->tasks);
}

static void
hildon_thumbnail_request_finalize (GObject *object)
{
	HildonThumbnailRequestPrivate *r_priv = REQUEST_GET_PRIVATE (object);
	guint i;

	for (i = 0; i < 3; i++)
		g_free (r_priv->paths[i]);
	if (r_priv->uris)
		g_strfreev (r_priv->uris);
	g_free (r_priv->key);
	if (r_priv->factory)
		g_object_unref (r_priv->factory);
}

static void
hildon_thumbnail_request_class_init (HildonThumbnailRequestClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = hildon_thumbnail_request_finalize;

	g_type_class_add_private (object_class, sizeof (HildonThumbnailRequestPrivate));
}

static void
hildon_thumbnail_factory_class_init (HildonThumbnailFactoryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = hildon_thumbnail_factory_finalize;

	g_type_class_add_private (object_class, sizeof (HildonThumbnailFactoryPrivate));
}

static void 
on_got_handle (DBusGProxy *proxy, guint OUT_handle, GError *error, gpointer userdata)
{
	HildonThumbnailRequest *request = userdata;
	HildonThumbnailRequestPrivate *r_priv = REQUEST_GET_PRIVATE (request);
	HildonThumbnailFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (r_priv->factory);

	gchar *key = g_strdup_printf ("%d", OUT_handle);
	r_priv->key = g_strdup (key);
	g_hash_table_replace (f_priv->tasks, key, 
			      g_object_ref (request));
	g_object_unref (request);
}

HildonThumbnailRequest*
hildon_thumbnail_factory_request (HildonThumbnailFactory *self,
				  const gchar *uri,
				  guint width, guint height,
				  gboolean cropped,
				  HildonThumbnailRequestCallback callback,
				  gpointer user_data,
				  GDestroyNotify destroy)
{
	HildonThumbnailRequest *request = g_object_new (HILDON_TYPE_THUMBNAIL_REQUEST, NULL);
	HildonThumbnailRequestPrivate *r_priv = REQUEST_GET_PRIVATE (request);
	HildonThumbnailFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (self);
	guint i;
	gboolean have = TRUE;

	hildon_thumbnail_util_get_thumb_paths (uri, &r_priv->paths[0], 
					       &r_priv->paths[1], 
					       &r_priv->paths[2]);

	for (i = 0; i< 3 && !have; i++)
		have = g_file_test (r_priv->paths[i], G_FILE_TEST_EXISTS);

	r_priv->uris = (GStrv) g_malloc0 (sizeof (gchar *) * 2);
	r_priv->uris[0] = g_strdup (uri);
	r_priv->factory = g_object_ref (self);
	r_priv->user_data = user_data;
	r_priv->callback = callback;
	r_priv->destroy = destroy;
	r_priv->cropped = cropped;

	if (!have) {
		org_freedesktop_thumbnailer_Generic_queue_async (f_priv->proxy, 
								 (const char **) r_priv->uris, 0, 
								 on_got_handle, 
								 g_object_ref (request));
	} else {
		g_idle_add_full (G_PRIORITY_DEFAULT, have_all_for_request_cb, 
				 (GSourceFunc) g_object_ref (request),
				 (GDestroyNotify) g_object_unref);
	}

	return request;
}

void 
hildon_thumbnail_factory_join (HildonThumbnailFactory *self)
{
	HildonThumbnailFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (self);

	while(g_hash_table_size (f_priv->tasks) != 0) {
		g_main_context_iteration (NULL, TRUE);
	}
}

static void 
on_unqueued (DBusGProxy *proxy, GError *error, gpointer userdata)
{
	HildonThumbnailRequest *request = userdata;
	HildonThumbnailRequestPrivate *r_priv = REQUEST_GET_PRIVATE (request);
	HildonThumbnailFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (r_priv->factory);

	g_hash_table_remove (f_priv->tasks, r_priv->key);
	g_object_unref (request);
}

void 
hildon_thumbnail_request_unqueue (HildonThumbnailRequest *self)
{
	HildonThumbnailRequestPrivate *r_priv = REQUEST_GET_PRIVATE (self);
	HildonThumbnailFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (r_priv->factory);
	guint handle = atoi (r_priv->key);

	org_freedesktop_thumbnailer_Generic_unqueue_async (f_priv->proxy, handle,
							   on_unqueued, 
							   g_object_ref (self));
}

void 
hildon_thumbnail_request_join (HildonThumbnailRequest *self)
{
	HildonThumbnailRequestPrivate *r_priv = REQUEST_GET_PRIVATE (self);
	HildonThumbnailFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (r_priv->factory);
	HildonThumbnailRequest *found = g_hash_table_lookup (f_priv->tasks, r_priv->key);

	while (found) {
		g_main_context_iteration (NULL, TRUE);
		found = g_hash_table_lookup (f_priv->tasks, r_priv->key);
	}
}

G_DEFINE_TYPE (HildonThumbnailFactory, hildon_thumbnail_factory, G_TYPE_OBJECT)

G_DEFINE_TYPE (HildonThumbnailRequest, hildon_thumbnail_request, G_TYPE_OBJECT)
