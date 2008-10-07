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

#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <dbus/dbus-glib-bindings.h>

#include "hildon-thumbnail-plugin.h"

#define MANAGER_SERVICE        "org.freedesktop.thumbnailer"
#define MANAGER_PATH           "/org/freedesktop/thumbnailer/Manager"
#define MANAGER_INTERFACE      "org.freedesktop.thumbnailer.Manager"

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
	GModule *module;
	gboolean cropping;
} DaemonPrivate;

enum {
	PROP_0,
	PROP_CONNECTION,
	PROP_MODULE
};

G_DEFINE_TYPE (Daemon, daemon, G_TYPE_OBJECT)

#define DAEMON_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TYPE_DAEMON, DaemonPrivate))

#define plugin_runner_create daemon_create


static gboolean do_shut_down_next_time = TRUE;

static void
keep_alive (void) 
{
	do_shut_down_next_time = FALSE;
}

static gboolean
shut_down_after_timeout (gpointer user_data)
{
	GMainLoop *main_loop =  user_data;
	gboolean shut = FALSE;

	if (do_shut_down_next_time) {
		g_main_loop_quit (main_loop);
		shut = TRUE;
	} else
		do_shut_down_next_time = TRUE;

	return shut;
}

void 
daemon_create (Daemon *object, GStrv uris, DBusGMethodInvocation *context)
{
	DaemonPrivate *priv = DAEMON_GET_PRIVATE (object);
	GError *error = NULL;

	keep_alive ();

	hildon_thumbnail_plugin_do_create (priv->module, uris, &error);
	if (error) {
		dbus_g_method_return_error (context, error);
		g_error_free (error);
	} else
		dbus_g_method_return (context);
}

#include "plugin-runner-glue.h"


static void 
daemon_set_connection (Daemon *object, DBusGConnection *connection)
{
	DaemonPrivate *priv = DAEMON_GET_PRIVATE (object);
	priv->connection = connection;
}

static void 
daemon_set_module (Daemon *object, GModule *module)
{
	DaemonPrivate *priv = DAEMON_GET_PRIVATE (object);
	priv->module = module;
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
	case PROP_MODULE:
		daemon_set_module (DAEMON (object),
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
	case PROP_MODULE:
		g_value_set_pointer (value, priv->module);
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


	g_object_class_install_property (object_class,
					 PROP_MODULE,
					 g_param_spec_pointer ("module",
							       "Module to proxy to",
							       "Module to proxy to",
							      G_PARAM_READWRITE |
							      G_PARAM_CONSTRUCT));

	g_type_class_add_private (object_class, sizeof (DaemonPrivate));
}

static void
daemon_init (Daemon *object)
{
}


static void 
daemon_register_func (gpointer self, const gchar *mime_type, GModule *module)
{
	GError *nerror = NULL;

	dbus_g_proxy_call (self, "Register",
			   &nerror, G_TYPE_STRING,
			   mime_type,
			   G_TYPE_INVALID,
			   G_TYPE_INVALID);

	if (nerror) {
		g_critical ("Failed to init: %s\n", nerror->message);
		g_error_free (nerror);
	}
}

static void
daemon_start (Daemon *object, gboolean do_register)
{
	GError *error = NULL;
	DaemonPrivate *priv = DAEMON_GET_PRIVATE (object);
	GModule *module = priv->module;
	DBusGProxy *manager_proxy;

	manager_proxy = dbus_g_proxy_new_for_name (priv->connection, 
						   MANAGER_SERVICE,
						   MANAGER_PATH,
						   MANAGER_INTERFACE);

	hildon_thumbnail_plugin_do_init (module, &priv->cropping, 
					 daemon_register_func, 
					 manager_proxy, &error);


	g_object_unref (manager_proxy);

	if (error) {
		g_critical ("Failed to init: %s\n", error->message);
		g_error_free (error);
	}

}

static gchar *module_name;
static gboolean dynamic_register = FALSE;
static gchar *bus_name;
static gchar *bus_path;
static gint timeout = 600;

static GOptionEntry entries_daemon[] = {
	{ "module-name", 'm', G_OPTION_FLAG_REVERSE|G_OPTION_FLAG_OPTIONAL_ARG, 
	  G_OPTION_ARG_STRING, &module_name, 
	  "Module to load (eg. gdkpixbuif) ", 
	  NULL },
	{ "bus-name", 'b', 0, 
	  G_OPTION_ARG_STRING, &bus_name, 
	  "Busname to use (eg. com.company.Thumbnailer) ", 
	  NULL },
	{ "timeout", 't', 0, 
	  G_OPTION_ARG_INT, &timeout, 
	  "Timeout before the specialized thumbnailer dies (use -1 for unlimited)", 
	  NULL },
	{ "bus-path", 'p', 0, 
	  G_OPTION_ARG_STRING, &bus_path, 
	  "Buspath to use (eg. /com/company/Thumbnailer) ", 
	  NULL },
	{ "dynamic-register", 'd', 0, 
	  G_OPTION_ARG_NONE, &dynamic_register, 
	  "Dynamic registration using org.freedesktop.Thumbnailer.Manager", 
	  NULL },
	{ NULL }
};

int 
main (int argc, char **argv) 
{
	GOptionContext *context = NULL;
	GOptionGroup *group;
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *error = NULL;
	guint result;
	GMainLoop *main_loop;
	GObject *object;
	GModule *module;

	g_type_init ();

	if (!g_thread_supported ())
		g_thread_init (NULL);

	context = g_option_context_new ("- start a plugin as a standalone daemon");

	/* Daemon group */
	group = g_option_group_new ("daemon", 
				    "Daemon Options",
				    "Show daemon options", 
				    NULL, 
				    NULL);
	g_option_group_add_entries (group, entries_daemon);
	g_option_context_add_group (context, group);

	g_option_context_parse (context, &argc, &argv, &error);
	g_option_context_free (context);

	if (error) {
		g_printerr ("Invalid arguments, %s\n", error->message);
		g_error_free (error);
		return -1;
	}

	module = hildon_thumbnail_plugin_load (module_name);

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	proxy = dbus_g_proxy_new_for_name (connection, 
					   DBUS_SERVICE_DBUS,
					   DBUS_PATH_DBUS,
					   DBUS_INTERFACE_DBUS);

	org_freedesktop_DBus_request_name (proxy, bus_name,
					   DBUS_NAME_FLAG_DO_NOT_QUEUE,
					   &result, &error);

	object = g_object_new (TYPE_DAEMON, 
			       "connection", connection, 
			       "module", module,
			       NULL);

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), 
					 &dbus_glib_plugin_runner_object_info);

	dbus_g_connection_register_g_object (connection, 
					     bus_path, 
					     object);

	daemon_start (DAEMON (object), dynamic_register);

	if (timeout > -1)
		g_timeout_add_seconds (timeout, 
				       shut_down_after_timeout,
				       main_loop);

	main_loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (main_loop);

	g_main_loop_unref (main_loop);

	return 0;
}
