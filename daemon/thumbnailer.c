#include <glib.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GIO
#include <gio/gio.h>
#else
#include <libgnomevfs/gnome-vfs.h>
#endif

#include <dbus/dbus-glib-bindings.h>

#include "manager.h"
#include "thumbnailer.h"
#include "thumbnailer-glue.h"
#include "hildon-thumbnail-plugin.h"
#include "dbus-utils.h"

#define THUMBNAILER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TYPE_THUMBNAILER, ThumbnailerPrivate))

G_DEFINE_TYPE (Thumbnailer, thumbnailer, G_TYPE_OBJECT)

typedef struct {
	DBusGProxy *proxy;
	DBusGConnection *connection;
	Manager *manager;
	GHashTable *plugins;
} ThumbnailerPrivate;

enum {
	PROP_0,
	PROP_PROXY,
	PROP_CONNECTION,
	PROP_MANAGER
};


void 
thumbnailer_register_plugin (Thumbnailer *object, const gchar *mime_type, GModule *plugin)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

	g_print ("reg:%s\n", mime_type);
	g_hash_table_insert (priv->plugins, 
			     g_strdup (mime_type), 
			     plugin);
}

static gboolean 
do_delete_or_not (gpointer key, gpointer value, gpointer user_data)
{
	if (value == user_data)
		return TRUE;
	return FALSE;
}

void 
thumbnailer_unregister_plugin (Thumbnailer *object, GModule *plugin)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

	g_hash_table_foreach_remove (priv->plugins, 
				     do_delete_or_not, 
				     plugin);
}

typedef struct {
	DBusGProxy *proxy;
	DBusGMethodInvocation *context;
} ProxyCallInfo;


static void
on_create_finished (DBusGProxy *proxy, DBusGProxyCall *call_id, ProxyCallInfo *info)
{
	GError *error = NULL;
	GStrv thumb_urls = NULL;

	dbus_g_proxy_end_call (proxy, call_id, 
			       &error,
			       G_TYPE_STRV,
			       &thumb_urls);

	if (error) {
		dbus_g_method_return_error (info->context, error);
		g_clear_error (&error);
	} else
		dbus_g_method_return (info->context, thumb_urls);

	g_strfreev (thumb_urls);
}

static void 
on_create_destroy (ProxyCallInfo *info)
{
	g_object_unref (info->proxy);
	g_slice_free (ProxyCallInfo, info);
}

static void
on_plugin_finished (const GStrv thumb_urls, GError *error, DBusGMethodInvocation *context)
{
	if (error) {
		dbus_g_method_return_error (context, error);
		g_clear_error (&error);
	} else
		dbus_g_method_return (context, thumb_urls);
}


static gchar *
get_mime_type (const gchar *path)
{
	gchar *content_type;

#ifdef HAVE_GIO
	GFileInfo *info;
	GFile *file;
	GError *error = NULL;

	file = g_file_new_for_path (path);
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
				  G_FILE_QUERY_INFO_NONE,
				  NULL, &error);

	if (error) {
		g_warning ("Error guessing mimetype for '%s': %s\n", path, error->message);
		g_error_free (error);

		return g_strdup ("unknown");
	}

	content_type = g_strdup (g_file_info_get_content_type (info));

	g_object_unref (info);
	g_object_unref (file);

	if (!content_type) {
		return g_strdup ("unknown");
	}
#else
	content_type = gnome_vfs_get_mime_type_from_uri (path);
#endif

	return content_type;
}

void
thumbnailer_create (Thumbnailer *object, GStrv urls, DBusGMethodInvocation *context)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);
	GHashTable *hash = g_hash_table_new (g_str_hash, g_str_equal);
	guint i = 0;
	GHashTableIter iter;
	gpointer key, value;

	while (urls[i] != NULL) {
		GList *urls_for_mime;
		gchar *mime_type = get_mime_type (urls[i]);
		urls_for_mime = g_hash_table_lookup (hash, mime_type);
		urls_for_mime = g_list_prepend (urls_for_mime, urls[i]);
		g_hash_table_replace (hash, mime_type, urls_for_mime);
	}

	g_hash_table_iter_init (&iter, hash);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		GList *urlm = value, *copy = urlm;
		GStrv urlss = (GStrv) g_malloc0 (sizeof (gchar *) * (g_list_length (urlm) + 1));
		DBusGProxy *proxy;

		i = 0;

		while (copy) {
			urlss[i] = copy->data;
			i++;
			copy = g_list_next (copy);
		}

		g_list_free (urlm);

		proxy = manager_get_handler (priv->manager, key);

		if (proxy) {
			ProxyCallInfo *info = g_slice_new (ProxyCallInfo);

			info->context = context;
			info->proxy = proxy;

			dbus_g_proxy_begin_call (proxy, "Create",
						 (DBusGProxyCallNotify) on_create_finished, 
						 info,
						 (GDestroyNotify) on_create_destroy,
						 G_TYPE_STRV,
						 urlss);
			
		} else {
			GModule *module = g_hash_table_lookup (priv->plugins, key);
			if (module) {
				hildon_thumbnail_plugin_do_create (module, urlss, 
								   (create_cb) on_plugin_finished, 
								   context);
			} else {
				GError *error = NULL;
				g_set_error (&error,
					     DBUS_ERROR, 0,
					     "No handler");
				dbus_g_method_return_error (context, error);
				g_clear_error (&error);
			}
		}
	}

	g_hash_table_unref (hash);
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
	g_hash_table_unref (priv->plugins);

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
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

	priv->plugins = g_hash_table_new_full (g_str_hash, g_str_equal,
					       (GDestroyNotify) g_free,
					       NULL);
}



void 
thumbnailer_do_stop (void)
{
}


void 
thumbnailer_do_init (DBusGConnection *connection, Manager *manager, Thumbnailer **thumbnailer, GError **error)
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

	dbus_g_connection_register_g_object (connection, 
					     THUMBNAILER_PATH, 
					     object);

	*thumbnailer = THUMBNAILER (object);

}
