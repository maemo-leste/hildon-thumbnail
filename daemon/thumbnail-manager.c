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

#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <dbus/dbus-glib-bindings.h>

#include "thumbnail-manager.h"
#include "manager-glue.h"
#include "dbus-utils.h"
#include "thumbnailer.h"
#include "thumbnailer-marshal.h"

static GFile *homedir, *thumbdir;
static GFileMonitor *homemon, *thumbmon;

#define THUMBNAIL_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TYPE_THUMBNAIL_MANAGER, ThumbnailManagerPrivate))

G_DEFINE_TYPE (ThumbnailManager, thumbnail_manager, G_TYPE_OBJECT)

#ifndef dbus_g_method_get_sender
gchar* dbus_g_method_get_sender (DBusGMethodInvocation *context);
#endif

void keep_alive (void);

typedef struct {
	DBusGConnection *connection;
	GHashTable *handlers;
	GMutex *mutex;
	GList *thumber_has;
} ThumbnailManagerPrivate;

enum {
	PROP_0,
	PROP_CONNECTION
};

DBusGProxy*
thumbnail_manager_get_handler (ThumbnailManager *object, const gchar *uri_scheme, const gchar *mime_type)
{
	ThumbnailManagerPrivate *priv = THUMBNAIL_MANAGER_GET_PRIVATE (object);
	DBusGProxy *proxy;
	gchar *query = g_strdup_printf ("%s-%s", uri_scheme, mime_type);

	g_mutex_lock (priv->mutex);
	proxy = g_hash_table_lookup (priv->handlers, query);
	if (proxy) {
		g_object_ref (proxy);
	}
	g_mutex_unlock (priv->mutex);

	g_free (query);

	return proxy;
}

static void
thumbnail_manager_add (ThumbnailManager *object, gchar *mime_type, gchar *name)
{
	ThumbnailManagerPrivate *priv = THUMBNAIL_MANAGER_GET_PRIVATE (object);
	DBusGProxy *mime_proxy;
	gchar *path = g_strdup_printf ("/%s", name);
	guint len = strlen (path);
	guint i;

	/* Not sure if this path stuff makes any sense ... but it works */

	for (i = 0; i< len; i++) {
		if (path[i] == '.')
			path[i] = '/';
	}

	mime_proxy = dbus_g_proxy_new_for_name (priv->connection, name, 
						path,
						SPECIALIZED_INTERFACE);

	dbus_g_proxy_add_signal (mime_proxy, "Ready", 
				 G_TYPE_STRING,
				 G_TYPE_INVALID);

	dbus_g_proxy_add_signal (mime_proxy, "Error", 
				 G_TYPE_STRING, 
				 G_TYPE_INT,
				 G_TYPE_STRING,
				 G_TYPE_INVALID);

	g_free (path);

	g_hash_table_replace (priv->handlers, 
			      g_strdup (mime_type),
			      mime_proxy);

}

typedef struct {
	gchar *name;
	guint64 mtime;
	gboolean prio;
} ValueInfo;

static void
free_valueinfo (ValueInfo *info) {
	g_free (info->name);
	g_slice_free (ValueInfo, info);
}

