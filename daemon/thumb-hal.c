#include <stdlib.h>

#include "thumb-hal.h"
#include "config.h"

static GVolumeMonitor *monitor;

static void
on_pre_unmount (GVolumeMonitor *volume_monitor,
                GMount         *mount,
                gpointer        user_data) 
{
	GDrive *drive = g_mount_get_drive (mount);
	if (g_drive_is_media_removable (drive)) {
		g_object_unref (drive);
		exit (0);
	}
	g_object_unref (drive);
}

void
thumb_hal_init (void)
{
	monitor = g_volume_monitor_get ();

	g_signal_connect (G_OBJECT (monitor), "mount-pre-unmount", 
					  G_CALLBACK (on_pre_unmount), NULL);
}

void
thumb_hal_shutdown (void)
{
	g_object_unref (monitor);
}
