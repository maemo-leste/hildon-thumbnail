#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "thumb-hal.h"
#include "config.h"

#define CHECK_FILE "/tmp/thumbnailer_please_wait"

static GVolumeMonitor *monitor;
static Thumbnailer *thumbnailer;

static void
on_pre_unmount (GVolumeMonitor *volume_monitor,
                GMount         *mount,
                gpointer        user_data) 
{
	GDrive *drive = g_mount_get_drive (mount);
	if (g_drive_is_media_removable (drive)) {
		FILE *filep;

		thumbnailer_crash_out (thumbnailer);

		g_object_unref (drive);

		/* The idea here is that if we had to force-shutdown because of an
		 * unmount event, that it's possible that we get soon-after more
		 * items to process that are also on the removable device. So we 
		 * leave a message for the next startup, to wait for 10 seconds
		 * before we start for real. */

		filep = fopen (CHECK_FILE, "w");
		if (filep)
			fclose (filep);

		exit (0);
	}
	g_object_unref (drive);
}

void
thumb_hal_init (Thumbnailer *thumbnailer_)
{
	FILE *filep;

	filep = fopen (CHECK_FILE, "r");
	if (filep) {
		/* See above */
		sleep (15);
		fclose (filep);
		g_unlink (CHECK_FILE);
	}

	monitor = g_volume_monitor_get ();

	thumbnailer = g_object_ref (thumbnailer_);

	g_signal_connect (G_OBJECT (monitor), "mount-pre-unmount", 
					  G_CALLBACK (on_pre_unmount), NULL);
}

void
thumb_hal_shutdown (void)
{
	g_object_unref (monitor);
	g_object_unref (thumbnailer);
}
