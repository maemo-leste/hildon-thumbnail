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

#include "config.h"

#include <string.h>
#include <ctype.h>
#include <glib.h>
#include <gio/gio.h>
#include <dbus/dbus-glib-bindings.h>
#include <glib/gstdio.h>

#include "albumart.h"
#include "albumart-marshal.h"
#include "albumart-glue.h"
#include "dbus-utils.h"
#include "utils.h"
#include "thumbnailer.h"

#define ALBUMART_ERROR_DOMAIN	"HildonAlbumart"
#define ALBUMART_ERROR		g_quark_from_static_string (ALBUMART_ERROR_DOMAIN)

#define ALBUMART_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TYPE_ALBUMART, AlbumartPrivate))

G_DEFINE_TYPE (Albumart, albumart, G_TYPE_OBJECT)

void keep_alive (void);


typedef struct {
	AlbumartManager *manager;
	GThreadPool *normal_pool;
	GMutex mutex;
	GList *tasks;
} AlbumartPrivate;

enum {
	PROP_0,
	PROP_MANAGER
};

enum {
	STARTED_SIGNAL,
	FINISHED_SIGNAL,
	READY_SIGNAL,
	ERROR_SIGNAL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };



typedef struct {
	Albumart *object;
	gchar *album, *artist;
	gchar *kind;
	guint num;
	gboolean unqueued;
} WorkTask;



static DBusGProxy*
get_thumber (void)
{
	static DBusGProxy *proxy = NULL;

	if (!proxy) {
		GError          *error = NULL;
		DBusGConnection *connection;

		connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

		if (!error) {
			proxy = dbus_g_proxy_new_for_name (connection,
								  THUMBNAILER_SERVICE,
								  THUMBNAILER_PATH,
								  THUMBNAILER_INTERFACE);
		} else {
			g_error_free (error);
		}
	}

	return proxy;
}


static gint 
pool_sort_compare (gconstpointer a, gconstpointer b, gpointer user_data)
{
	WorkTask *task_a = (WorkTask *) a;
	WorkTask *task_b = (WorkTask *) b;

	/* This makes pool a LIFO */

	return task_b->num - task_a->num;
}

static void 
mark_unqueued (gpointer data, 
	       gpointer user_data)
{
	WorkTask *task = data;
	guint handle = GPOINTER_TO_UINT (user_data);

	if (task->num == handle)
		task->unqueued = TRUE;
}

void
albumart_unqueue (Albumart *object, guint handle, DBusGMethodInvocation *context)
{
	AlbumartPrivate *priv = ALBUMART_GET_PRIVATE (object);

	keep_alive ();

	g_mutex_lock (&priv->mutex);
	g_list_foreach (priv->tasks, mark_unqueued, GUINT_TO_POINTER (handle));
	g_mutex_unlock (&priv->mutex);
}

void
albumart_queue (Albumart *object, gchar *artist_or_title, gchar *album, gchar *kind, guint handle_to_unqueue, DBusGMethodInvocation *context)
{
	AlbumartPrivate *priv = ALBUMART_GET_PRIVATE (object);
	WorkTask *task;
	static guint num = 0;

	if (!kind || strlen (album) == 0)
		kind = (gchar *) "album";

	if (artist_or_title && strlen (artist_or_title) <= 0)
		artist_or_title = NULL;

	if (album && strlen (album) <= 0)
		album = NULL;

	if (!artist_or_title && !album) {
		num++;
		dbus_g_method_return (context, num);
		return;
	}

	task = g_slice_new0 (WorkTask);

	keep_alive ();

	task->unqueued = FALSE;
	task->num = ++num;
	task->object = g_object_ref (object);

	task->album = g_strdup (album);
	task->artist = g_strdup (artist_or_title);
	task->kind = g_strdup (kind);

	g_mutex_lock (&priv->mutex);
	g_list_foreach (priv->tasks, mark_unqueued, GUINT_TO_POINTER (handle_to_unqueue));
	priv->tasks = g_list_prepend (priv->tasks, task);
	g_thread_pool_push (priv->normal_pool, task, NULL);
	g_mutex_unlock (&priv->mutex);

	dbus_g_method_return (context, num);
}

/* This is the threadpool's function. This means that everything we do is 
 * asynchronous wrt to the mainloop (we aren't blocking it). Because it all 
 * happens in a thread, we must care about proper locking, too.
 * 
 * Thanks to the pool_sort_compare sorter is this pool a LIFO, which means that
 * new requests get a certain priority over older requests. Note that we are not
 * canceling currently running requests. Also note that the thread count of the 
 * pool is set to one. We could increase this number to add some parallelism */

