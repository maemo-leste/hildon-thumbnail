#ifndef __MANAGER_H__
#define __MANAGER_H__

#define TYPE_MANAGER             (manager_get_type())
#define MANAGER(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_MANAGER, Manager))
#define MANAGER_CLASS(c)         (G_TYPE_CHECK_CLASS_CAST ((c), TYPE_MANAGER, ManagerClass))
#define MANAGER_GET_CLASS(o)     (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_MANAGER, ManagerClass))

typedef struct Manager Manager;
typedef struct ManagerClass ManagerClass;

struct Manager {
	GObject parent;
};

struct ManagerClass {
	GObjectClass parent;
};

GType manager_get_type (void);

void manager_register (Manager *object, gchar *mime_type, DBusGMethodInvocation *context);
GHashTable* manager_get_handlers (Manager *object);

void manager_do_stop (void);
void manager_do_init (DBusGConnection *connection, Manager **manager, GError **error);

#endif
