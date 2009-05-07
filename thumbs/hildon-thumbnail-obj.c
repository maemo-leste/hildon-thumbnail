/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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
#include <gio/gio.h>
#include <glib/gfileutils.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <stdlib.h>

#include "hildon-thumbnail-factory.h"
#include "thumbnailer-client.h"
#include "thumbnailer-marshal.h"
#include "utils.h"

#define THUMBNAILER_SERVICE      "org.freedesktop.thumbnailer"
#define THUMBNAILER_PATH         "/org/freedesktop/thumbnailer/Generic"
#define THUMBNAILER_INTERFACE    "org.freedesktop.thumbnailer.Generic"

#define REQUEST_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HILDON_TYPE_THUMBNAIL_REQUEST, HildonThumbnailRequestPrivate))
#define FACTORY_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), HILDON_TYPE_THUMBNAIL_FACTORY, HildonThumbnailFactoryPrivate))

typedef struct {
	GStrv uris;
	gchar *key;
	guint width, height;
	gboolean cropped;
	HildonThumbnailRequestPixbufCallback pcallback;
	HildonThumbnailRequestUriCallback ucallback;
	GDestroyNotify destroy;
	HildonThumbnailFactory *factory;
	gpointer user_data;
	GString *errors;
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

#ifndef gdk_pixbuf_new_from_stream_at_scale
/* It's implemented in pixbuf-io-loader.c in this case */
GdkPixbuf *
gdk_pixbuf_new_from_stream_at_scale (GInputStream  *stream,
				     gint	    width,
				     gint 	    height,
				     gboolean       preserve_aspect_ratio,
				     GCancellable  *cancellable,
		  	    	     GError       **error);
#endif

#define HILDON_THUMBNAIL_APPLICATION "hildon-thumbnail"
#define FACTORY_ERROR g_quark_from_static_string (HILDON_THUMBNAIL_APPLICATION)

static void
create_pixbuf_and_callback (HildonThumbnailRequestPrivate *r_priv)
{
	GFile *filei = NULL, *local = NULL;
	GInputStream *stream = NULL;
	GdkPixbuf *pixbuf = NULL;
	GError *error = NULL;
	gboolean err_d = FALSE;
	gboolean have = FALSE;
	guint y, i, x;

	gchar *paths[3] = { NULL, NULL, NULL };
	gchar *lpaths[3] = { NULL, NULL, NULL };

	for (y = 0 ; y < 2 && !have; y++ ) {

		have = TRUE;

		for (x = 0; x < 3; x++) {
			g_free (paths[x]); 
			paths[x] = NULL;
			g_free (lpaths[x]); 
			lpaths[x] = NULL;
		}

		hildon_thumbnail_util_get_thumb_paths (r_priv->uris[0], 
						       &paths[0], 
						       &paths[1], 
						       &paths[2],
						       &lpaths[0],
						       &lpaths[1],
						       &lpaths[2], 
						       (y==0));

		for (i = 0; i < 3 && have; i++) {
			gchar *localp = g_filename_from_uri (lpaths[i], NULL, NULL);
			have = (g_file_test (paths[i], G_FILE_TEST_EXISTS) || 
				g_file_test (localp, G_FILE_TEST_EXISTS));
			g_free (localp);
		}
	}

	if (r_priv->cropped) {
		GFile *temp = g_file_new_for_uri (lpaths[2]);
		if (!g_file_query_exists (temp, NULL)) {
			filei = g_file_new_for_path (paths[2]);
			g_object_unref (temp);
		} else
			filei = temp;
	} else if (r_priv->width > 128 || r_priv->height > 128) {
		GFile *temp = g_file_new_for_uri (lpaths[0]);
		if (!g_file_query_exists (temp, NULL)) {
			filei = g_file_new_for_path (paths[0]);
			g_object_unref (temp);
		} else
			filei = temp;
	} else {
		GFile *temp = g_file_new_for_uri (lpaths[1]);
		if (!g_file_query_exists (temp, NULL)) {
			filei = g_file_new_for_path (paths[1]);
			g_object_unref (temp);
		} else
			filei = temp;
	}

	if (r_priv->cropped) {
		local = g_file_new_for_uri (lpaths[2]);
		if (!g_file_query_exists (local, NULL)) {
			filei = g_file_new_for_path (paths[2]);
			g_object_unref (local);
		} else {
			filei = local;
		}
	} else if (r_priv->width > 128 || r_priv->height > 128) {
		local = g_file_new_for_uri (lpaths[1]);
		if (!g_file_query_exists (local, NULL)) {
			filei = g_file_new_for_path (paths[1]);
			g_object_unref (local);
		} else {
			filei = local;
		}
	} else {
		local = g_file_new_for_uri (lpaths[0]);
		if (!g_file_query_exists (local, NULL)) {
			filei = g_file_new_for_path (paths[0]);
			g_object_unref (local);
		} else {
			filei = local;
		}
	}

	for (x = 0; x < 3; x++) {
		g_free (paths[x]); 
		paths[x] = NULL;
		g_free (lpaths[x]); 
		lpaths[x] = NULL;
	}

	if (r_priv->pcallback) {

		stream = G_INPUT_STREAM (g_file_read (filei, NULL, &error));

		if (!error) {
			/* Read the stream as a pixbuf at the requested exact scale */
			pixbuf = gdk_pixbuf_new_from_stream_at_scale (stream,
				r_priv->width, r_priv->height, TRUE, 
				NULL, &error);
		}

		/* Callback user function, passing the pixbuf and error */

		if (r_priv->errors) {
			err_d = TRUE;
			if (!error)
				g_set_error (&error, FACTORY_ERROR, 0, "%s", r_priv->errors->str);
			else {
				g_string_append (r_priv->errors, " - ");
				g_string_append (r_priv->errors, error->message);
				g_clear_error (&error);
				g_set_error (&error, FACTORY_ERROR, 0, "%s", r_priv->errors->str);
			}
		}

		r_priv->pcallback (r_priv->factory, pixbuf, error, r_priv->user_data);

		/* Cleanup */

		if (stream) {
			g_input_stream_close (G_INPUT_STREAM (stream), NULL, NULL);
			g_object_unref (stream);
		}

		if (pixbuf)
			gdk_pixbuf_unref (pixbuf);
	}

	if (r_priv->ucallback) {
		gchar *u = g_file_get_uri (filei); 

		if (r_priv->errors && !err_d) {
			if (!error)
				g_set_error (&error, FACTORY_ERROR, 0, "%s", r_priv->errors->str);
			else {
				g_string_append (r_priv->errors, " - ");
				g_string_append (r_priv->errors, error->message);
				g_clear_error (&error);
				g_set_error (&error, FACTORY_ERROR, 0, "%s", r_priv->errors->str);
			}
		}

		r_priv->ucallback (r_priv->factory, (const gchar *) u, error, r_priv->user_data);
		g_free (u);
	}

	if (filei)
		g_object_unref (filei);

	if (error)
		g_error_free (error);

	if (r_priv->destroy)
		r_priv->destroy (r_priv->user_data);

}


static void
on_task_finished (DBusGProxy *proxy,
		  guint       handle,
		  gpointer    user_data)
{
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

static gboolean waiting_for_cb = FALSE;

static void
on_task_error (DBusGProxy *proxy,
		  guint       handle,
		  GStrv       failed_uris,
		  gint        error_code,
		  gchar      *error_message,
		  gpointer    user_data)
{
	HildonThumbnailFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (user_data);
	gchar *key = g_strdup_printf ("%d", handle);
	HildonThumbnailRequest *request = g_hash_table_lookup (f_priv->tasks, key);

	if (request) {
		HildonThumbnailRequestPrivate *r_priv = REQUEST_GET_PRIVATE (request);

		if (!r_priv->errors) {
			r_priv->errors = g_string_new (error_message);
		} else {
			g_string_append (r_priv->errors, " - ");
			g_string_append (r_priv->errors, error_message);
		}
	}

	g_free (key);
}


static gboolean
have_all_for_request_cb (gpointer user_data)
{
	HildonThumbnailRequestPrivate *r_priv = REQUEST_GET_PRIVATE (user_data);
	create_pixbuf_and_callback (r_priv);
	waiting_for_cb = FALSE;
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

	dbus_g_object_register_marshaller (thumbnailer_marshal_VOID__UINT_BOXED_INT_STRING,
					G_TYPE_NONE,
					G_TYPE_UINT, 
					G_TYPE_BOXED,
					G_TYPE_INT,
					G_TYPE_STRING,
					G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (f_priv->proxy, "Finished",
			     G_CALLBACK (on_task_finished),
			     self,
			     NULL);

	dbus_g_proxy_add_signal (f_priv->proxy, "Error", 
				 G_TYPE_UINT, 
				 G_TYPE_BOXED,
				 G_TYPE_INT,
				 G_TYPE_STRING,
				 G_TYPE_INVALID);

	dbus_g_proxy_connect_signal (f_priv->proxy, "Error",
			     G_CALLBACK (on_task_error),
			     self,
			     NULL);
}

static void
hildon_thumbnail_request_init (HildonThumbnailRequest *self)
{
	HildonThumbnailRequestPrivate *r_priv = REQUEST_GET_PRIVATE (self);

	r_priv->uris = NULL;
	r_priv->key = NULL;
	r_priv->cropped = FALSE;
	r_priv->factory = NULL;
	r_priv->pcallback = NULL;
	r_priv->ucallback = NULL;
	r_priv->destroy = NULL;
	r_priv->user_data = NULL;
	r_priv->errors = NULL;
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
	r_priv->key = key;
	g_hash_table_replace (f_priv->tasks, g_strdup (key), 
			      g_object_ref (request));
	waiting_for_cb = FALSE;

	g_object_unref (request);
}

static HildonThumbnailRequest*
hildon_thumbnail_factory_request_generic (HildonThumbnailFactory *self,
					 const gchar *uri,
					 guint width, guint height,
					 gboolean cropped,
					 const gchar *mime_type,
					 HildonThumbnailRequestUriCallback ucallback,
					 HildonThumbnailRequestPixbufCallback pcallback,
					 gpointer user_data,
					 GDestroyNotify destroy)
{
	HildonThumbnailRequest *request = g_object_new (HILDON_TYPE_THUMBNAIL_REQUEST, NULL);
	HildonThumbnailRequestPrivate *r_priv = REQUEST_GET_PRIVATE (request);
	HildonThumbnailFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (self);
	gboolean have = FALSE;
	GStrv mime_types = NULL;
	guint y, i, x;

	gchar *paths[3] = { NULL, NULL, NULL };
	gchar *lpaths[3] = { NULL, NULL, NULL };

	g_debug ("Thumbnail n request for %s at %dx%d %s\n",
		 uri, width, height, cropped?"CROPPED":"NON-CROPPED");

	for (y = 0 ; y < 2 && !have; y++ ) {

		have = TRUE;

		for (x = 0; x < 3; x++) {
			g_free (paths[x]); 
			paths[x] = NULL;
			g_free (lpaths[x]); 
			lpaths[x] = NULL;
		}

		hildon_thumbnail_util_get_thumb_paths (uri, 
						       &paths[0], 
						       &paths[1], 
						       &paths[2],
						       &lpaths[0],
						       &lpaths[1],
						       &lpaths[2], 
						       (y==0));

		for (i = 0; i < 3 && have; i++) {
			gchar *localp = g_filename_from_uri (lpaths[i], NULL, NULL);
			have = (g_file_test (paths[i], G_FILE_TEST_EXISTS) || 
				g_file_test (localp, G_FILE_TEST_EXISTS));
			g_free (localp);
		}
	}

	for (x = 0; x < 3; x++) {
		g_free (paths[x]); 
		paths[x] = NULL;
		g_free (lpaths[x]); 
		lpaths[x] = NULL;
	}

	r_priv->uris = (GStrv) g_malloc0 (sizeof (gchar *) * 2);
	r_priv->uris[0] = g_strdup (uri);
	r_priv->factory = g_object_ref (self);
	r_priv->user_data = user_data;
	r_priv->pcallback = pcallback;
	r_priv->ucallback = ucallback;
	r_priv->destroy = destroy;
	r_priv->cropped = cropped;

	mime_types = (GStrv) g_malloc0 (sizeof (gchar *) * 2);

	if (mime_type) {
		mime_types[0] = g_strdup (mime_type);
	} else {
		mime_types[0] = NULL;
	}

	waiting_for_cb = TRUE;
	if (!have) {
		org_freedesktop_thumbnailer_Generic_queue_async (f_priv->proxy, 
								 (const char **) r_priv->uris, 
								 (const char **) mime_types,
								 0, 
								 on_got_handle, 
								 g_object_ref (request));
	} else {
		g_idle_add_full (G_PRIORITY_DEFAULT, have_all_for_request_cb, 
				 (GSourceFunc) g_object_ref (request),
				 (GDestroyNotify) g_object_unref);
	}

	g_strfreev (mime_types);

	return request;
}

HildonThumbnailRequest*
hildon_thumbnail_factory_request_pixbuf (HildonThumbnailFactory *self,
					 const gchar *uri,
					 guint width, guint height,
					 gboolean cropped,
					 const gchar *mime_type,
					 HildonThumbnailRequestPixbufCallback callback,
					 gpointer user_data,
					 GDestroyNotify destroy)
{
	return hildon_thumbnail_factory_request_generic (self, uri, width, height,
							 cropped, mime_type, 
							 NULL,
							 callback, 
							 user_data,
							 destroy);
}

HildonThumbnailRequest*
hildon_thumbnail_factory_request_uri (HildonThumbnailFactory *self,
					 const gchar *uri,
					 guint width, guint height,
					 gboolean cropped,
					 const gchar *mime_type,
					 HildonThumbnailRequestUriCallback callback,
					 gpointer user_data,
					 GDestroyNotify destroy)
{
	return hildon_thumbnail_factory_request_generic (self, uri, width, height,
							 cropped, mime_type, 
							 callback,
							 NULL, 
							 user_data,
							 destroy);
}

void 
hildon_thumbnail_factory_join (HildonThumbnailFactory *self)
{
	HildonThumbnailFactoryPrivate *f_priv = FACTORY_GET_PRIVATE (self);

	while (waiting_for_cb)
		g_main_context_iteration (NULL, FALSE);

	while(g_hash_table_size (f_priv->tasks) != 0) {
		g_main_context_iteration (NULL, FALSE);
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
	HildonThumbnailRequest *found;

	while (waiting_for_cb)
		g_main_context_iteration (NULL, FALSE);

	if (!r_priv->key)
		return;

	found = g_hash_table_lookup (f_priv->tasks, r_priv->key);

	while (found) {
		g_main_context_iteration (NULL, FALSE);
		found = g_hash_table_lookup (f_priv->tasks, r_priv->key);
	}
}

static GStaticMutex mutex = G_STATIC_MUTEX_INIT;
static HildonThumbnailFactory *singleton = NULL;

HildonThumbnailFactory* 
hildon_thumbnail_factory_get_instance (void)
{
	g_static_mutex_lock (&mutex);

	if (!singleton) {
		singleton = g_object_new (HILDON_TYPE_THUMBNAIL_FACTORY, NULL);
	}

	g_static_mutex_unlock (&mutex);

	return g_object_ref (singleton);
}

G_DEFINE_TYPE (HildonThumbnailFactory, hildon_thumbnail_factory, G_TYPE_OBJECT)

G_DEFINE_TYPE (HildonThumbnailRequest, hildon_thumbnail_request, G_TYPE_OBJECT)
