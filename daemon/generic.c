#include <glib.h>
#include <dbus/dbus-glib-bindings.h>

#include "generic.h"
#include "generic-glue.h"

#define GENERIC_SERVICE      "org.freedesktop.thumbnailer.generic"
#define GENERIC_PATH         "/org/freedesktop/thumbnailer/generic"
#define GENERIC_INTERFACE    "org.freedesktop.thumbnailer.generic"

void
generic_create (Generic *object, GStrv urls, DBusGMethodInvocation *context)
{
}

void
generic_move (Generic *object, GStrv from_urls, GStrv to_urls, DBusGMethodInvocation *context)
{
}

void
generic_delete (Generic *object, GStrv urls, DBusGMethodInvocation *context)
{
}


static void
generic_class_init (GenericClass *klass)
{
}

static void
generic_init (Generic *object)
{
}


G_DEFINE_TYPE(Generic, generic, G_TYPE_OBJECT)

void 
generic_do_stop (void)
{
}


void 
generic_do_init (DBusGConnection *connection, GError **error)
{
	guint result;
	DBusGProxy *proxy;
	GObject *object;

	proxy = dbus_g_proxy_new_for_name (connection, 
					   DBUS_SERVICE_DBUS,
					   DBUS_PATH_DBUS,
					   DBUS_INTERFACE_DBUS);

	org_freedesktop_DBus_request_name (proxy, GENERIC_SERVICE,
					   DBUS_NAME_FLAG_DO_NOT_QUEUE,
					   &result, error);

	object = g_object_new (generic_get_type (), NULL);

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), 
					 &dbus_glib_generic_object_info);

	dbus_g_connection_register_g_object (connection, GENERIC_PATH, object);

}
