#include <glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <hildon-thumbnail-factory.h>

#include "default.h"
#include "image-png-glue.h"
#include "hildon-thumbnail-plugin.h"

typedef struct ImagePng ImagePng;
typedef struct ImagePngClass ImagePngClass;

struct ImagePng {
	GObject parent;
};

struct ImagePngClass {
	GObjectClass parent;
};

#define DEFAULT_PNG_SERVICE      "org.freedesktop.Thumbnailer.image-png"
#define DEFAULT_PNG_PATH         "/org/freedesktop/Thumbnailer/image-png"
#define DEFAULT_PNG_INTERFACE    "org.freedesktop.Thumbnailer.image-png"

void
image_png_create (GObject *object, GStrv urls, DBusGMethodInvocation *context)
{
}

void
image_png_move (GObject *object, GStrv from_urls, GStrv to_urls, DBusGMethodInvocation *context)
{
}

void
image_png_delete (GObject *object, GStrv urls, DBusGMethodInvocation *context)
{
}


static void
image_png_class_init (ImagePngClass *klass)
{
}

static void
image_png_init (ImagePng *object)
{
}


G_DEFINE_TYPE(ImagePng, image_png, G_TYPE_OBJECT)

void 
hildon_thumbnail_plugin_stop (void)
{
}


void 
hildon_thumbnail_plugin_init (DBusGConnection *connection, GError **error)
{
	guint result;
	DBusGProxy *proxy;
	GObject *object;

	proxy = dbus_g_proxy_new_for_name (connection, 
					   DBUS_SERVICE_DBUS,
					   DBUS_PATH_DBUS,
					   DBUS_INTERFACE_DBUS);

	org_freedesktop_DBus_request_name (proxy, DEFAULT_PNG_SERVICE,
					   DBUS_NAME_FLAG_DO_NOT_QUEUE,
					   &result, error);

	object = g_object_new (image_png_get_type (), NULL);

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), &dbus_glib_image_png_object_info);
	dbus_g_connection_register_g_object (connection, DEFAULT_PNG_PATH, object);

}