static void
thumbnail_manager_check_dir (ThumbnailManager *object, gchar *path, gboolean override)
{
	ThumbnailManagerPrivate *priv = THUMBNAIL_MANAGER_GET_PRIVATE (object);
	const gchar *filen;
	GDir *dir;
	GHashTableIter iter;
	GHashTable *pre;
	gpointer pkey, pvalue;
	gboolean has_override = FALSE;

	dir = g_dir_open (path, 0, NULL);

	if (!dir)
		return;

	pre = g_hash_table_new_full (g_str_hash, g_str_equal,
				     (GDestroyNotify) g_free, 
				     (GDestroyNotify) free_valueinfo);

	for (filen = g_dir_read_name (dir); filen; filen = g_dir_read_name (dir)) {
		GKeyFile *keyfile;
		gchar *fullfilen;
		gchar *value;
		GStrv values;
		GStrv uri_schemes;
		GError *error = NULL;
		guint i = 0, y = 0;
		guint64 mtime;
		GFileInfo *info;
		GFile *file;

		/* If the file is the 'overrides', skip it (we'll deal with that later) */

		if (strcmp (filen, "overrides") == 0) {
			has_override = TRUE;
			continue;
		}

		fullfilen = g_build_filename (path, filen, NULL);
		keyfile = g_key_file_new ();

		/* If we can't parse it as a key-value file, skip */
	
		if (!g_key_file_load_from_file (keyfile, fullfilen, G_KEY_FILE_NONE, NULL)) {
			g_free (fullfilen);
			g_key_file_free (keyfile);
			continue;
		}

		value = g_key_file_get_string (keyfile, "D-BUS Thumbnailer", "Name", NULL);

		/* If it doesn't have the required things, skip */

		if (!value) {
			g_free (fullfilen);
			g_key_file_free (keyfile);
			continue;
		}

		values = g_key_file_get_string_list (keyfile, "D-BUS Thumbnailer", "MimeTypes", NULL, NULL);

		/* If it doesn't have the required things, skip */

		if (!values) {
			g_free (fullfilen);
			g_free (value);
			g_key_file_free (keyfile);
			continue;
		}

		/* Get the supported uri-schemes, if none we default to just `file` */

		uri_schemes = g_key_file_get_string_list (keyfile, "D-BUS Thumbnailer", "UriSchemes", NULL, NULL);

		if (!uri_schemes) {
			uri_schemes = g_new0 (gchar*, 2);
			uri_schemes[0] = g_strdup ("file");
			uri_schemes[1] = NULL;
		}

		/* Else, get the modificiation time, we'll need it later */

		file = g_file_new_for_path (fullfilen);

		g_free (fullfilen);

		info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED,
					  G_FILE_QUERY_INFO_NONE,
					  NULL, &error);

		/* If that didn't work out, skip */

		if (error) {
			if (info)
				g_object_unref (info);
			if (file)
				g_object_unref (file);
			g_free (value);
			g_strfreev (values);
			g_strfreev (uri_schemes);
			g_key_file_free (keyfile);
			g_clear_error (&error);
			continue;
		}

		mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

		/* And register it in the temporary hashtable that is being formed */

		y = 0;

		while (uri_schemes[y] != NULL) {

		  i = 0;

		  while (values[i] != NULL) {
			ValueInfo *vi;

			vi = g_hash_table_lookup (pre, values[i]);

			if (!vi || vi->mtime < mtime) {
				vi = g_slice_new0 (ValueInfo);

				vi->name = g_strdup (value);

				/* The modification time of the thumbnailer-service file is
				 * used, as specified, to determine the priority. We simply 
				 * override older-ones with newer-ones in the hashtable (using 
				 * replace). */
				vi->mtime = mtime;

				/* Only items in overrides are prioritized. */
				vi->prio = FALSE;

				g_hash_table_replace (pre, 
						      g_strdup_printf ("%s-%s", uri_schemes[y], values[i]), 
						      vi);
			}

			i++;
		  }
		  y++;
		}

		if (info)
			g_object_unref (info);
		if (file)
			g_object_unref (file);

		g_free (value);
		g_strfreev (uri_schemes);
		g_strfreev (values);
		g_key_file_free (keyfile);
	}

	g_dir_close (dir);

	if (has_override) {
		GKeyFile *keyfile;
		gchar *fullfilen = g_build_filename (path, "overrides", NULL);
		gsize length;

		keyfile = g_key_file_new ();

		if (g_key_file_load_from_file (keyfile, fullfilen, G_KEY_FILE_NONE, NULL)) {
			gchar **urisch_and_mimes = g_key_file_get_groups (keyfile, &length);
			guint i;

			for (i = 0; i< length; i++) {
				ValueInfo *vi = g_slice_new0 (ValueInfo);

				vi->name = g_key_file_get_string (keyfile, urisch_and_mimes[i], 
								  "Name", NULL);

				/* This is atm unused for items in overrides. */

				vi->mtime = time (NULL);

				/* Items in overrides are prioritized. */

				vi->prio = TRUE;

				g_hash_table_replace (pre, g_strdup (urisch_and_mimes[i]), 
						      vi);
			}
			g_strfreev (urisch_and_mimes);
		}

		g_free (fullfilen);
		g_key_file_free (keyfile);
	}

	g_hash_table_iter_init (&iter, pre);

	while (g_hash_table_iter_next (&iter, &pkey, &pvalue))  {
		gchar *k = pkey;
		ValueInfo *v = pvalue;
		gchar *oname = NULL;

		/* If this is a prioritized one, we'll always override the older. If we
		 * are in overriding mode, we'll also override. We override by looking
		 * up the service-name for the MIME-type and we put that in oname, which
		 * stands for original-name. */

		if (!v->prio && !override) {
			DBusGProxy *proxy = g_hash_table_lookup (priv->handlers, k);
			if (proxy)
				oname = (gchar *) dbus_g_proxy_get_bus_name (proxy);
		}

		/* Now if the original name is set (we'll override) and if the new name
		 * is different than the original-name (else there's no point in 
		 * overriding anything, als the proxy will point to the same thing 
		 * anyway), we add it (adding here means overriding, as replace is used
		 * on the hashtable). */

		if (!oname || g_ascii_strcasecmp (v->name, oname) != 0)
			thumbnail_manager_add (object, k, v->name);
	}

	g_hash_table_unref (pre);
}


