#include <glib.h>
#include <dbus/dbus-glib-bindings.h>

#include "manager.h"
#include "thumbnailer.h"
#include "thumbnailer-glue.h"

#define THUMBNAILER_SERVICE      "org.freedesktop.thumbnailer"
#define THUMBNAILER_PATH         "/org/freedesktop/thumbnailer"
#define THUMBNAILER_INTERFACE    "org.freedesktop.thumbnailer"


#define THUMBNAILER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TYPE_THUMBNAILER, ThumbnailerPrivate))

G_DEFINE_TYPE (Thumbnailer, thumbnailer, G_TYPE_OBJECT)

typedef struct {
	DBusGProxy *proxy;
	DBusGConnection *connection;
	Manager *manager;
} ThumbnailerPrivate;

enum {
	PROP_0,
	PROP_PROXY,
	PROP_CONNECTION,
	PROP_MANAGER
};


void
thumbnailer_create (Thumbnailer *object, GStrv urls, DBusGMethodInvocation *context)
{
}

void
thumbnailer_move (Thumbnailer *object, GStrv from_urls, GStrv to_urls, DBusGMethodInvocation *context)
{
}

void
thumbnailer_delete (Thumbnailer *object, GStrv urls, DBusGMethodInvocation *context)
{
}

static void
thumbnailer_finalize (GObject *object)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

	g_object_unref (priv->manager);
	g_object_unref (priv->proxy);
	g_object_unref (priv->connection);

	G_OBJECT_CLASS (thumbnailer_parent_class)->finalize (object);
}

static void 
thumbnailer_set_connection (Thumbnailer *object, DBusGConnection *connection)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);
	priv->connection = connection;
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
thumbnailer_set_proxy (Thumbnailer *object, DBusGProxy *proxy)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

	if (priv->proxy)
		g_object_unref (priv->proxy);
	priv->proxy = g_object_ref (proxy);
}

static void
thumbnailer_set_property (GObject      *object,
		      guint         prop_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_PROXY:
		thumbnailer_set_proxy (THUMBNAILER (object),
				   g_value_get_object (value));
		break;
	case PROP_CONNECTION:
		thumbnailer_set_connection (THUMBNAILER (object),
					    g_value_get_pointer (value));
		break;
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
	case PROP_PROXY:
		g_value_set_object (value, priv->proxy);
		break;
	case PROP_CONNECTION:
		g_value_set_pointer (value, priv->connection);
		break;
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

	g_object_class_install_property (object_class,
					 PROP_MANAGER,
					 g_param_spec_object ("manager",
							      "Manager",
							      "Manager",
							      TYPE_MANAGER,
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (ThumbnailerPrivate));
}

static void
thumbnailer_init (Thumbnailer *object)
{

}



void 
thumbnailer_do_stop (void)
{
}


void 
thumbnailer_do_init (DBusGConnection *connection, Manager *manager, GError **error)
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
			       "proxy", proxy, 
			       "connection", connection,
			       "manager", manager,
			       NULL);

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), 
					 &dbus_glib_thumbnailer_object_info);

	dbus_g_connection_register_g_object (connection, THUMBNAILER_PATH, object);

}
