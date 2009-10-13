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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string.h>
#include <ctype.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <dbus/dbus-glib-bindings.h>

#include "thumbnailer.h"
#include "thumbnailer-marshal.h"
#include "thumbnailer-glue.h"

#include <hildon-thumbnail-plugin.h>

#include "dbus-utils.h"
#include "utils.h"

#define THUMB_ERROR_DOMAIN	"HildonThumbnailer"
#define THUMB_ERROR		g_quark_from_static_string (THUMB_ERROR_DOMAIN)

#define THUMBNAILER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TYPE_THUMBNAILER, ThumbnailerPrivate))

G_DEFINE_TYPE (Thumbnailer, thumbnailer, G_TYPE_OBJECT)

void keep_alive (void);
void initialize_priority (void);

typedef struct {
	ThumbnailManager *manager;
	GHashTable *plugins_perscheme;
	GThreadPool *large_pool;
	GThreadPool *normal_pool;
	GMutex *mutex;
	GList *tasks;
#ifdef HAVE_OSSO
	GMutex *cmutex;
	gboolean waiting, must_wait;
	GCond *cond;
#endif
} ThumbnailerPrivate;

#ifdef HAVE_OSSO
static __thread int big_thread = 0;
#endif

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
	GModule *plugin;
	gint priority;
} PluginRegistration;

static void
free_pluginregistration (PluginRegistration *r)
{
	g_slice_free (PluginRegistration, r);
}

#ifdef HAVE_OSSO

#define TRACKER_SERVICE		"org.freedesktop.Tracker"
#define TRACKER_PATH		"/org/freedesktop/Tracker"
#define TRACKER_INTERFACE	"org.freedesktop.Tracker"

static DBusGProxy*
get_tracker (void)
{
	static DBusGProxy *proxy = NULL;

	if (!proxy) {
		GError          *error = NULL;
		DBusGConnection *connection;

		connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

		if (!error) {
			proxy = dbus_g_proxy_new_for_name (connection,
							   TRACKER_SERVICE,
							   TRACKER_PATH,
							   TRACKER_INTERFACE);

			dbus_g_object_register_marshaller (thumbnailer_marshal_VOID__STRING_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN_BOOLEAN,
							   G_TYPE_NONE,
							   G_TYPE_STRING,
							   G_TYPE_BOOLEAN,
							   G_TYPE_BOOLEAN,
							   G_TYPE_BOOLEAN,
							   G_TYPE_BOOLEAN,
							   G_TYPE_BOOLEAN,
							   G_TYPE_BOOLEAN,
							   G_TYPE_INVALID);

			dbus_g_proxy_add_signal (proxy, "IndexStateChange",
						 G_TYPE_STRING,
						 G_TYPE_BOOLEAN,
						 G_TYPE_BOOLEAN,
						 G_TYPE_BOOLEAN,
						 G_TYPE_BOOLEAN,
						 G_TYPE_BOOLEAN,
						 G_TYPE_BOOLEAN,
						 G_TYPE_INVALID);
		} else {
			g_error_free (error);
		}
	}

	return proxy;
}
#endif

static GModule*
get_plugin (Thumbnailer *object, const gchar *uri_scheme, const gchar *mime_type)
{
	// TODO: take into account uri_scheme
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);
	GHashTable *hash;
	GModule *plugin = NULL;

	hash = g_hash_table_lookup (priv->plugins_perscheme, uri_scheme);

	if (hash) {
		PluginRegistration *reg = g_hash_table_lookup (hash, mime_type);
		if (reg) {
			plugin = reg->plugin;
		}
	}

	return plugin;
}

void 
thumbnailer_register_plugin (Thumbnailer *object, const gchar *mime_type, GModule *plugin, const GStrv uri_schemes, gint priority)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);
	guint i = 0;

	g_mutex_lock (priv->mutex);

	while (uri_schemes[i] != NULL) {
		GHashTable *hash;

		hash = g_hash_table_lookup (priv->plugins_perscheme, uri_schemes[i]);

		if (!hash) {
			PluginRegistration *reg = g_slice_new (PluginRegistration);

			reg->plugin = plugin;
			reg->priority = priority;

			hash = g_hash_table_new_full (g_str_hash, g_str_equal, 
					 (GDestroyNotify) g_free, 
					 (GDestroyNotify) free_pluginregistration);

			g_hash_table_replace (priv->plugins_perscheme, 
					      g_strdup (uri_schemes[i]), 
					      hash);

			g_hash_table_replace (hash, g_strdup (mime_type), reg);

		} else {
			PluginRegistration *o_reg = g_hash_table_lookup (hash, mime_type);

			if (!o_reg || (o_reg && o_reg->priority < priority)) {
				PluginRegistration *reg = g_slice_new (PluginRegistration);

				reg->plugin = plugin;
				reg->priority = priority;

				g_hash_table_replace (hash, g_strdup (mime_type), reg);

			}
		}

		i++;
	}

	thumbnail_manager_i_have (priv->manager, mime_type);

	g_mutex_unlock (priv->mutex);

}

