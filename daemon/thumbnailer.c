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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <gio/gio.h>
#include <dbus/dbus-glib-bindings.h>

#include "manager.h"
#include "thumbnailer.h"
#include "thumbnailer-marshal.h"
#include "thumbnailer-glue.h"
#include "hildon-thumbnail-plugin.h"
#include "dbus-utils.h"
#include "utils.h"

#define THUMBNAILER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TYPE_THUMBNAILER, ThumbnailerPrivate))

G_DEFINE_TYPE (Thumbnailer, thumbnailer, G_TYPE_OBJECT)

typedef struct {
	Manager *manager;
	GHashTable *plugins;
	GThreadPool *pool;
	GMutex *mutex;
	GList *tasks;
} ThumbnailerPrivate;

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

void 
thumbnailer_register_plugin (Thumbnailer *object, const gchar *mime_type, GModule *plugin)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

	g_hash_table_insert (priv->plugins, 
			     g_strdup (mime_type), 
			     plugin);
}

static gboolean 
do_delete_or_not (gpointer key, gpointer value, gpointer user_data)
{
	if (value == user_data)
		return TRUE;
	return FALSE;
}

void 
thumbnailer_unregister_plugin (Thumbnailer *object, GModule *plugin)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

	g_hash_table_foreach_remove (priv->plugins, 
				     do_delete_or_not, 
				     plugin);
}


static void
get_some_file_infos (const gchar *uri, gchar **mime_type, gboolean *has_thumb, GError **error)
{
	const gchar *content_type;
	GFileInfo *info;
	GFile *file;
	gchar *normal = NULL, *large = NULL;

	*mime_type = NULL;
	*has_thumb = FALSE;

	file = g_file_new_for_uri (uri);
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL, error);

	if (info) {
		content_type = g_file_info_get_content_type (info);
		*mime_type = content_type?g_strdup (content_type):g_strdup ("unknown/unknown");
		g_object_unref (info);
	}

	hildon_thumbnail_util_get_thumb_paths (uri, &large, &normal, error);
	*has_thumb = (g_file_test (large, G_FILE_TEST_EXISTS) && g_file_test (normal, G_FILE_TEST_EXISTS));

	g_free (normal);
	g_free (large);

	g_object_unref (file);
}

typedef struct {
	Thumbnailer *object;
	GStrv urls;
	guint num;
	gboolean unqueued;
} WorkTask;


static gint 
pool_sort_compare (gconstpointer a, gconstpointer b, gpointer user_data)
{
	WorkTask *task_a = (WorkTask *) a;
	WorkTask *task_b = (WorkTask *) b;

	/* This makes pool a LIFO */

	return task_b->num - task_a->num;
}

static void 
mark_unqueued (WorkTask *task, guint handle)
{
	if (task->num == handle)
		task->unqueued = TRUE;
}

void
thumbnailer_unqueue (Thumbnailer *object, guint handle, DBusGMethodInvocation *context)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

	g_mutex_lock (priv->mutex);
	g_list_foreach (priv->tasks, (GFunc) mark_unqueued, (gpointer) handle);
	g_mutex_unlock (priv->mutex);
}

void
thumbnailer_queue (Thumbnailer *object, GStrv urls, guint handle_to_unqueue, DBusGMethodInvocation *context)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);
	WorkTask *task = g_slice_new (WorkTask);
	static guint num = 0;

	dbus_async_return_if_fail (urls != NULL, context);

	task->unqueued = FALSE;
	task->num = num++;
	task->object = g_object_ref (object);
	task->urls = g_strdupv (urls);

	g_mutex_lock (priv->mutex);
	g_list_foreach (priv->tasks, (GFunc) mark_unqueued, (gpointer) handle_to_unqueue);
	priv->tasks = g_list_prepend (priv->tasks, task);
	g_thread_pool_push (priv->pool, task, NULL);
	g_mutex_unlock (priv->mutex);

	dbus_g_method_return (context, num);
}

/* This is the threadpool's function. This means that everything we do is 
 * asynchronous wrt to the mainloop (we aren't blocking it).
 * 
 * Thanks to the pool_sort_compare sorter is this pool a LIFO, which means that
 * new requests get a certain priority over older requests. Note that we are not
 * canceling currently running requests. Also note that the thread count of the 
 * pool is set to one. We could increase this number to add some parallelism */

