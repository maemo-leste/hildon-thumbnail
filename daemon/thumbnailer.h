#ifndef __THUMBNAILER_H__
#define __THUMBNAILER_H__

#include "manager.h"

typedef struct Thumbnailer Thumbnailer;
typedef struct ThumbnailerClass ThumbnailerClass;

struct Thumbnailer {
	GObject parent;
};

struct ThumbnailerClass {
	GObjectClass parent;
};

void thumbnailer_create (Thumbnailer *object, GStrv urls, DBusGMethodInvocation *context);
void thumbnailer_move (Thumbnailer *object, GStrv from_urls, GStrv to_urls, DBusGMethodInvocation *context);
void thumbnailer_delete (Thumbnailer *object, GStrv urls, DBusGMethodInvocation *context);

void thumbnailer_do_stop (void);
void thumbnailer_do_init (DBusGConnection *connection, Manager *manager, GError **error);

#endif
