#include <string.h>
#include <glib.h>
#include <dbus/dbus-glib-bindings.h>

#include "manager.h"
#include "manager-glue.h"


#define MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TYPE_MANAGER, ManagerPrivate))

G_DEFINE_TYPE (Manager, manager, G_TYPE_OBJECT)

#ifndef dbus_g_method_get_sender
gchar* dbus_g_method_get_sender (DBusGMethodInvocation *context);
#endif

typedef struct {
	DBusGProxy *proxy;
	DBusGConnection *connection;
	GHashTable *handlers;
} ManagerPrivate;

enum {
	PROP_0,
	PROP_PROXY,
	PROP_CONNECTION
};

DBusGProxy*
manager_get_handler (Manager *object, const gchar *mime_type)
{
	ManagerPrivate *priv = MANAGER_GET_PRIVATE (object);
	DBusGProxy *proxy = g_hash_table_lookup (priv->handlers, mime_type);
	if (proxy)
		g_object_ref (proxy);
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

	g_hash_table_foreach_remove (priv->handlers, 
				     do_remove_or_not,
				     proxy);

}

void
manager_register (Manager *object, gchar *mime_type, DBusGMethodInvocation *context)
{
	ManagerPrivate *priv = MANAGER_GET_PRIVATE (object);
	DBusGProxy *mime_proxy;
	gchar *sender = dbus_g_method_get_sender (context);

	mime_proxy = dbus_g_proxy_new_for_name (priv->connection, sender, 
						MANAGER_PATH,
						MANAGER_INTERFACE);

	g_hash_table_insert (priv->handlers, 
			     mime_type,
			     mime_proxy);

	g_free (sender);

	g_signal_connect (mime_proxy, "destroy",
			  G_CALLBACK (service_gone),
			  object);
}

static void
manager_finalize (GObject *object)
{
	ManagerPrivate *priv = MANAGER_GET_PRIVATE (object);

	g_hash_table_unref (priv->handlers);
	g_object_unref (priv->proxy);
	g_object_unref (priv->connection);

	G_OBJECT_CLASS (manager_parent_class)->finalize (object);
}

static void 
manager_set_connection (Manager *object, DBusGConnection *connection)
{
	ManagerPrivate *priv = MANAGER_GET_PRIVATE (object);
	priv->connection = connection;
}

static void 
manager_set_proxy (Manager *object, DBusGProxy *proxy)
{
	ManagerPrivate *priv = MANAGER_GET_PRIVATE (object);

	if (priv->proxy)
		g_object_unref (priv->proxy);
	priv->proxy = g_object_ref (proxy);
}

static void
manager_set_property (GObject      *object,
		      guint         prop_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_PROXY:
		manager_set_proxy (MANAGER (object),
				   g_value_get_object (value));
		break;
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
	case PROP_PROXY:
		g_value_set_object (value, priv->proxy);
		break;
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
					 PROP_PROXY,
					 g_param_spec_object ("proxy",
							      "DBus proxy",
							      "DBus proxy",
							      DBUS_TYPE_G_PROXY,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT));

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
			       "proxy", proxy, 
			       "connection", connection,
			       NULL);

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), 
					 &dbus_glib_manager_object_info);

	dbus_g_connection_register_g_object (connection, 
					     MANAGER_PATH, 
					     object);

	*manager = MANAGER (object);
}
