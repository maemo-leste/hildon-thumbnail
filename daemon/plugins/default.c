#include <glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <hildon-thumbnail-factory.h>

#include "default.h"
#include "hildon-thumbnail-plugin.h"

static const gchar* supported[2] = { "image/png" , NULL };


const gchar** 
hildon_thumbnail_plugin_supported (void)
{
	return supported;
}

void
hildon_thumbnail_plugin_create (GStrv uris, GError **error)
{
	guint i = 0;
	while (uris[i] != NULL) {
		g_print ("%s\n", uris[i]);
		i++;
	}
	return;
}

void 
hildon_thumbnail_plugin_stop (void)
{
}


void 
hildon_thumbnail_plugin_init (GError **error)
{
}
