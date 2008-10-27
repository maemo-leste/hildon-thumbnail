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

#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <dbus/dbus-glib-bindings.h>

#include "albumart-manager.h"
#include "albumart.h"

static GFile *artdir;
static GFileMonitor *artmon;

#define ALBUMART_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TYPE_ALBUMART_MANAGER, AlbumartManagerPrivate))

G_DEFINE_TYPE (AlbumartManager, albumart_manager, G_TYPE_OBJECT)

void keep_alive (void);

typedef struct {
	DBusGConnection *connection;
	GList *handlers;
	GMutex *mutex;
} AlbumartManagerPrivate;

enum {
	PROP_0,
	PROP_CONNECTION
};


typedef struct {
	gchar *name;
	gint prio;
	DBusGProxy *proxy;
} ValueInfo;

static void
free_valueinfo (ValueInfo *info) {
	if (info->proxy)
		g_object_unref (info->proxy);
	g_free (info->name);
	g_slice_free (ValueInfo, info);
}



GList*
albumart_manager_get_handlers (AlbumartManager *object)
{
	AlbumartManagerPrivate *priv = ALBUMART_MANAGER_GET_PRIVATE (object);
	GList *retval = NULL, *copy = priv->handlers;

	g_mutex_lock (priv->mutex);

	while (copy) {
		ValueInfo *info = copy->data;
		retval = g_list_prepend (retval, g_object_ref (info->proxy));
		copy = g_list_next (copy);
	}

	g_mutex_unlock (priv->mutex);

	return retval;
}


static gint 
compar_func (gconstpointer a, gconstpointer b)
{
	ValueInfo *info_a = (ValueInfo *) a;
	ValueInfo *info_b = (ValueInfo *) b;

	return strcmp (info_a->name, info_b->name);
}

static gint 
sort_func (gconstpointer a, gconstpointer b)
{
	ValueInfo *info_a = (ValueInfo *) a;
	ValueInfo *info_b = (ValueInfo *) b;

	return info_a->prio - info_b->prio;
}

static void
albumart_manager_add (AlbumartManager *object, ValueInfo *info)
{
	AlbumartManagerPrivate *priv = ALBUMART_MANAGER_GET_PRIVATE (object);
	gchar *path = g_strdup_printf ("/%s", info->name);
	guint len = strlen (path);
	guint i;
	GList *link;

	/* Not sure if this path stuff makes any sense ... but it works */

	for (i = 0; i< len; i++) {
		if (path[i] == '.')
			path[i] = '/';
	}

	info->proxy = dbus_g_proxy_new_for_name (priv->connection, info->name, 
									   path,
									   PROVIDER_INTERFACE);

	link = g_list_find_custom (priv->handlers, info, compar_func);

	if (link) {
		priv->handlers = g_list_remove_link (priv->handlers, link);
		free_valueinfo (link->data);
		g_list_free (link);
	}

	priv->handlers = g_list_insert_sorted (priv->handlers, info, sort_func);

	g_free (path);

}
static void
albumart_manager_check_dir (AlbumartManager *object, gchar *path, gboolean override)
{
	const gchar *filen;
	GDir *dir;

	dir = g_dir_open (path, 0, NULL);

	if (!dir)
		return;

	for (filen = g_dir_read_name (dir); filen; filen = g_dir_read_name (dir)) {
		GKeyFile *keyfile;
		gchar *fullfilen;
		gchar *value;
		GError *error = NULL;
		guint64 mtime;
		GFileInfo *info;
		GFile *file;
		ValueInfo *v_info;
		gint prio;
		GError *err = NULL;

		fullfilen = g_build_filename (path, filen, NULL);
		keyfile = g_key_file_new ();

		/* If we can't parse it as a key-value file, skip */

		if (!g_key_file_load_from_file (keyfile, fullfilen, G_KEY_FILE_NONE, NULL)) {
			g_free (fullfilen);
			continue;
		}

		value = g_key_file_get_string (keyfile, "D-BUS Album art provider", "Name", NULL);

		/* If it doesn't have the required things, skip */

		if (!value) {
			g_free (fullfilen);
			g_key_file_free (keyfile);
			continue;
		}

		prio = g_key_file_get_integer (keyfile, "D-BUS Album art provider", "Priority", &err);

		if (err) {
			prio = -1;
			g_error_free (err);
		}

		/* Else, get the modificiation time, we'll need it later */

		file = g_file_new_for_path (fullfilen);

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
			g_key_file_free (keyfile);
			g_free (fullfilen);
			continue;
		}

		mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);

		/* And register it in the temporary hashtable that is being formed */

		v_info = g_slice_new (ValueInfo);

		v_info->name = g_strdup (value);
		v_info->prio = prio;
		v_info->proxy = NULL;

		albumart_manager_add (object, v_info);

		g_free (fullfilen);

		if (info)
			g_object_unref (info);
		if (file)
			g_object_unref (file);

		g_free (value);
		g_key_file_free (keyfile);
	}

	g_dir_close (dir);

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
			AlbumartManager *object = user_data;
			AlbumartManagerPrivate *priv = ALBUMART_MANAGER_GET_PRIVATE (object);
			g_mutex_lock (priv->mutex);
			albumart_manager_check_dir (object, ALBUMARTERS_DIR, FALSE);
			g_mutex_unlock (priv->mutex);
		} break;
		default:
		break;
	}
}

