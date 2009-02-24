#include <stdlib.h>

#include "thumb-hal.h"
#include "config.h"

#ifdef HAVE_HAL

#include <libhal.h>
#include <libhal-storage.h>

static LibHalContext *context;

#define PROP_IS_MOUNTED        "volume.is_mounted"


static void
hal_device_removed_cb (LibHalContext *context, const gchar   *udi)
{
	/* We instantly exit the process whenever an unmount occurs. We are an 
	 * activatable service so this is ok. The reason why we do this is to
	 * make sure that we release all file descriptors, so that the unmount
	 * can cleanly succeed at all times (if we were building a thumbnail for
	 * a file on the device being unmounted, then that's bad luck for that
	 * thumbnail). */

	exit (0);
}

static void
hal_device_property_modified_cb (LibHalContext *context,
				 const char    *udi,
				 const char    *key,
				 dbus_bool_t	is_removed,
				 dbus_bool_t	is_added)
{
	gboolean is_mounted;

	if (strcmp (key, PROP_IS_MOUNTED) != 0) {
			return;
	}

	is_mounted = libhal_device_get_property_bool (context,
							      udi,
							      key,
							      NULL);

	if (is_mounted)
		hal_device_removed_cb (context, udi); 
}


#endif

void
thumb_hal_init (void)
{
#ifdef HAVE_HAL
	DBusError	error;
	DBusConnection *connection;

	dbus_error_init (&error);

	connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (dbus_error_is_set (&error)) {
		g_critical ("Could not get the system DBus connection, %s",
			    error.message);
		dbus_error_free (&error);
		return;
	}

	dbus_connection_setup_with_g_main (connection, NULL);

	context = libhal_ctx_new ();

	if (!context) {
		g_critical ("Could not create HAL context");
		return;
	}

	libhal_ctx_set_user_data (context, NULL);
	libhal_ctx_set_dbus_connection (context, connection);

	if (!libhal_ctx_init (context, &error)) {
		if (dbus_error_is_set (&error)) {
			g_critical ("Could not initialize the HAL context, %s",
				    error.message);
			dbus_error_free (&error);
		} else {
			g_critical ("Could not initialize the HAL context, "
				    "no error, is hald running?");
		}

		libhal_ctx_free (context);
		return;
	}


	libhal_ctx_set_device_removed (context, hal_device_removed_cb);
	libhal_ctx_set_device_property_modified (context, hal_device_property_modified_cb);
#endif
}

void
thumb_hal_shutdown (void)
{
#ifdef HAVE_HAL
	libhal_ctx_set_user_data (context, NULL);
	libhal_ctx_free (context);
#endif
}