static void
on_dir_changed (GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data)
{
	switch (event_type) 
	{
		case G_FILE_MONITOR_EVENT_CHANGED:
		case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
		case G_FILE_MONITOR_EVENT_DELETED:
		case G_FILE_MONITOR_EVENT_CREATED: {
			ThumbnailManager *object = user_data;
			ThumbnailManagerPrivate *priv = THUMBNAIL_MANAGER_GET_PRIVATE (object);
			gchar *path = g_file_get_path (file);
			gboolean override = (strcmp (THUMBNAILERS_DIR, path) == 0);

			g_mutex_lock (priv->mutex);
			/* We override when it's the dir in the user's homedir*/
			thumbnail_manager_check_dir (object, path, override);
			g_mutex_unlock (priv->mutex);

			g_free (path);
		} 
		break;
		default:
		break;
	}
}

static void
thumbnail_manager_check (ThumbnailManager *object)
{
	ThumbnailManagerPrivate *priv = THUMBNAIL_MANAGER_GET_PRIVATE (object);

	gchar *home_thumbnlrs = g_build_filename (g_get_user_data_dir (), 
		"thumbnailers", NULL);

	g_mutex_lock (priv->mutex);

	/* We override when it's the one in the user's homedir*/
	thumbnail_manager_check_dir (object, THUMBNAILERS_DIR, FALSE);
	thumbnail_manager_check_dir (object, home_thumbnlrs, TRUE);

	/* Monitor the dir for changes */
	homedir = g_file_new_for_path (home_thumbnlrs);
	homemon =  g_file_monitor_directory (homedir, G_FILE_MONITOR_NONE, NULL, NULL);
	g_signal_connect (G_OBJECT (homemon), "changed", 
			  G_CALLBACK (on_dir_changed), object);

	/* Monitor the dir for changes */
	thumbdir = g_file_new_for_path (THUMBNAILERS_DIR);
	thumbmon =  g_file_monitor_directory (thumbdir, G_FILE_MONITOR_NONE, NULL, NULL);
	g_signal_connect (G_OBJECT (thumbmon), "changed", 
			  G_CALLBACK (on_dir_changed), object);

	g_mutex_unlock (priv->mutex);

	g_free (home_thumbnlrs);
}


static gboolean 
do_remove_or_not (gpointer key, gpointer value, gpointer user_data)
{
	if (user_data == value)
		return TRUE;
	return FALSE;
}