static gboolean 
do_delete_or_not (gpointer key, gpointer value, gpointer user_data)
{
	PluginRegistration *reg = value;

	if (reg->plugin == user_data)
		return TRUE;

	return FALSE;
}

static void
foreach_scheme (gpointer key, gpointer value, gpointer plugin)
{
	GHashTable *hash = value;
	g_hash_table_foreach_remove (hash, do_delete_or_not, plugin);
}

void 
thumbnailer_unregister_plugin (Thumbnailer *object, GModule *plugin)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

	g_mutex_lock (priv->mutex);
	/* This might leave an empty node in case there are no more plugins
	 * for a specific scheme. But that's harmless. */
	g_hash_table_foreach (priv->plugins_perscheme, foreach_scheme, plugin);
	g_mutex_unlock (priv->mutex);
}

static void
get_some_file_infos (const gchar *uri, gchar **mime_type, guint64 *mtime, gchar *mime_hint, GError **error)
{
	const gchar *content_type;
	GFileInfo *info;
	GFile *file;

	*mime_type = NULL;

	file = g_file_new_for_uri (uri);
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				  G_FILE_ATTRIBUTE_TIME_MODIFIED,
				  G_FILE_QUERY_INFO_NONE,
				  NULL, error);

	if (info) {
		if (mtime) 
			*mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
		content_type = g_file_info_get_content_type (info);
		if (content_type)
			*mime_type = g_strdup (content_type);
		else if (mime_hint)
			*mime_type = g_strdup (mime_hint);
		else 
			*mime_type = g_strdup ("unknown/unknown");
		g_object_unref (info);
	}

	g_object_unref (file);
}

typedef struct {
	Thumbnailer *object;
	GStrv urls, mime_types;
	guint num;
	gboolean unqueued, dead;
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
mark_unqueued (gpointer data,
	       gpointer user_data)
{
	WorkTask *task = data;
	guint handle = GPOINTER_TO_UINT (user_data);

	if (task->num == handle)
		task->unqueued = TRUE;
}

void
thumbnailer_unqueue (Thumbnailer *object, guint handle, DBusGMethodInvocation *context)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

	keep_alive ();

	g_mutex_lock (priv->mutex);
	g_list_foreach (priv->tasks, mark_unqueued, GUINT_TO_POINTER (handle));
	g_mutex_unlock (priv->mutex);
}

static void 
crash_queued (WorkTask *task, guint handle)
{
	task->unqueued = TRUE;
	task->dead = TRUE;
	g_signal_emit (task->object, signals[FINISHED_SIGNAL], 0,
			       task->num);
}

void
thumbnailer_crash_out (Thumbnailer *object)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

	g_mutex_lock (priv->mutex);
	g_list_foreach (priv->tasks, (GFunc) crash_queued, NULL);
	g_mutex_unlock (priv->mutex);
}

void
thumbnailer_queue (Thumbnailer *object, GStrv urls, GStrv mime_hints, guint handle_to_unqueue, DBusGMethodInvocation *context)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);
	WorkTask *task;
	static guint num = 0;

	dbus_async_return_if_fail (urls != NULL, context);

	task = g_slice_new (WorkTask);

	keep_alive ();

	task->unqueued = FALSE;
	task->num = ++num;
	task->object = g_object_ref (object);
	task->urls = g_strdupv (urls);
	task->dead = FALSE;

	if (mime_hints)
		task->mime_types = g_strdupv (mime_hints);
	else
		task->mime_types = NULL;

	g_mutex_lock (priv->mutex);
	g_list_foreach (priv->tasks, mark_unqueued, GUINT_TO_POINTER (handle_to_unqueue));
	priv->tasks = g_list_prepend (priv->tasks, task);
	if (g_strv_length (urls) > 50)
		g_thread_pool_push (priv->large_pool, task, NULL);
	else
		g_thread_pool_push (priv->normal_pool, task, NULL);
	g_mutex_unlock (priv->mutex);

	dbus_g_method_return (context, num);
}

