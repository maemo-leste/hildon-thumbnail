#ifndef __UTILS_H__
#define __UTILS_H__

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>

void hildon_thumbnail_util_get_thumb_paths (const gchar *uri, gchar **large, gchar **normal, GError **error);

#ifndef g_sprintf
gint g_sprintf (gchar *string, gchar const *format, ...);
#endif


#ifndef gdk_pixbuf_new_from_stream_at_scale

GdkPixbuf* gdk_pixbuf_new_from_stream_at_scale (GInputStream *stream, gint width,
			gint height, gboolean preserve_aspect_ratio,
			GCancellable *cancellable, GError **error);
#endif

#endif
