#ifndef __MANAGER_H__
#define __MANAGER_H__

typedef struct Manager Manager;
typedef struct ManagerClass ManagerClass;

struct Manager {
	GObject parent;
};

struct ManagerClass {
	GObjectClass parent;
};

void manager_register (Manager *object, gchar *mime_type, DBusGMethodInvocation *context);

GHashTable* manager_get_handlers (Manager *object);

void manager_do_stop (void);
void manager_do_init (DBusGConnection *connection, Manager **manager, GError **error);

#endif