static gboolean 
strv_contains (const gchar **list, gchar *uri)
{
	guint i = 0;
	gboolean found = FALSE;
	while (list[i] != NULL && !found) {
		/* We indeed search the full URI for the piece */ 
		if (strcasestr ((char *) uri, (char *) list[i]) != NULL)
			found = TRUE;
		i++;
	}
	return found;
}

static GStrv
subtract_strv (GStrv a, GStrv b)
{
	guint i = 0;
	GList *newlist = NULL;
	GStrv retval = NULL;

	while (a[i] != NULL) {
		guint y = 0;
		gboolean found = FALSE;

		while (b[y] != NULL) {
			if (strcmp (a[i], b[y]) == 0) {
				found = TRUE;
				break;
			}
			y++;
		}
		if (!found)
			newlist = g_list_append (newlist, g_strdup (a[i]));
		i++;
	}

	if (newlist) {
		guint t = 0;
		GList *copy = newlist;
		retval = (GStrv) g_malloc0 (sizeof (gchar*) * (g_list_length (newlist) + 1));
		while (copy) {
			retval[t] = copy->data;
			copy = g_list_next (copy);
			t++;
		}
		retval[t] = NULL;
		g_list_free (newlist);
	}

	return retval;
}


typedef struct {
	GCond *condition;
	GMutex *mutex;
	gchar *error_msg;
	gint error_code;
	const gchar *uri, *mime_type;
	gboolean had_callback, had_event;
} SpecializedInfo;

static void
specialized_error (DBusGProxy   *proxy,
		   gchar *uri,
		   gint error_code,
		   gchar *error_msg,
		   gpointer user_data)
{
	SpecializedInfo *info = user_data;

	if (g_strcmp0 (info->uri, uri) == 0) {
		info->error_msg = g_strdup (error_msg);
		info->error_code = error_code;

		g_mutex_lock (info->mutex);
		g_cond_broadcast (info->condition);
		info->had_callback = TRUE;
		g_mutex_unlock (info->mutex);
	}
}

static void
specialized_ready (DBusGProxy   *proxy,
		   gchar *uri,
		   gpointer user_data)
{
	SpecializedInfo *info = user_data;

	if (g_strcmp0 (info->uri, uri) == 0) {
		info->error_msg = NULL;

		g_mutex_lock (info->mutex);
		g_cond_broadcast (info->condition);
		info->had_callback = TRUE;
		g_mutex_unlock (info->mutex);
	}
}

#define DAEMON_ERROR_DOMAIN	"HildonThumbnailerSpecialized"
#define DAEMON_ERROR		g_quark_from_static_string (DAEMON_ERROR_DOMAIN)