static void
service_gone (DBusGProxy *proxy, ThumbnailManager *object)
{
	ThumbnailManagerPrivate *priv = THUMBNAIL_MANAGER_GET_PRIVATE (object);

	g_mutex_lock (priv->mutex);

	/* This only happens for not-activable ones: if the service disappears, we
	 * unregister it. Note that this is actually only for our plugin-runner to
	 * work correctly (not to implement any specification related things) */

	g_hash_table_foreach_remove (priv->handlers, 
				     do_remove_or_not,
				     proxy);

	g_mutex_unlock (priv->mutex);
}

/* This is a custom spec addition, for dynamic registration of thumbnailers.
 * Consult manager.xml for more information about this custom spec addition. */

void
thumbnail_manager_register (ThumbnailManager *object, gchar *uri_scheme, gchar *mime_type, DBusGMethodInvocation *context)
{
	ThumbnailManagerPrivate *priv = THUMBNAIL_MANAGER_GET_PRIVATE (object);
	DBusGProxy *mime_proxy;
	gchar *sender;
	gchar *query;

	dbus_async_return_if_fail (mime_type != NULL, context);
	dbus_async_return_if_fail (uri_scheme != NULL, context);

	query = g_strdup_printf ("%s-%s", uri_scheme, mime_type);

	keep_alive ();

	g_mutex_lock (priv->mutex);

	sender = dbus_g_method_get_sender (context);

	thumbnail_manager_add (object, query, sender);

	g_free (sender);

	/* This is not necessary for activatable ones */

	mime_proxy = g_hash_table_lookup (priv->handlers, query);

	g_free (query);

	g_signal_connect (mime_proxy, "destroy",
			  G_CALLBACK (service_gone),
			  object);

	g_mutex_unlock (priv->mutex);

	dbus_g_method_return (context);
}

/* A function for letting the thumbnail_manager know what mime-types we already deal with
 * ourselves outside of the thumbnail_manager's registration procedure (internal plugins) */

void 
thumbnail_manager_i_have (ThumbnailManager *object, const gchar *mime_type)
{
	ThumbnailManagerPrivate *priv = THUMBNAIL_MANAGER_GET_PRIVATE (object);
	GList *list;
	gboolean found = FALSE;

	g_mutex_lock (priv->mutex);
	list = priv->thumber_has;
	while (list) {
		if (strcmp (list->data, mime_type) == 0) {
			found = TRUE;
			break;
		}
		list = g_list_next (list);
	}
	if (!found)
		priv->thumber_has = g_list_prepend (priv->thumber_has, 
						    g_strdup (mime_type));
	g_mutex_unlock (priv->mutex);
}


/* This is a custom spec addition, for dynamic registration of thumbnailers.
 * Consult thumbnail_manager.xml for more information about this custom spec addition. */

void
thumbnail_manager_get_supported (ThumbnailManager *object, DBusGMethodInvocation *context)
{
	ThumbnailManagerPrivate *priv = THUMBNAIL_MANAGER_GET_PRIVATE (object);
	GStrv supported;
	GHashTable *supported_h;
	GHashTableIter iter;
	gpointer key, value;
	GList *copy, *l;
	guint y;

	keep_alive ();

	supported_h = g_hash_table_new_full (g_str_hash, g_str_equal,
					     (GDestroyNotify) g_free, 
					     (GDestroyNotify) NULL);

	g_mutex_lock (priv->mutex);
	copy = priv->thumber_has;
	while (copy) {
		g_hash_table_replace (supported_h, g_strdup (copy->data), NULL);
		copy = g_list_next (copy);
	}

	copy = g_hash_table_get_keys (priv->handlers);
	for (l = copy; l; l = l->next) {
		gchar *mime = g_strdup (l->data), *ptr;

		/* We stored it in the hash as "vfs-mime/type" */
		ptr = strchr (mime, '-');

		if (ptr) {
			*ptr = '\0';
			ptr++;
		} else {
			ptr = mime;
		}

		g_hash_table_replace (supported_h, g_strdup (ptr), NULL);

		g_free (mime);

		copy = g_list_next (copy);
	}
	g_list_free (copy);

	g_mutex_unlock (priv->mutex);

	g_hash_table_iter_init (&iter, supported_h);

	supported = (GStrv) g_malloc0 (sizeof (gchar *) * (g_hash_table_size (supported_h) + 1));

	y = 0;
	while (g_hash_table_iter_next (&iter, &key, &value))  {
		supported[y] = g_strdup (key);
		y++;
	}

	dbus_g_method_return (context, supported);

	g_strfreev (supported);
	g_hash_table_unref (supported_h);
}

