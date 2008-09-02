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
	Manager *manager;
	GHashTable *plugins;
	GThreadPool *pool;
} ThumbnailerPrivate;

enum {
	PROP_0,
	PROP_MANAGER
};


void 
thumbnailer_register_plugin (Thumbnailer *object, const gchar *mime_type, GModule *plugin)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);

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

static void
cleanup_hash (gpointer key, gpointer value, gpointer user_data)
{
	if (value)
		g_list_free (value);
}

typedef struct {
	Thumbnailer *object;
	GStrv urls;
	DBusGMethodInvocation *context;
	guint num;
} WorkTask;


static gint 
pool_sort_compare (gconstpointer a, gconstpointer b, gpointer user_data)
{
	WorkTask *task_a = (WorkTask *) a;
	WorkTask *task_b = (WorkTask *) b;

	/* This makes pool a LIFO */

	return task_b->num - task_a->num;
}

void
thumbnailer_create (Thumbnailer *object, GStrv urls, DBusGMethodInvocation *context)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (object);
	WorkTask *task = g_slice_new (WorkTask);
	guint urls_size = g_strv_length (urls), i = 0;
	static guint num = 0;

	task->num = num++;
	task->object = g_object_ref (object);
	task->urls = (GStrv) g_malloc0 (sizeof (gchar *) * (urls_size + 1));

	while (urls[i] != NULL) {
		task->urls[i] = g_strdup (urls[i]);
		i++;
	}

	task->urls[i] = NULL;
	task->context = context;

	g_thread_pool_push (priv->pool, task, NULL);
}

/* This is the threadpool's function. This means that everything we do is 
 * asynchronous wrt to the mainloop (we aren't blocking it).
 * 
 * Thanks to the pool_sort_compare sorter is this pool a LIFO, which means that
 * new requests get a certain priority over older requests. Note that we are not
 * canceling currently running requests. Also note that the thread count of the 
 * pool is set to one. We could increase this number to add some parallelism */

static void 
do_the_work (WorkTask *task, gpointer user_data)
{
	ThumbnailerPrivate *priv = THUMBNAILER_GET_PRIVATE (task->object);
	GHashTable *hash = g_hash_table_new (g_str_hash, g_str_equal);
	GStrv urls = task->urls;
	DBusGMethodInvocation *context = task->context;
	guint i = 0;
	GHashTableIter iter;
	gpointer key, value;
	gboolean had_error = FALSE;

	/* We split the request into groups that all have the same mime-type */

	while (urls[i] != NULL) {
		GList *urls_for_mime;
		gchar *mime_type = get_mime_type (urls[i]);
		urls_for_mime = g_hash_table_lookup (hash, mime_type);
		urls_for_mime = g_list_prepend (urls_for_mime, urls[i]);
		g_hash_table_replace (hash, mime_type, urls_for_mime);
		i++;
	}

	g_hash_table_iter_init (&iter, hash);

	/* Foreach of those groups */

	while (g_hash_table_iter_next (&iter, &key, &value) && !had_error) {

		GList *urlm = value, *copy = urlm;
		GStrv urlss = (GStrv) g_malloc0 (sizeof (gchar *) * (g_list_length (urlm) + 1));
		DBusGProxy *proxy;

		i = 0;

		while (copy) {
			urlss [i] = g_strdup ((gchar *) copy->data);
			i++;
			copy = g_list_next (copy);
		}

		g_list_free (urlm);
		g_hash_table_iter_remove (&iter);

		/* If we have a third party thumbnailer for this mime-type, we
		 * proxy the call */

		proxy = manager_get_handler (priv->manager, key);
		if (proxy) {
			GError *error = NULL;
			dbus_g_proxy_call (proxy, "Create", &error, 
					   G_TYPE_STRV, urlss,
					   G_TYPE_INVALID, 
					   G_TYPE_INVALID);

			g_object_unref (proxy);

			if (error) {
				had_error = TRUE;
				dbus_g_method_return_error (context, error);
				g_clear_error (&error);
				g_strfreev (urlss);
				break;
			}

		/* If not if we have a plugin that can handle it, we let the 
		 * plugin have a go at it */

		} else {
			GModule *module = g_hash_table_lookup (priv->plugins, key);
			if (module) {
				GError *error = NULL;

				hildon_thumbnail_plugin_do_create (module, urlss, &error);

				if (error) {
					had_error = TRUE;
					dbus_g_method_return_error (context, error);
					g_clear_error (&error);
					g_strfreev (urlss);
					break;
				}

			/* And if even that is not the case, we are very sorry */

			} else
				g_message ("No handler for %s", (gchar*) key);
		}

		g_strfreev (urlss);
	}

	if (!had_error)
		dbus_g_method_return (context);
	else
		g_hash_table_foreach (hash, cleanup_hash, NULL);

	g_hash_table_unref (hash);

	/* task->context will always be returned by now */

	g_object_unref (task->object);
	g_strfreev (task->urls);
	g_slice_free (WorkTask, task);

	return;
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

	g_thread_pool_free (priv->pool, TRUE, TRUE);

	g_object_unref (priv->manager);
	g_hash_table_unref (priv->plugins);


	G_OBJECT_CLASS (thumbnailer_parent_class)->finalize (object);
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
thumbnailer_set_property (GObject      *object,
		      guint         prop_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	switch (prop_id) {
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

	/* We could increase the amount of threads to add some parallelism */

	priv->pool = g_thread_pool_new ((GFunc) do_the_work,NULL,1,TRUE,NULL);

	/* This sort function makes the pool a LIFO */

	g_thread_pool_set_sort_function (priv->pool, pool_sort_compare, NULL);
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
			       "manager", manager,
			       NULL);

	dbus_g_object_type_install_info (G_OBJECT_TYPE (object), 
					 &dbus_glib_thumbnailer_object_info);

	dbus_g_connection_register_g_object (connection, 
					     THUMBNAILER_PATH, 
					     object);

	*thumbnailer = THUMBNAILER (object);
}