static gboolean
thumb_check (const gchar *filename, guint64 mtime)
{
	gboolean retval = FALSE;

	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		GFileInfo *info;
		GFile *file = g_file_new_for_path (filename);
		info = g_file_query_info (file, 
					   G_FILE_ATTRIBUTE_TIME_MODIFIED,
					   G_FILE_QUERY_INFO_NONE, NULL, NULL);
		if (info) {
			guint64 fmtime;
			fmtime = g_file_info_get_attribute_uint64 (info, 
							   G_FILE_ATTRIBUTE_TIME_MODIFIED);
			if (fmtime == (guint64) mtime) {
				retval = TRUE;
			}
			g_object_unref (info);
		}
		g_object_unref (file);
	}

	return retval;
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
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (task->object);
	GStrv urls = task->urls;
	GStrv mime_types = task->mime_types;
	guint i;
	GHashTable *schemes;
	GHashTableIter s_iter;
	gpointer s_key, s_value;
	GList *thumb_items = NULL, *copy;
	GStrv cached_items;

	static const gchar *remotefss[9] = { 
		"smb://", "file:///media", 
		"file:///mnt", "ftp://", 
		"ftps://", "dav://", "nfs://",
		"file:///home/user/MyDocs",
		NULL };

	g_signal_emit (task->object, signals[STARTED_SIGNAL], 0,
			task->num);

	g_mutex_lock (priv->mutex);
	priv->tasks = g_list_remove (priv->tasks, task);
	if (task->unqueued) {
		g_mutex_unlock (priv->mutex);
		goto unqueued;
	}
	g_mutex_unlock (priv->mutex);

	/* We split the request into groups that have items with the same 
	  * mime-type and one group with items that already have a thumbnail */

	schemes = g_hash_table_new_full (g_str_hash, g_str_equal, 
					 (GDestroyNotify) g_free, 
					 (GDestroyNotify) g_hash_table_unref);

	i = 0;

	while (urls[i] != NULL) {
		gchar *mime_type = NULL;
		gboolean has_thumb = FALSE;
		GError *error = NULL;
		gchar *normal = NULL, *large = NULL, *cropped = NULL;
		guint64 mtime_x = 0;
		gchar *mhint = NULL;

#ifdef HAVE_OSSO
		if (big_thread && priv->must_wait) {
			g_mutex_lock (priv->cmutex);
			priv->waiting = TRUE;
			g_debug ("Big-queue thread waiting for Tracker to finish Indexing (Maemo specific)");
			g_cond_wait (priv->cond, priv->cmutex);
			g_mutex_unlock (priv->cmutex);
		}
#endif

		hildon_thumbnail_util_get_thumb_paths (urls[i], &large, &normal, &cropped, 
						       NULL, NULL, NULL, FALSE);

		if (mime_types && g_strv_length (mime_types) >= i)
			mhint = mime_types[i];

		get_some_file_infos (urls[i], &mime_type, &mtime_x,
				     mhint, &error);


#ifdef LARGE_THUMBNAILS
		has_thumb = (thumb_check (large, mtime_x) && 
			     thumb_check (normal, mtime_x) && 
			     thumb_check (cropped, mtime_x));
#else
	#ifdef NORMAL_THUMBNAILS
		has_thumb = (thumb_check (normal, mtime_x) && 
			     thumb_check (cropped, mtime_x));
	#else
		has_thumb =  thumb_check (cropped, mtime_x);
	#endif
#endif


		if (!has_thumb) {
			gchar *pnormal = NULL, *plarge = NULL, *pcropped = NULL;
			hildon_thumbnail_util_get_thumb_paths (urls[i], &plarge, &pnormal, &pcropped, 
						       NULL, NULL, NULL, FALSE);

#ifdef LARGE_THUMBNAILS
			has_thumb = (thumb_check (plarge, mtime_x) && 
				     thumb_check (pnormal, mtime_x) && 
				     thumb_check (pcropped, mtime_x));
#else
	#ifdef NORMAL_THUMBNAILS
			has_thumb = (thumb_check (pnormal, mtime_x) && 
				     thumb_check (pcropped, mtime_x));
	#else
			has_thumb =  thumb_check (pcropped, mtime_x);
	#endif
#endif

			if (has_thumb) {
				g_free (normal);
				normal = pnormal;
				g_free (large);
				large = plarge;
				g_free (cropped);
				cropped = pcropped;
			} else {
				g_free (pcropped);
				g_free (pnormal);
				g_free (plarge);
			}
		}

		g_free (normal);
		g_free (large);
		g_free (cropped);

		if (error) {
			GStrv oneurl = (GStrv) g_malloc0 (sizeof (gchar*) * 2);
			oneurl[0] = g_strdup (urls[i]);
			oneurl[1] = NULL;
			g_signal_emit (task->object, signals[ERROR_SIGNAL],
				       0, task->num, oneurl, 1, error->message);
			g_error_free (error);
			g_strfreev (oneurl);
		} else {
			if (mime_type && !has_thumb) {
				GList *urls_for_mime;
				GHashTable *hash;
				gchar *uri_scheme = g_strdup (urls[i]);
				gchar *ptr = strchr (uri_scheme, ':');
				gchar *uri;

				if (ptr) {
					/* We set the ':' to end-of-string */
					*ptr = '\0';
					/* Contains ie. ftp, ftps, file, http */
					uri = g_strdup (urls[i]); /* XU1 */
				} else {
					g_free (uri_scheme);
					uri_scheme = g_strdup ("file");
					uri = g_strdup_printf ("file://%s", 
						     /* XU2 */ urls[i]);
				}

				hash = g_hash_table_lookup (schemes, uri_scheme);

				if (!hash) {
					hash = g_hash_table_new_full (g_str_hash, g_str_equal, 
					                              (GDestroyNotify) NULL,
					                              (GDestroyNotify) NULL);
					g_hash_table_replace (schemes, uri_scheme, hash);
					urls_for_mime = NULL;
				} else 
					urls_for_mime = g_hash_table_lookup (hash, mime_type);

				urls_for_mime = g_list_prepend (urls_for_mime, uri);
				g_hash_table_replace (hash, mime_type, 
				                      urls_for_mime);
				g_free (uri_scheme);
			} else if (has_thumb)
				thumb_items = g_list_prepend (thumb_items, 
						     /*XU3 */ g_strdup (urls[i]));
		}

		g_free (mime_type);
		i++;
	}

	/* We emit the group that already has a thumbnail */

	cached_items = (GStrv) g_malloc0 (sizeof (gchar*) * (g_list_length (thumb_items) + 1));
	copy = thumb_items;

	i = 0;

	while (copy) {
		/* Copied as new memory at XU3 */
		cached_items[i] = (gchar *) copy->data;
		copy = g_list_next (copy);
		i++;
	}
	cached_items[i] = NULL;

	if (i > 0)
		g_signal_emit (task->object, signals[READY_SIGNAL], 0,
			       cached_items);

	g_list_free (thumb_items);

	g_strfreev (cached_items); /* Frees all XU3 too */

	g_hash_table_iter_init (&s_iter, schemes);

	/* Foreach of the groups that have items that require creating a thumbnail */
	while (g_hash_table_iter_next (&s_iter, &s_key, &s_value)) {

	  GHashTable *hash = s_value;
	  gchar *uri_scheme = s_key;
	  GHashTableIter iter;
	  gpointer key, value;

	  g_hash_table_iter_init (&iter, hash);

	  while (g_hash_table_iter_next (&iter, &key, &value)) {
		gboolean had_err = FALSE;
		gchar *mime_type = g_strdup (key);
		GList *urlm = value;
		GStrv urlss;
		DBusGProxy *proxy;

		copy = urlm;

 		urlss = (GStrv) g_malloc0 (sizeof (gchar *) * (g_list_length (urlm) + 1));

		i = 0;

		while (copy) {
			/* Copied as new memory at XU1 or XU2 */
			urlss[i] = (gchar *) copy->data;
			i++;
			copy = g_list_next (copy);
		}

		urlss[i] = NULL;

		/* Free the value in the hash and remove the key */

		g_list_free (urlm);
		g_hash_table_iter_remove (&iter);

		/* If we have a third party thumbnailer for this mime-type, we
		 * proxy the call */

		proxy = thumbnail_manager_get_handler (priv->manager, uri_scheme, mime_type);

		if (proxy) {
			guint o;


			for (o = 0; urlss[o]; o++) {
				GError *error = NULL;
				SpecializedInfo info;
				GTimeVal timev;

				keep_alive ();

				info.condition = g_cond_new ();
				info.had_callback = FALSE;
				info.mutex = g_mutex_new ();
				info.uri = urlss[o];
				info.mime_type = mime_type;
				info.error_msg = NULL;

				dbus_g_proxy_connect_signal (proxy, "Ready",
							     G_CALLBACK (specialized_ready),
							     &info, 
							     NULL);

				dbus_g_proxy_connect_signal (proxy, "Error",
							     G_CALLBACK (specialized_error),
							     &info, 
							     NULL);

				dbus_g_proxy_call_no_reply (proxy, "Create", 
							    G_TYPE_STRING, info.uri,
							    G_TYPE_STRING, info.mime_type,
							    G_TYPE_INVALID, 
							    G_TYPE_INVALID);

				g_get_current_time (&timev);
				g_time_val_add  (&timev, 100000000); /* 100 seconds worth of timeout */

				g_mutex_lock (info.mutex);
				/* We are a thread, so the mainloop will still be
				 * be running to receive the error and ready signals */
				if (!info.had_callback)
					g_cond_timed_wait (info.condition, info.mutex, &timev);
				g_mutex_unlock (info.mutex);

				if (!info.had_callback) {
					g_set_error (&error, DAEMON_ERROR, 0,
						     "Timeout for %s", info.uri);
				}

				if (info.error_msg) {
					g_set_error (&error, DAEMON_ERROR, 
						     info.error_code,
						     "%s", info.error_msg);
					g_free (info.error_msg);
				}

				dbus_g_proxy_disconnect_signal (proxy, "Error",
								G_CALLBACK (specialized_error),
								&info);

				dbus_g_proxy_disconnect_signal (proxy, "Ready",
								G_CALLBACK (specialized_ready),
								&info);

				g_cond_free (info.condition);
				g_mutex_free (info.mutex);

				keep_alive ();

				if (error) {
					GStrv failed_urls = (GStrv) g_malloc0 (sizeof (gchar *) * 2);

					failed_urls[0] = g_strdup (info.uri);
					failed_urls[1] = NULL;

					g_signal_emit (task->object, signals[ERROR_SIGNAL],
						       0, task->num, failed_urls, 1, 
						       error->message);

					g_clear_error (&error);

					g_strfreev (failed_urls);

					had_err = TRUE;
				} else {
					GStrv succeeded_urls = (GStrv) g_malloc0 (sizeof (gchar *) * 2);

					succeeded_urls[0] = g_strdup (info.uri);
					succeeded_urls[1] = NULL;

					g_signal_emit (task->object, signals[READY_SIGNAL], 
						       0, succeeded_urls);

					g_strfreev (succeeded_urls);
				}
			}

			g_object_unref (proxy);

		/* If not if we have a plugin that can handle it, we let the 
		 * plugin have a go at it */

		} else {
			GModule *module;
			g_mutex_lock (priv->mutex);
			module = get_plugin (task->object, uri_scheme, key);
			g_mutex_unlock (priv->mutex);

			if (module) {
				GError *error = NULL;
				GStrv failed_urls = NULL;

				keep_alive ();

				hildon_thumbnail_plugin_do_create (module, urlss, 
								   mime_type, 
								   &failed_urls, 
								   &error);

				keep_alive ();

				if (error) {
					GStrv newlist = subtract_strv (urlss, failed_urls);

					if (newlist) {
						g_signal_emit (task->object, signals[READY_SIGNAL], 
							       0, newlist);
						g_strfreev (newlist);
					}

					g_signal_emit (task->object, signals[ERROR_SIGNAL],
						       0, task->num, failed_urls, 1, 
						       error->message);
					g_clear_error (&error);
					had_err = TRUE;
				} else
					g_signal_emit (task->object, signals[READY_SIGNAL], 
						       0, urlss);

				if (failed_urls)
					g_strfreev (failed_urls);

			/* And if even that is not the case, we are very sorry */

			} else {
				gchar *str = g_strdup_printf ("No handler for %s", (gchar*) key);
				g_signal_emit (task->object, signals[ERROR_SIGNAL],
						       0, task->num, urlss, 0, str);
				had_err = TRUE;
				g_free (str);
			}
		}

		i = 0;

		while (!had_err && urlss[i] != NULL) {
			if (strv_contains (remotefss, urlss[i])) {
			  guint y = 0;
			  for (y = 0; y < 2; y++) {
				gchar *from[4] = { NULL, NULL, NULL, NULL };
				gchar *to[4] = { NULL, NULL, NULL, NULL };
				guint z = 0;
				GError *error = NULL;

				hildon_thumbnail_util_get_thumb_paths (urlss[i], 
								       &from[0], 
								       &from[1], 
								       &from[2], 
								       &to[0], 
								       &to[1], 
								       &to[2], 
								       (y == 0));

				for (z = 0; z < 3 && !error; z++) {
					GFile *from_file, *to_file;

					from_file = g_file_new_for_path (from[z]);
					to_file = g_file_new_for_uri (to[z]);

					g_file_copy (from_file, to_file, 0, NULL, 
						     NULL, NULL, &error);

					g_object_unref (from_file);
					g_object_unref (to_file);
				}

				for (z = 0; z < 3; z++) {
					g_free (from[z]);
					g_free (to[z]);
				}

				if (error)
					g_error_free (error);
			  }
			}
			i++;
		}

		g_free (mime_type);

		/* Frees all XU1 and XU2 (in this schemes-group) */
		g_strfreev (urlss); 
	  }
	  g_assert (g_hash_table_size (hash) == 0);
	}

	g_hash_table_unref (schemes);