static void 
do_the_work (WorkTask *task, gpointer user_data)
{
	AlbumartPrivate *priv = ALBUMART_GET_PRIVATE (task->object);
	gchar *artist = task->artist;
	gchar *album = task->album;
	gchar *kind = task->kind;
	gchar *path;

	g_signal_emit (task->object, signals[STARTED_SIGNAL], 0,
			task->num);

	g_mutex_lock (&priv->mutex);
	priv->tasks = g_list_remove (priv->tasks, task);
	if (task->unqueued) {
		g_mutex_unlock (&priv->mutex);
		goto unqueued;
	}
	g_mutex_unlock (&priv->mutex);

	hildon_thumbnail_util_get_albumart_path (artist, album, kind, &path);

	if (!g_file_test (path, G_FILE_TEST_EXISTS)) {
		GList *proxies, *copy;
		gboolean handled = FALSE;

		proxies = albumart_manager_get_handlers (priv->manager);
		copy = proxies;

		while (copy && !handled) {
			DBusGProxy *proxy = copy->data;
			GError *error = NULL;

			keep_alive ();

			dbus_g_proxy_call (proxy, "Fetch", &error, 
					   G_TYPE_STRING, artist,
					   G_TYPE_STRING, album,
					   G_TYPE_STRING, kind,
					   G_TYPE_INVALID, 
					   G_TYPE_INVALID);

			keep_alive ();

			if (error) {
				g_signal_emit (task->object, signals[ERROR_SIGNAL],
					       0, task->num, 1, error->message);
				g_clear_error (&error);
			} else {
				gchar **uris = (gchar **) g_malloc0 (sizeof (gchar*) * 2);
				const gchar *mimes[2] = { "image/jpeg", NULL };

				uris[0] = g_filename_to_uri (path, NULL, NULL);
				uris[1] = NULL;
				
				dbus_g_proxy_call_no_reply (get_thumber (),
							    "Queue",
							    G_TYPE_STRV, uris,
							    G_TYPE_STRV, mimes,
							    G_TYPE_UINT, 0,
							    G_TYPE_INVALID,
							    G_TYPE_INVALID);

				g_signal_emit (task->object, signals[READY_SIGNAL], 
				       0, artist, album, kind, path);

				g_strfreev (uris);
				handled = TRUE;
			}

			copy = g_list_next (copy);

			g_object_unref (proxy);
		}

		if (proxies)
			g_list_free (proxies);
	}

	g_free (path);

unqueued:

	g_signal_emit (task->object, signals[FINISHED_SIGNAL], 0,
		       task->num);

	g_object_unref (task->object);
	g_free (task->artist);
	g_free (task->album);
	g_free (task->kind);
	g_slice_free (WorkTask, task);

	return;
}


void
albumart_delete (Albumart *object, gchar *artist_or_title, gchar *album, gchar *kind, DBusGMethodInvocation *context)
{
	gchar *path;

	keep_alive ();

	hildon_thumbnail_util_get_albumart_path (artist_or_title, album, kind, &path);

	g_unlink (path);

	g_free (path);

	dbus_g_method_return (context);
}

static void
albumart_finalize (GObject *object)
{
	AlbumartPrivate *priv = ALBUMART_GET_PRIVATE (object);

	g_thread_pool_free (priv->normal_pool, TRUE, TRUE);
	g_object_unref (priv->manager);

	G_OBJECT_CLASS (albumart_parent_class)->finalize (object);
}

static void 
albumart_set_manager (Albumart *object, AlbumartManager *manager)
{
	AlbumartPrivate *priv = ALBUMART_GET_PRIVATE (object);
	if (priv->manager)
		g_object_unref (priv->manager);
	priv->manager = g_object_ref (manager);
}

static void
albumart_set_property (GObject      *object,
		      guint         prop_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_MANAGER:
		albumart_set_manager (ALBUMART (object),
				      g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}


static void
albumart_get_property (GObject    *object,
		      guint       prop_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	AlbumartPrivate *priv;

	priv = ALBUMART_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_MANAGER:
		g_value_set_object (value, priv->manager);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
albumart_class_init (AlbumartClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = albumart_finalize;
	object_class->set_property = albumart_set_property;
	object_class->get_property = albumart_get_property;

	g_object_class_install_property (object_class,
					 PROP_MANAGER,
					 g_param_spec_object ("manager",
							      "Manager",
							      "Manager",
							      TYPE_ALBUMART_MANAGER,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT));

	signals[READY_SIGNAL] =
		g_signal_new ("ready",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (AlbumartClass, ready),
			      NULL, NULL,
			      albumart_marshal_VOID__STRING_STRING_STRING_STRING,
			      G_TYPE_NONE,
			      3,
			      G_TYPE_STRING,
			      G_TYPE_STRING,
			      G_TYPE_STRING,
			      G_TYPE_STRING);

	signals[STARTED_SIGNAL] =
		g_signal_new ("started",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (AlbumartClass, ready),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_UINT);

	signals[FINISHED_SIGNAL] =
		g_signal_new ("finished",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (AlbumartClass, finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_UINT);
	
	signals[ERROR_SIGNAL] =
		g_signal_new ("error",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (AlbumartClass, error),
			      NULL, NULL,
			      albumart_marshal_VOID__UINT_INT_STRING,
			      G_TYPE_NONE,
			      3,
			      G_TYPE_UINT,
			      G_TYPE_INT,
			      G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (AlbumartPrivate));
}

static void
albumart_init (Albumart *object)
{
	AlbumartPrivate *priv = ALBUMART_GET_PRIVATE (object);

	g_mutex_init (&priv->mutex);

	/* We could increase the amount of threads to add some parallelism */

	priv->normal_pool = g_thread_pool_new ((GFunc) do_the_work,NULL,1,TRUE,NULL);

	/* This sort function makes the pool a LIFO */

	g_thread_pool_set_sort_function (priv->normal_pool, pool_sort_compare, NULL);

}



void 
albumart_do_stop (void)
{
}


void 
albumart_do_init (DBusGConnection *connection, AlbumartManager *manager, Albumart **albumart, GError **error)
{
	guint result;
	DBusGProxy *proxy;
	GObject *object;

	proxy = dbus_g_proxy_new_for_name (connection, 
					   DBUS_SERVICE_DBUS,
					   DBUS_PATH_DBUS,
					   DBUS_INTERFACE_DBUS);

	org_freedesktop_DBus_request_name (proxy, ALBUMART_SERVICE,
					   DBUS_NAME_FLAG_DO_NOT_QUEUE,
					   &result, error);

	g_object_unref (proxy);

	object = g_object_new (TYPE_ALBUMART, 
			       "manager", manager,
			       NULL);

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), 
					 &dbus_glib_albumart_object_info);

	dbus_g_connection_register_g_object (connection, 
					     ALBUMART_PATH, 
					     object);

	*albumart = ALBUMART (object);
}
