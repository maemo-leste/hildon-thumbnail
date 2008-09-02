
#include "hildon-thumbnail-plugin.h"


GModule *
hildon_thumbnail_plugin_load (const gchar *module_name)
{
	gchar *full_name, *path;
	GModule *module;

	g_return_val_if_fail (module_name != NULL, NULL);

	full_name = g_strdup_printf ("libhildon-thumbnailer-%s", module_name);
	path = g_build_filename (PLUGINS_DIR, full_name, NULL);

	module = g_module_open (path, G_MODULE_BIND_LOCAL);

	if (!module) {
		g_warning ("Could not load thumbnailer module '%s', %s\n", 
			   module_name, 
			   g_module_error ());
	} else {
		g_module_make_resident (module);
	}

	g_free (full_name);
	g_free (path);

	return module;
}

typedef void (*InitFunc) (DBusGConnection *connection,  DBusGProxy *manager, GError **error);

void
hildon_thumbnail_plugin_do_init (GModule *module, DBusGConnection *connection, DBusGProxy *manager, GError **error)
{
	InitFunc func;

	if (g_module_symbol (module, "hildon_thumbnail_plugin_init", (gpointer *) &func)) {
		(func) (connection, manager, error);
	}
}

typedef void (*StopFunc) (void);

void
hildon_thumbnail_plugin_do_stop (GModule *module)
{
	StopFunc func;

	if (g_module_symbol (module, "hildon_thumbnail_plugin_stop", (gpointer *) &func)) {
		(func) ();
	}
}