unqueued:

	if (!task->dead) {
		g_signal_emit (task->object, signals[FINISHED_SIGNAL], 0,
				       task->num);
	}

	g_object_unref (task->object);
	g_strfreev (task->urls);
	if (task->mime_types)
		g_strfreev (task->mime_types);

	g_slice_free (WorkTask, task);

	return;
}


static void 
do_the_large_work (WorkTask *task, gpointer user_data)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (task->object);

#ifdef HAVE_OSSO
	big_thread = 1;
#endif

	initialize_priority ();
	do_the_work (task, user_data);
}


void
thumbnailer_move (Thumbnailer *object, GStrv from_urls, GStrv to_urls, DBusGMethodInvocation *context)
{
	guint i = 0;

	dbus_async_return_if_fail (from_urls != NULL, context);
	dbus_async_return_if_fail (to_urls != NULL, context);

	keep_alive ();

	while (from_urls[i] != NULL && to_urls[i] != NULL) {

	  guint y = 0;
	  for (y = 0; y < 2; y++ ) {
		const gchar *from_uri = from_urls[i];
		const gchar *to_uri = to_urls[i];
		gchar *from_normal = NULL, 
		      *from_large = NULL, 
		      *from_cropped = NULL;
		gchar *to_normal = NULL, 
		      *to_large = NULL,
		      *to_cropped = NULL;

		hildon_thumbnail_util_get_thumb_paths (from_uri, &from_large, 
						       &from_normal, 
						       &from_cropped,
						       NULL, NULL, NULL,
						       (y == 0));


		hildon_thumbnail_util_get_thumb_paths (to_uri, &to_large, 
						       &to_normal, 
						       &to_cropped,
						       NULL, NULL, NULL,
						       (y == 0));

		g_rename (from_large, to_large);
		g_rename (from_normal, to_normal);
		g_rename (from_cropped, to_cropped);

		g_free (from_normal);
		g_free (from_large);
		g_free (from_cropped);
		g_free (to_normal);
		g_free (to_large);
		g_free (to_cropped);

	  }
	  i++;
	}

	dbus_g_method_return (context);
}