static void
albumart_manager_check (AlbumartManager *object)
{
	AlbumartManagerPrivate *priv = ALBUMART_MANAGER_GET_PRIVATE (object);

	g_mutex_lock (priv->mutex);

	albumart_manager_check_dir (object, ALBUMARTERS_DIR, FALSE);

	/* Monitor the dir for changes */
	artdir = g_file_new_for_path (ALBUMARTERS_DIR);
	artmon =  g_file_monitor_directory (artdir, G_FILE_MONITOR_NONE, NULL, NULL);
	g_signal_connect (G_OBJECT (artmon), "changed", 
			  G_CALLBACK (on_dir_changed), object);

	g_mutex_unlock (priv->mutex);
}


static void
albumart_manager_finalize (GObject *object)
{
	AlbumartManagerPrivate *priv = ALBUMART_MANAGER_GET_PRIVATE (object);

	if (priv->handlers) {
		g_list_foreach (priv->handlers, (GFunc) free_valueinfo, NULL);
		g_list_free (priv->handlers);
	}

	g_mutex_free (priv->mutex);

	G_OBJECT_CLASS (albumart_manager_parent_class)->finalize (object);
}

static void 
albumart_manager_set_connection (AlbumartManager *object, DBusGConnection *connection)
{
	AlbumartManagerPrivate *priv = ALBUMART_MANAGER_GET_PRIVATE (object);
	priv->connection = connection;
}


static void
albumart_manager_set_property (GObject      *object,
		      guint         prop_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_CONNECTION:
		albumart_manager_set_connection (ALBUMART_MANAGER (object),
					g_value_get_pointer (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}


static void
albumart_manager_get_property (GObject    *object,
		      guint       prop_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	AlbumartManagerPrivate *priv;

	priv = ALBUMART_MANAGER_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_CONNECTION:
		g_value_set_pointer (value, priv->connection);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
albumart_manager_class_init (AlbumartManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = albumart_manager_finalize;
	object_class->set_property = albumart_manager_set_property;
	object_class->get_property = albumart_manager_get_property;

	g_object_class_install_property (object_class,
					 PROP_CONNECTION,
					 g_param_spec_pointer ("connection",
							       "DBus connection",
							       "DBus connection",
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (AlbumartManagerPrivate));
}

static void
albumart_manager_init (AlbumartManager *object)
{
	AlbumartManagerPrivate *priv = ALBUMART_MANAGER_GET_PRIVATE (object);

	priv->mutex = g_mutex_new ();
	priv->handlers = NULL;
}

void 
albumart_manager_do_stop (void)
{
	g_object_unref (artdir);
	g_object_unref (artdir);
}

void 
albumart_manager_do_init (DBusGConnection *connection, AlbumartManager **albumart_manager, GError **error)
{
	GObject *object;

	object = g_object_new (TYPE_ALBUMART_MANAGER, 
			       "connection", connection,
			       NULL);

	albumart_manager_check (ALBUMART_MANAGER (object));

	*albumart_manager = ALBUMART_MANAGER (object);
}
