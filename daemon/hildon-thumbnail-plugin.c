
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

typedef void (*InitFunc) (GError **error);
typedef GStrv (*SupportedFunc) (void);

void
hildon_thumbnail_plugin_do_init (GModule *module, Thumbnailer *thumbnailer, GError **error)
{
	InitFunc func;

	if (g_module_symbol (module, "hildon_thumbnail_plugin_init", (gpointer *) &func)) {
		GStrv supported = NULL;
		SupportedFunc supported_func;

		if (g_module_symbol (module, "hildon_thumbnail_plugin_supported", (gpointer *) &supported_func)) {
			guint i = 0;

			supported = (supported_func) ();

			if (supported) {
				while (supported[i] != NULL) {

					thumbnailer_register_plugin (thumbnailer, 
								     supported[i], 
								     module);
					i++;
				}
			}

			(func) (error);
		}
	}
}

typedef void (*CreateFunc) (GStrv uris, GError **error);

void 
hildon_thumbnail_plugin_do_create (GModule *module, GStrv uris, GError **error)
{
	CreateFunc func;
	if (g_module_symbol (module, "hildon_thumbnail_plugin_create", (gpointer *) &func)) {
		(func) (uris, error);
	}
}

typedef void (*StopFunc) (void);

void
hildon_thumbnail_plugin_do_stop (GModule *module, Thumbnailer *thumbnailer)
{
	StopFunc func;

	if (g_module_symbol (module, "hildon_thumbnail_plugin_stop", (gpointer *) &func)) {
		thumbnailer_unregister_plugin (thumbnailer, module);
		(func) ();
	}
}
