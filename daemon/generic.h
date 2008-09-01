#ifndef __GENERIC_H__
#define __GENERIC_H__

typedef struct Generic Generic;
typedef struct GenericClass GenericClass;

struct Generic {
	GObject parent;
};

struct GenericClass {
	GObjectClass parent;
};

void generic_create (Generic *object, GStrv urls, DBusGMethodInvocation *context);
void generic_move (Generic *object, GStrv from_urls, GStrv to_urls, DBusGMethodInvocation *context);
void generic_delete (Generic *object, GStrv urls, DBusGMethodInvocation *context);

void generic_do_stop (void);
void generic_do_init (DBusGConnection *connection, GError **error);

#endif