static void
thumbnail_manager_finalize (GObject *object)
{
	ThumbnailManagerPrivate *priv = THUMBNAIL_MANAGER_GET_PRIVATE (object);

	if (priv->thumber_has) {
		g_list_foreach (priv->thumber_has, (GFunc) g_free, NULL);
		g_list_free (priv->thumber_has);
	}
	g_hash_table_unref (priv->handlers);
	g_mutex_free (priv->mutex);

	G_OBJECT_CLASS (thumbnail_manager_parent_class)->finalize (object);
}

static void 
thumbnail_manager_set_connection (ThumbnailManager *object, DBusGConnection *connection)
{
	ThumbnailManagerPrivate *priv = THUMBNAIL_MANAGER_GET_PRIVATE (object);
	priv->connection = connection;
}


static void
thumbnail_manager_set_property (GObject      *object,
		      guint         prop_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_CONNECTION:
		thumbnail_manager_set_connection (THUMBNAIL_MANAGER (object),
					g_value_get_pointer (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}


static void
thumbnail_manager_get_property (GObject    *object,
		      guint       prop_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	ThumbnailManagerPrivate *priv;

	priv = THUMBNAIL_MANAGER_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_CONNECTION:
		g_value_set_pointer (value, priv->connection);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
thumbnail_manager_class_init (ThumbnailManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	dbus_g_object_register_marshaller (thumbnailer_marshal_VOID__STRING_INT_STRING,
					   G_TYPE_NONE,
					   G_TYPE_STRING,
					   G_TYPE_INT,
					   G_TYPE_STRING,
					   G_TYPE_INVALID);

	object_class->finalize = thumbnail_manager_finalize;
	object_class->set_property = thumbnail_manager_set_property;
	object_class->get_property = thumbnail_manager_get_property;

	g_object_class_install_property (object_class,
					 PROP_CONNECTION,
					 g_param_spec_pointer ("connection",
							       "DBus connection",
							       "DBus connection",
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (ThumbnailManagerPrivate));
}

static void
thumbnail_manager_init (ThumbnailManager *object)
{
	ThumbnailManagerPrivate *priv = THUMBNAIL_MANAGER_GET_PRIVATE (object);

	priv->mutex = g_mutex_new ();
	priv->thumber_has = NULL;
	priv->handlers = g_hash_table_new_full (g_str_hash, g_str_equal,
						(GDestroyNotify) g_free, 
						(GDestroyNotify) g_object_unref);
}

void 
thumbnail_manager_do_stop (void)
{
	g_object_unref (homemon);
	g_object_unref (thumbmon);
	g_object_unref (homedir);
	g_object_unref (thumbdir);
}

void 
thumbnail_manager_do_init (DBusGConnection *connection, ThumbnailManager **thumbnail_manager, GError **error)
{
	guint result;
	DBusGProxy *proxy;
	GObject *object;

	proxy = dbus_g_proxy_new_for_name (connection, 
					   DBUS_SERVICE_DBUS,
					   DBUS_PATH_DBUS,
					   DBUS_INTERFACE_DBUS);

	org_freedesktop_DBus_request_name (proxy, MANAGER_SERVICE,
					   DBUS_NAME_FLAG_DO_NOT_QUEUE,
					   &result, error);

	g_object_unref (proxy);

	object = g_object_new (TYPE_THUMBNAIL_MANAGER, 
			       "connection", connection,
			       NULL);

	thumbnail_manager_check (THUMBNAIL_MANAGER (object));

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), 
					 &dbus_glib_manager_object_info);

	dbus_g_connection_register_g_object (connection, 
					     MANAGER_PATH, 
					     object);

	*thumbnail_manager = THUMBNAIL_MANAGER (object);
}
