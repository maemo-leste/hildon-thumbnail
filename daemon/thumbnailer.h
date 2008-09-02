#ifndef __THUMBNAILER_H__
#define __THUMBNAILER_H__

#include "manager.h"

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
};

GType thumbnailer_get_type (void);

void thumbnailer_create (Thumbnailer *object, GStrv urls, DBusGMethodInvocation *context);
void thumbnailer_move (Thumbnailer *object, GStrv from_urls, GStrv to_urls, DBusGMethodInvocation *context);
void thumbnailer_delete (Thumbnailer *object, GStrv urls, DBusGMethodInvocation *context);

void thumbnailer_do_stop (void);
void thumbnailer_do_init (DBusGConnection *connection, Manager *manager, GError **error);

#endif