void
thumbnailer_copy (Thumbnailer *object, GStrv from_urls, GStrv to_urls, DBusGMethodInvocation *context)
{
	guint i = 0;

	dbus_async_return_if_fail (from_urls != NULL, context);
	dbus_async_return_if_fail (to_urls != NULL, context);

	keep_alive ();

	while (from_urls[i] != NULL && to_urls[i] != NULL) {
	  guint y = 0;
	  for (y = 0; y < 2; y++ ) {
		const gchar *from_uri = from_urls[i];
		const gchar *to_uri = to_urls[i];
		gchar *from_s[3] = { NULL, NULL, NULL };
		gchar *to_s[3] = { NULL, NULL, NULL };
		guint n;

		hildon_thumbnail_util_get_thumb_paths (from_uri, &from_s[0], 
						       &from_s[1], 
						       &from_s[2],
						       NULL, NULL, NULL,
						       (y == 0));

		hildon_thumbnail_util_get_thumb_paths (to_uri, &to_s[0], 
						       &to_s[1], 
						       &to_s[2],
						       NULL, NULL, NULL,
						       (y == 0));

		for (n = 0; n<3; n++) {
			GFile *from, *to;

			if (!from_s[n] || !to_s[n])
				continue;
			
			from = g_file_new_for_path (from_s[n]);
			to = g_file_new_for_path (to_s[n]);

			/* We indeed ignore copy errors here */

			g_file_copy (from, to, 
				     G_FILE_COPY_NONE|G_FILE_COPY_OVERWRITE|G_FILE_COPY_ALL_METADATA,
				     NULL, NULL, NULL,
				     NULL);

			g_object_unref (from);
			g_object_unref (to);

		}

		for (n = 0; n<3; n++) {
			/* These can be NULL, but that's ok for g_free */
			g_free (from_s[n]);
			g_free (to_s[n]);
		}
	  }
	  i++;
	}

	dbus_g_method_return (context);
}

