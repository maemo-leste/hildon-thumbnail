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

typedef struct {
	GStrv uris;
	create_cb cb;
	gpointer user_data;
} CreateInfo;


static gboolean 
on_finished (CreateInfo *info)
{
	// Returning back the same uris is of course not right :). The first 
	// param should be set with the uris made for the uris in info->uris, 
	// else the second parameter, the error, must be set

	info->cb (info->uris, NULL, info->user_data);

	return FALSE;
}

static void
on_destroy (CreateInfo *info)
{
	g_slice_free (CreateInfo, info);
}

void
hildon_thumbnail_plugin_create (GStrv uris, create_cb callback, gpointer user_data)
{
	CreateInfo *info = g_slice_new (CreateInfo);

	info->uris = uris;
	info->cb = callback;
	info->user_data = user_data;

	// This is of course a dummy implementation

	g_idle_add_full (G_PRIORITY_DEFAULT,
			 (GSourceFunc) on_finished,
			 info,
			 (GDestroyNotify) on_destroy);
}


void 
hildon_thumbnail_plugin_stop (void)
{
}


void 
hildon_thumbnail_plugin_init (GError **error)
{
}