static void 
do_the_work (WorkTask *task, gpointer user_data)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (task->object);
	GHashTable *hash;
	GStrv urls = task->urls;
	guint i;
	GHashTableIter iter;
	gpointer key, value;
	GList *thumb_items = NULL, *copy;
	GStrv cached_items;

	g_mutex_lock (priv->mutex);
	priv->tasks = g_list_remove (priv->tasks, task);
	if (task->unqueued) {
		g_mutex_unlock (priv->mutex);
		goto unqueued;
	}
	g_mutex_unlock (priv->mutex);


	manager_check (priv->manager);

	/* We split the request into groups that have items with the same 
	  * mime-type and one group with items that already have a thumbnail */

	g_signal_emit (task->object, signals[STARTED_SIGNAL], 0,
			task->num);

	hash = g_hash_table_new (g_str_hash, g_str_equal);

	i = 0;

	while (urls[i] != NULL) {
		gchar *mime_type = NULL;
		gboolean has_thumb = FALSE;
		GError *error = NULL;

		get_some_file_infos (urls[i], &mime_type, &has_thumb, &error);

		if (error) {
			g_signal_emit (task->object, signals[ERROR_SIGNAL],
				       0, task->num, 1, error->message);
			g_error_free (error);
		} else {
			// g_print ("M: %s\n", mime_type);
			if (mime_type && !has_thumb) {
				GList *urls_for_mime = g_hash_table_lookup (hash, mime_type);
				urls_for_mime = g_list_prepend (urls_for_mime, urls[i]);
				g_hash_table_replace (hash, mime_type, urls_for_mime);
			} else if (has_thumb)
				thumb_items = g_list_prepend (thumb_items, urls[i]);
		}

		i++;
	}

	/* We emit the group that already has a thumbnail */

	cached_items = (GStrv) g_malloc0 (sizeof (gchar*) * (g_list_length (thumb_items) + 1));
	copy = thumb_items;

	i = 0;

	while (copy) {
		cached_items[i] = g_strdup (copy->data);
		copy = g_list_next (copy);
		i++;
	}
	cached_items[i] = NULL;
	if (i > 0)
		g_signal_emit (task->object, signals[READY_SIGNAL], 0,
			       cached_items);

	g_list_free (thumb_items);
	g_strfreev (cached_items);


	g_hash_table_iter_init (&iter, hash);

	/* Foreach of the groups that have items that require creating a thumbnail */

	while (g_hash_table_iter_next (&iter, &key, &value)) {

		gchar *mime_type = g_strdup (key);
		GList *urlm = value, *copy = urlm;
		GStrv urlss;
		DBusGProxy *proxy;

 		urlss = (GStrv) g_malloc0 (sizeof (gchar *) * (g_list_length (urlm) + 1));

		i = 0;

		while (copy) {
			urlss[i] = g_strdup ((gchar *) copy->data);
			i++;
			copy = g_list_next (copy);
		}

		urlss[i] = NULL;

		/* Free the value in the hash and remove the key */

		g_list_free (urlm);
		g_hash_table_iter_remove (&iter);

		/* If we have a third party thumbnailer for this mime-type, we
		 * proxy the call */

		proxy = manager_get_handler (priv->manager, mime_type);
		if (proxy) {
			GError *error = NULL;

			dbus_g_proxy_call (proxy, "Create", &error, 
					   G_TYPE_STRV, urlss,
					   G_TYPE_INVALID, 
					   G_TYPE_INVALID);

			g_object_unref (proxy);

			if (error) {
				g_signal_emit (task->object, signals[ERROR_SIGNAL],
					       0, task->num, 1, error->message);
				g_clear_error (&error);
			} else
				g_signal_emit (task->object, signals[READY_SIGNAL], 
					       0, urlss);


		/* If not if we have a plugin that can handle it, we let the 
		 * plugin have a go at it */

		} else {
			GModule *module = g_hash_table_lookup (priv->plugins, key);
			if (module) {
				GError *error = NULL;

				hildon_thumbnail_plugin_do_create (module, urlss, &error);

				if (error) {
					g_signal_emit (task->object, signals[ERROR_SIGNAL],
						       0, task->num, 1, error->message);
					g_clear_error (&error);
				} else
					g_signal_emit (task->object, signals[READY_SIGNAL], 
						       0, urlss);

			/* And if even that is not the case, we are very sorry */

			} else {
				gchar *str = g_strdup_printf ("No handler for %s", (gchar*) key);
				g_signal_emit (task->object, signals[ERROR_SIGNAL],
						       0, task->num, 0, str);
				g_free (str);
			}
		}

		g_free (mime_type);
		g_strfreev (urlss);
	}

	g_assert (g_hash_table_size (hash) == 0);

	g_hash_table_unref (hash);

	g_signal_emit (task->object, signals[FINISHED_SIGNAL], 0,
			       task->num);