void
thumbnailer_delete (Thumbnailer *object, GStrv urls, DBusGMethodInvocation *context)
{
	guint i = 0;

	dbus_async_return_if_fail (urls != NULL, context);

	keep_alive ();

	while (urls[i] != NULL) {
	  guint y = 0;
	  for (y = 0; y < 2; y++ ) {
		const gchar *uri = urls[i];
		gchar *normal = NULL, 
		      *large = NULL, 
		      *cropped = NULL;

		hildon_thumbnail_util_get_thumb_paths (uri, &large, 
						       &normal, 
						       &cropped,
						       NULL, NULL, NULL,
						       (y == 0));

		g_unlink (large);
		g_unlink (normal);
		g_unlink (cropped);

		g_free (normal);
		g_free (large);
		g_free (cropped);
	  }
	  i++;
	}

	dbus_g_method_return (context);
}

void
thumbnailer_cleanup (Thumbnailer *object, gchar *uri_prefix, guint since, DBusGMethodInvocation *context)
{
	hildon_thumbnail_outplugins_cleanup (uri_prefix, since);
	dbus_g_method_return (context);
}

static void
thumbnailer_finalize (GObject *object)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

	g_thread_pool_free (priv->normal_pool, TRUE, TRUE);
	g_thread_pool_free (priv->large_pool, TRUE, TRUE);

	g_object_unref (priv->manager);
	g_hash_table_unref (priv->plugins_perscheme);
	g_mutex_free (priv->mutex);

