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
#include <dbus/dbus-glib-bindings.h>

#include "manager.h"
#include "manager-glue.h"
#include "dbus-utils.h"
#include "thumbnailer.h"

#define MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TYPE_MANAGER, ManagerPrivate))

G_DEFINE_TYPE (Manager, manager, G_TYPE_OBJECT)

#ifndef dbus_g_method_get_sender
gchar* dbus_g_method_get_sender (DBusGMethodInvocation *context);
#endif

typedef struct {
	DBusGConnection *connection;
	GHashTable *handlers;
	GMutex *mutex;
} ManagerPrivate;

enum {
	PROP_0,
	PROP_CONNECTION
};

DBusGProxy*
manager_get_handler (Manager *object, const gchar *mime_type)
{
	ManagerPrivate *priv = MANAGER_GET_PRIVATE (object);
	DBusGProxy *proxy;

	g_mutex_lock (priv->mutex);
	proxy = g_hash_table_lookup (priv->handlers, mime_type);
	if (proxy)
		g_object_ref (proxy);
	g_mutex_unlock (priv->mutex);

	return proxy;
}


static gboolean 
do_remove_or_not (gpointer key, gpointer value, gpointer user_data)
{
	if (user_data == value)
		return TRUE;
	return FALSE;
}

static void
service_gone (DBusGProxy *proxy,
	      Manager *object)
{
	ManagerPrivate *priv = MANAGER_GET_PRIVATE (object);

	g_mutex_lock (priv->mutex);

	g_hash_table_foreach_remove (priv->handlers, 
				     do_remove_or_not,
				     proxy);

	g_mutex_unlock (priv->mutex);
}

void
manager_register (Manager *object, gchar *mime_type, DBusGMethodInvocation *context)
{
	ManagerPrivate *priv = MANAGER_GET_PRIVATE (object);
	DBusGProxy *mime_proxy;
	gchar *sender;

	dbus_async_return_if_fail (mime_type != NULL, context);

	g_mutex_lock (priv->mutex);

	sender = dbus_g_method_get_sender (context);

	mime_proxy = dbus_g_proxy_new_for_name (priv->connection, sender, 
						THUMBNAILER_PATH,
						THUMBNAILER_INTERFACE);

	g_hash_table_insert (priv->handlers, 
			     mime_type,
			     mime_proxy);

	g_free (sender);

	g_signal_connect (mime_proxy, "destroy",
			  G_CALLBACK (service_gone),
			  object);

	g_mutex_unlock (priv->mutex);
}

static void
manager_finalize (GObject *object)
{
	ManagerPrivate *priv = MANAGER_GET_PRIVATE (object);

	g_hash_table_unref (priv->handlers);
	g_mutex_free (priv->mutex);

	G_OBJECT_CLASS (manager_parent_class)->finalize (object);
}

static void 
manager_set_connection (Manager *object, DBusGConnection *connection)
{
	ManagerPrivate *priv = MANAGER_GET_PRIVATE (object);
	priv->connection = connection;
}


static void
manager_set_property (GObject      *object,
		      guint         prop_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_CONNECTION:
		manager_set_connection (MANAGER (object),
					g_value_get_pointer (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}


static void
manager_get_property (GObject    *object,
		      guint       prop_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	ManagerPrivate *priv;

	priv = MANAGER_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_CONNECTION:
		g_value_set_pointer (value, priv->connection);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
manager_class_init (ManagerClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = manager_finalize;
	object_class->set_property = manager_set_property;
	object_class->get_property = manager_get_property;

	g_object_class_install_property (object_class,
					 PROP_CONNECTION,
					 g_param_spec_pointer ("connection",
							       "DBus connection",
							       "DBus connection",
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (ManagerPrivate));
}

static void
manager_init (Manager *object)
{
	ManagerPrivate *priv = MANAGER_GET_PRIVATE (object);

	priv->mutex = g_mutex_new ();
	priv->handlers = g_hash_table_new_full (g_str_hash, g_str_equal,
						(GDestroyNotify) g_free, 
						(GDestroyNotify) g_object_unref);

}

void 
manager_do_stop (void)
{
}

void 
manager_do_init (DBusGConnection *connection, Manager **manager, GError **error)
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

	object = g_object_new (TYPE_MANAGER, 
			       "connection", connection,
			       NULL);

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), 
					 &dbus_glib_manager_object_info);

	dbus_g_connection_register_g_object (connection, 
					     MANAGER_PATH, 
					     object);

	*manager = MANAGER (object);
}
