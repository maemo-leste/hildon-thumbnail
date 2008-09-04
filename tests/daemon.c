#include <glib.h>
#include <gio/gio.h>
#include <dbus/dbus-glib-bindings.h>


#define DAEMON_SERVICE      "org.freedesktop.Thumbnailer"
#define DAEMON_PATH         "/org/freedesktop/Thumbnailer"
#define DAEMON_INTERFACE    "org.freedesktop.Thumbnailer"

#define MANAGER_SERVICE      "org.freedesktop.Thumbnailer"
#define MANAGER_PATH         "/org/freedesktop/Thumbnailer/Manager"
#define MANAGER_INTERFACE    "org.freedesktop.Thumbnailer.Manager"

#define TYPE_DAEMON             (daemon_get_type())
#define DAEMON(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_DAEMON, Daemon))
#define DAEMON_CLASS(c)         (G_TYPE_CHECK_CLASS_CAST ((c), TYPE_DAEMON, DaemonClass))
#define DAEMON_GET_CLASS(o)     (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_DAEMON, DaemonClass))

typedef struct Daemon Daemon;
typedef struct DaemonClass DaemonClass;

struct Daemon {
	GObject parent;
};

struct DaemonClass {
	GObjectClass parent;
};

typedef struct {
	DBusGConnection *connection;
} DaemonPrivate;

enum {
	PROP_0,
	PROP_CONNECTION
};

G_DEFINE_TYPE (Daemon, daemon, G_TYPE_OBJECT)

#define DAEMON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TYPE_DAEMON, DaemonPrivate))

void 
daemon_create (Daemon *object, GStrv uris, DBusGMethodInvocation *context)
{
	guint i = 0;

	while (uris[i] != NULL) {
		g_print ("Request for %s\n", uris[i]);
		i++;
	}

	dbus_g_method_return (context);
}

#include "daemon-glue.h"


static void 
daemon_set_connection (Daemon *object, DBusGConnection *connection)
{
	DaemonPrivate *priv = DAEMON_GET_PRIVATE (object);
	priv->connection = connection;
}


static void
daemon_set_property (GObject      *object,
		      guint         prop_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_CONNECTION:
		daemon_set_connection (DAEMON (object),
				       g_value_get_pointer (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}


static void
daemon_get_property (GObject    *object,
		      guint       prop_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	DaemonPrivate *priv;

	priv = DAEMON_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_CONNECTION:
		g_value_set_pointer (value, priv->connection);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
daemon_finalize (GObject *object)
{
	G_OBJECT_CLASS (daemon_parent_class)->finalize (object);
}

static void
daemon_class_init (DaemonClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = daemon_finalize;
	object_class->set_property = daemon_set_property;
	object_class->get_property = daemon_get_property;

	
	g_object_class_install_property (object_class,
					 PROP_CONNECTION,
					 g_param_spec_pointer ("connection",
							       "DBus connection",
							       "DBus connection",
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (DaemonPrivate));
}

static void
daemon_init (Daemon *object)
{
}

static void
daemon_start (Daemon *object)
{
	GError *error = NULL;
	DaemonPrivate *priv = DAEMON_GET_PRIVATE (object);

	DBusGProxy *manager_proxy = dbus_g_proxy_new_for_name (priv->connection, 
					   MANAGER_SERVICE,
					   MANAGER_PATH,
					   MANAGER_INTERFACE);


	dbus_g_proxy_call (manager_proxy, "Register",
			   &error, G_TYPE_STRING,
			   "image/png",
			   G_TYPE_INVALID,
			   G_TYPE_INVALID);

	if (error) {
		g_critical ("Failed to init: %s\n", error->message);
		g_error_free (error);
	}

	g_object_unref (manager_proxy);
	
}


int 
main (int argc, char **argv) 
{
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error = NULL;
	guint result;
	GMainLoop *main_loop;
	GObject *object;

	g_type_init ();

	if (!g_thread_supported ())
		g_thread_init (NULL);

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	proxy = dbus_g_proxy_new_for_name (connection, 
					   DBUS_SERVICE_DBUS,
					   DBUS_PATH_DBUS,
					   DBUS_INTERFACE_DBUS);

	org_freedesktop_DBus_request_name (proxy, DAEMON_SERVICE,
					   DBUS_NAME_FLAG_DO_NOT_QUEUE,
					   &result, error);

	object = g_object_new (TYPE_DAEMON, 
			       "connection", connection, 
			       NULL);

	daemon_start (DAEMON (object));

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), 
					 &dbus_glib_daemon_object_info);

	dbus_g_connection_register_g_object (connection, 
					     DAEMON_PATH, 
					     object);


	main_loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (main_loop);

	g_main_loop_unref (main_loop);

	return 0;
}