#ifdef HAVE_OSSO
	g_mutex_free (priv->cmutex);
	g_cond_free (priv->cond);
#endif

	G_OBJECT_CLASS (thumbnailer_parent_class)->finalize (object);
}

static void 
thumbnailer_set_manager (Thumbnailer *object, ThumbnailManager *manager)
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
							      TYPE_THUMBNAIL_MANAGER,
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
			      G_STRUCT_OFFSET (ThumbnailerClass, started),
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
			      thumbnailer_marshal_VOID__UINT_BOXED_INT_STRING,
			      G_TYPE_NONE,
			      4,
			      G_TYPE_UINT,
			      G_TYPE_STRV,
			      G_TYPE_INT,
			      G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (ThumbnailerPrivate));
}

static void
thumbnailer_init (Thumbnailer *object)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

	priv->mutex = g_mutex_new ();

#ifdef HAVE_OSSO
	priv->cmutex = g_mutex_new ();
	priv->cond = g_cond_new ();
	priv->waiting = FALSE;
	priv->must_wait = FALSE;
#endif

	priv->plugins_perscheme = g_hash_table_new_full (g_str_hash, g_str_equal,
							 (GDestroyNotify) g_free,
							 (GDestroyNotify) g_hash_table_unref);

	/* We could increase the amount of threads to add some parallelism */

	priv->large_pool = g_thread_pool_new ((GFunc) do_the_large_work,NULL,1,TRUE,NULL);
	priv->normal_pool = g_thread_pool_new ((GFunc) do_the_work,NULL,2,TRUE,NULL);

	/* This sort function makes the pool a LIFO */

	g_thread_pool_set_sort_function (priv->large_pool, pool_sort_compare, NULL);
	g_thread_pool_set_sort_function (priv->normal_pool, pool_sort_compare, NULL);
}

#ifdef HAVE_OSSO
static void
tracker_index_state_changed (DBusGProxy *tracker, gchar *state, gboolean initial_index,
			     gboolean in_merge, gboolean is_man_paused, gboolean is_bat_paused,
			     gboolean is_io_paused, gboolean is_indx_en, gpointer user_data)
{
	Thumbnailer *object = user_data;
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

	if (g_strcmp0 (state, "Idle") == 0) {

		g_mutex_lock (priv->cmutex);
		if (priv->waiting) {
			g_debug ("Tracker finised indexing, releasing big-queue thread (Maemo specific)");
			g_cond_broadcast (priv->cond);
			priv->waiting = FALSE;
		}
		g_mutex_unlock (priv->cmutex);

		priv->must_wait = FALSE;
	} else {
		priv->must_wait = TRUE;
	}
}
#endif

void 
thumbnailer_do_stop (void)
{
}


void 
thumbnailer_do_init (DBusGConnection *connection, ThumbnailManager *manager, Thumbnailer **thumbnailer, GError **error)
{
	guint result;
	DBusGProxy *proxy, *tracker;
	GObject *object;

	proxy = dbus_g_proxy_new_for_name (connection, 
					   DBUS_SERVICE_DBUS,
					   DBUS_PATH_DBUS,
					   DBUS_INTERFACE_DBUS);

	org_freedesktop_DBus_request_name (proxy, THUMBNAILER_SERVICE,
					   DBUS_NAME_FLAG_DO_NOT_QUEUE,
					   &result, error);

	g_object_unref (proxy);

	object = g_object_new (TYPE_THUMBNAILER, 
			       "manager", manager,
			       NULL);

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), 
					 &dbus_glib_thumbnailer_object_info);

	dbus_g_connection_register_g_object (connection, 
					     THUMBNAILER_PATH, 
					     object);

#ifdef HAVE_OSSO
	tracker = get_tracker ();

	dbus_g_proxy_connect_signal (tracker, "IndexStateChange",
				     G_CALLBACK (tracker_index_state_changed),
				     object, NULL);
#endif

	*thumbnailer = THUMBNAILER (object);
}
