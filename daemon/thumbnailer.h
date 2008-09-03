#ifndef __THUMBNAILER_H__
#define __THUMBNAILER_H__

#include <gmodule.h>

#include "manager.h"

#define THUMBNAILER_SERVICE      "org.freedesktop.thumbnailer"
#define THUMBNAILER_PATH         "/org/freedesktop/thumbnailer"
#define THUMBNAILER_INTERFACE    "org.freedesktop.thumbnailer"

#define TYPE_THUMBNAILER             (thumbnailer_get_type())
#define THUMBNAILER(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_THUMBNAILER, Thumbnailer))
#define THUMBNAILER_CLASS(c)         (G_TYPE_CHECK_CLASS_CAST ((c), TYPE_THUMBNAILER, ThumbnailerClass))
#define THUMBNAILER_GET_CLASS(o)     (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_THUMBNAILER, ThumbnailerClass))

typedef struct Thumbnailer Thumbnailer;
typedef struct ThumbnailerClass ThumbnailerClass;

struct Thumbnailer {
	GObject parent;
};

struct ThumbnailerClass {
	GObjectClass parent;

	void (*started) (Thumbnailer *object, guint handle);
	void (*ready) (Thumbnailer *object, guint handle);
	void (*error) (Thumbnailer *object, guint handle, gchar *reason);
};

GType thumbnailer_get_type (void);

void thumbnailer_queue (Thumbnailer *object, GStrv urls, DBusGMethodInvocation *context);
void thumbnailer_unqueue (Thumbnailer *object, guint handle, DBusGMethodInvocation *context);
void thumbnailer_move (Thumbnailer *object, GStrv from_urls, GStrv to_urls, DBusGMethodInvocation *context);
void thumbnailer_copy (Thumbnailer *object, GStrv from_urls, GStrv to_urls, DBusGMethodInvocation *context);
void thumbnailer_delete (Thumbnailer *object, GStrv urls, DBusGMethodInvocation *context);

void thumbnailer_register_plugin (Thumbnailer *object, const gchar *mime_type, GModule *plugin);
void thumbnailer_unregister_plugin (Thumbnailer *object, GModule *plugin);

void thumbnailer_do_stop (void);
void thumbnailer_do_init (DBusGConnection *connection, Manager *manager, Thumbnailer **thumbnailer, GError **error);

#endif
