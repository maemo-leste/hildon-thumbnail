#ifndef __DBUS_UTILS_H__
#define __DBUS_UTILS_H__

#define DBUS_ERROR_DOMAIN	"HildonThumbnailer"
#define DBUS_ERROR		g_quark_from_static_string (DBUS_ERROR_DOMAIN)

#define dbus_async_return_if_fail(expr,context)				\
	G_STMT_START {							\
		if G_LIKELY(expr) { } else {				\
			GError *error = NULL;				\
									\
			g_set_error (&error,				\
				     DBUS_ERROR,			\
				     0,					\
				     "Assertion `%s' failed",		\
				     #expr);				\
									\
			dbus_g_method_return_error (context, error);	\
			g_clear_error (&error);				\
									\
			return;						\
		};							\
	} G_STMT_END

#endif