unqueued:

	g_object_unref (task->object);
	g_strfreev (task->urls);
	g_slice_free (WorkTask, task);

	return;
}

void
thumbnailer_move (Thumbnailer *object, GStrv from_urls, GStrv to_urls, DBusGMethodInvocation *context)
{
	dbus_async_return_if_fail (from_urls != NULL, context);
	dbus_async_return_if_fail (to_urls != NULL, context);
	
	// TODO
	
	dbus_g_method_return (context);
}

void
thumbnailer_copy (Thumbnailer *object, GStrv from_urls, GStrv to_urls, DBusGMethodInvocation *context)
{
	dbus_async_return_if_fail (from_urls != NULL, context);
	dbus_async_return_if_fail (to_urls != NULL, context);
	
	// TODO
	
	dbus_g_method_return (context);
}

void
thumbnailer_delete (Thumbnailer *object, GStrv urls, DBusGMethodInvocation *context)
{
	dbus_async_return_if_fail (urls != NULL, context);
	
	// TODO
	
	dbus_g_method_return (context);
}

static void
thumbnailer_finalize (GObject *object)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

	g_thread_pool_free (priv->pool, TRUE, TRUE);

	g_object_unref (priv->manager);
	g_hash_table_unref (priv->plugins);
	g_mutex_free (priv->mutex);

	G_OBJECT_CLASS (thumbnailer_parent_class)->finalize (object);
}

static void 
thumbnailer_set_manager (Thumbnailer *object, Manager *manager)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);
	if (priv->manager)
		g_object_unref (priv->manager);
	priv->manager = g_object_ref (manager);
}

static void
thumbnailer_set_property (GObject      *object,
		      guint         prop_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_MANAGER:
		thumbnailer_set_manager (THUMBNAILER (object),
					 g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}


static void
thumbnailer_get_property (GObject    *object,
		      guint       prop_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	ThumbnailerPrivate *priv;

	priv = THUMBNAILER_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_MANAGER:
		g_value_set_object (value, priv->manager);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
thumbnailer_class_init (ThumbnailerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = thumbnailer_finalize;
	object_class->set_property = thumbnailer_set_property;
	object_class->get_property = thumbnailer_get_property;

	g_object_class_install_property (object_class,
					 PROP_MANAGER,
					 g_param_spec_object ("manager",
							      "Manager",
							      "Manager",
							      TYPE_MANAGER,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT));

	signals[READY_SIGNAL] =
		g_signal_new ("ready",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ThumbnailerClass, ready),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__BOXED,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRV);

	signals[STARTED_SIGNAL] =
		g_signal_new ("started",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ThumbnailerClass, ready),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_UINT);

	signals[FINISHED_SIGNAL] =
		g_signal_new ("finished",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ThumbnailerClass, finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_UINT);
	
	signals[ERROR_SIGNAL] =
		g_signal_new ("error",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ThumbnailerClass, error),
			      NULL, NULL,
			      thumbnailer_marshal_VOID__UINT_INT_STRING,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_UINT,
			      G_TYPE_INT,
			      G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (ThumbnailerPrivate));
}

static void
thumbnailer_init (Thumbnailer *object)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

	priv->mutex = g_mutex_new ();

	priv->plugins = g_hash_table_new_full (g_str_hash, g_str_equal,
					       (GDestroyNotify) g_free,
					       NULL);

	/* We could increase the amount of threads to add some parallelism */

	priv->pool = g_thread_pool_new ((GFunc) do_the_work,NULL,1,TRUE,NULL);

	/* This sort function makes the pool a LIFO */

	g_thread_pool_set_sort_function (priv->pool, pool_sort_compare, NULL);
}



void 
thumbnailer_do_stop (void)
{
}


void 
thumbnailer_do_init (DBusGConnection *connection, Manager *manager, Thumbnailer **thumbnailer, GError **error)
{
	guint result;
	DBusGProxy *proxy;
	GObject *object;

	proxy = dbus_g_proxy_new_for_name (connection, 
					   DBUS_SERVICE_DBUS,
					   DBUS_PATH_DBUS,
					   DBUS_INTERFACE_DBUS);

	org_freedesktop_DBus_request_name (proxy, THUMBNAILER_SERVICE,
					   DBUS_NAME_FLAG_DO_NOT_QUEUE,
					   &result, error);

	object = g_object_new (TYPE_THUMBNAILER, 
			       "manager", manager,
			       NULL);

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), 
					 &dbus_glib_thumbnailer_object_info);

	dbus_g_connection_register_g_object (connection, 
					     THUMBNAILER_PATH, 
					     object);

	*thumbnailer = THUMBNAILER (object);
}
