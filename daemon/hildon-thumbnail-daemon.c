/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * This file is part of hildon-thumbnail package
 *
 * Copyright (C) 2005 Nokia Corporation.  All Rights reserved.
 *
 * Contact: Marius Vollmer <marius.vollmer@nokia.com>
 * Author: Philip Van Hoof <philip@codeminded.be>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/sched.h>
#include <sched.h>

#ifdef HAVE_OSSO
#include <osso-mem.h>
#include <osso-log.h>
#endif

#ifndef SCHED_IDLE
#define SCHED_IDLE 5
#endif

#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include <unistd.h>

#include <glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <gio/gio.h>

#include <hildon-thumbnail-plugin.h>

#include "thumbnailer.h"
#include "albumart.h"
#include "thumbnail-manager.h"
#include "albumart-manager.h"
#include "thumb-hal.h"

void keep_alive (void);

static GHashTable *registrations;
static GHashTable *outregistrations;
static gboolean do_shut_down_next_time = TRUE;


#ifndef __NR_ioprio_set

#if defined(__i386__)
#define __NR_ioprio_set		289
#define __NR_ioprio_get		290
#elif defined(__powerpc__) || defined(__powerpc64__)
#define __NR_ioprio_set		273
#define __NR_ioprio_get		274
#elif defined(__x86_64__)
#define __NR_ioprio_set		251
#define __NR_ioprio_get		252
#elif defined(__ia64__)
#define __NR_ioprio_set		1274
#define __NR_ioprio_get		1275
#elif defined(__alpha__)
#define __NR_ioprio_set		442
#define __NR_ioprio_get		443
#elif defined(__s390x__) || defined(__s390__)
#define __NR_ioprio_set		282
#define __NR_ioprio_get		283
#elif defined(__SH4__)
#define __NR_ioprio_set		288
#define __NR_ioprio_get		289
#elif defined(__SH5__)
#define __NR_ioprio_set		316
#define __NR_ioprio_get		317
#elif defined(__sparc__) || defined(__sparc64__)
#define __NR_ioprio_set		196
#define __NR_ioprio_get		218
#elif defined(__arm__)
#define __NR_ioprio_set		314
#define __NR_ioprio_get		315
#else
#error "Unsupported architecture!"
#endif

#endif

enum {
	IOPRIO_CLASS_NONE,
	IOPRIO_CLASS_RT,
	IOPRIO_CLASS_BE,
	IOPRIO_CLASS_IDLE,
};

enum {
	IOPRIO_WHO_PROCESS = 1,
	IOPRIO_WHO_PGRP,
	IOPRIO_WHO_USER,
};

#define IOPRIO_CLASS_SHIFT 13

void initialize_priority (void);


static inline int
ioprio_set (int which, int who, int ioprio_val)
{
	return syscall (__NR_ioprio_set, which, who, ioprio_val);
}

void
initialize_priority (void)
{
#if 0
	struct sched_param sp;
	int ioprio, ioclass;

	ioprio = 7; /* priority is ignored with idle class */
	ioclass = IOPRIO_CLASS_IDLE << IOPRIO_CLASS_SHIFT;

	ioprio_set (IOPRIO_WHO_PROCESS, 0, ioprio | ioclass);

	nice (19);

	/* Maemo on X86 doesn't have new enough kernel headers.
	 */
#ifdef SCHED_IDLE
 	if (sched_getparam (0, &sp) == 0) {
		if (sched_setscheduler (0, SCHED_IDLE, &sp) == -1) {
			/* Didn't work, but we ignore */
		}
	 }
#endif
#endif
}

void
keep_alive (void) 
{
	do_shut_down_next_time = FALSE;
}

static gboolean
shut_down_after_timeout (gpointer user_data)
{
	GMainLoop *main_loop =  user_data;
	gboolean shut = FALSE;

	if (do_shut_down_next_time) {
		g_main_loop_quit (main_loop);
		shut = TRUE;
	} else
		do_shut_down_next_time = TRUE;

	return shut;
}


static GHashTable*
init_plugins (DBusGConnection *connection, Thumbnailer *thumbnailer)
{
	GHashTable *regs;
	GModule *module;
	GError *error = NULL;
	GDir *dir;
	const gchar *plugin;

	regs = g_hash_table_new_full (g_str_hash, g_str_equal,
				      (GDestroyNotify) g_free, 
				      (GDestroyNotify) NULL);

	dir = g_dir_open (PLUGINS_DIR, 0, &error);

	if (dir) {
	  while ((plugin = g_dir_read_name (dir)) != NULL) {
		gboolean cropping;
		gchar *full;

		if (!g_str_has_suffix (plugin, "." G_MODULE_SUFFIX)) {
			continue;
		}

		full = g_build_filename (PLUGINS_DIR, plugin, NULL);

		module = hildon_thumbnail_plugin_load (full);
		if (module)
			hildon_thumbnail_plugin_do_init (module, &cropping,
							 (hildon_thumbnail_register_func) thumbnailer_register_plugin,
							 thumbnailer,
							 &error);
		if (error) {
			g_warning ("Can't load plugin [%s]: %s\n", plugin, 
				   error->message);
			g_error_free (error);
			g_free (full);
			if (module)
				g_module_close (module);
		} else if (module)
			g_hash_table_replace (regs, full, module);
		  else
			g_free (full);
	  }
	  g_dir_close (dir);
	}

	if (error)
		g_error_free (error);

	return regs;
}


static GHashTable*
init_outputplugins (DBusGConnection *connection, Thumbnailer *thumbnailer)
{
	GHashTable *regs;
	GModule *module;
	GError *error = NULL;
	GDir *dir;
	const gchar *plugin;

	regs = g_hash_table_new_full (g_str_hash, g_str_equal,
				      (GDestroyNotify) g_free, 
				      (GDestroyNotify) NULL);

	dir = g_dir_open (OUTPUTPLUGINS_DIR, 0, &error);

	if (dir) {
	  while ((plugin = g_dir_read_name (dir)) != NULL) {
		gchar *full;

		if (!g_str_has_suffix (plugin, "." G_MODULE_SUFFIX)) {
			continue;
		}

		full = g_build_filename (OUTPUTPLUGINS_DIR, plugin, NULL);
		module = hildon_thumbnail_outplugin_load (full);
		g_hash_table_replace (regs, full, module);
	  }
	  g_dir_close (dir);
	}

	if (error)
		g_error_free (error);

	return regs;
}

static void
stop_plugins (GHashTable *regs, Thumbnailer *thumbnailer)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, regs);

	while (g_hash_table_iter_next (&iter, &key, &value))  {
		g_hash_table_iter_remove (&iter);
		thumbnailer_unregister_plugin (thumbnailer, value);
		hildon_thumbnail_plugin_do_stop (value);
	}
}

static void
stop_outputplugins (GHashTable *regs, Thumbnailer *thumbnailer)
{
	GHashTableIter iter;
	gpointer key, value;

	g_hash_table_iter_init (&iter, regs);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		g_hash_table_iter_remove (&iter);
		hildon_thumbnail_outplugin_unload (value);
	}
}

static void
on_plugin_changed (GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data)
{
	Thumbnailer *thumbnailer = user_data;
	gchar *path = g_file_get_path (file);

	if (path && g_str_has_suffix (path, "." G_MODULE_SUFFIX)) {
		switch (event_type)  {
			case G_FILE_MONITOR_EVENT_DELETED: {
				GModule *module = g_hash_table_lookup (registrations, path);
				if (module) {
					g_hash_table_remove (registrations, path);
					thumbnailer_unregister_plugin (thumbnailer, module);
					hildon_thumbnail_plugin_do_stop (module);
				}
			}
			break;
			case G_FILE_MONITOR_EVENT_CREATED: {
				GModule *module = hildon_thumbnail_plugin_load (path);
				gboolean cropping = FALSE;

				if (module) {
					GError *error = NULL;

					hildon_thumbnail_plugin_do_init (module, &cropping,
							 (hildon_thumbnail_register_func) thumbnailer_register_plugin,
							 thumbnailer,
							 &error);
					if (error) {
						g_warning ("Can't load plugin [%s]: %s\n", path, 
							   error->message);
						g_error_free (error);
					} else
						g_hash_table_replace (registrations, g_strdup (path),
								      module);
				}
			}
			break;
			default:
			break;
		}
	}

	g_free (path);
}



static void
on_outputplugin_changed (GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data)
{
	/* Thumbnailer *thumbnailer = user_data; */
	gchar *path = g_file_get_path (file);

	if (path && g_str_has_suffix (path, "." G_MODULE_SUFFIX)) {
		switch (event_type)  {
			case G_FILE_MONITOR_EVENT_DELETED: {
				GModule *module = g_hash_table_lookup (outregistrations, path);
				if (module) {
					g_hash_table_remove (outregistrations, path);
					hildon_thumbnail_outplugin_unload (module);
				}
			}
			break;
			case G_FILE_MONITOR_EVENT_CREATED: {
				GModule *module = hildon_thumbnail_outplugin_load (path);
				if (module) {
					g_hash_table_replace (outregistrations, g_strdup (path),
							      module);
				}
			}
			break;
			default:
			break;
		}
	}

	g_free (path);
}

static void
thumbnailer_oom_func (size_t cur, size_t max, void *data)
{
	exit(1);
}

static void
create_dummy_files (void)
{
	gchar *dir;

	dir = g_build_filename (g_get_home_dir (), ".config", "hildon-thumbnailer", NULL);
	g_mkdir_with_parents (dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	g_free (dir);

	dir = g_build_filename (g_get_home_dir (), ".local", "share", "thumbnailers", NULL);
	g_mkdir_with_parents (dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	g_free (dir);

}

int 
main (int argc, char **argv) 
{
	DBusGConnection *connection;
	GError *error = NULL;
	int result;

#if defined (HAVE_MALLOPT) && defined(M_MMAP_THRESHOLD)
	mallopt (M_MMAP_THRESHOLD, 128 *1024);
#endif

#ifdef HAVE_OSSO
	result = osso_mem_saw_enable(4 << 20, 64, thumbnailer_oom_func, NULL);
#endif

	g_type_init ();

	if (!g_thread_supported ())
		g_thread_init (NULL);

	create_dummy_files ();

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

	if (!connection)
		g_critical ("Could not connect to the DBus session bus, %s",
			    error ? error->message : "no error given.");
	else {
		GMainLoop *main_loop;
		ThumbnailManager *manager;
		AlbumartManager *a_manager;
		Thumbnailer *thumbnailer;
		Albumart *arter;
		DBusGProxy *manager_proxy;
		GFile *file, *fileo;
		GFileMonitor *monitor, *monitoro;

		thumbnail_manager_do_init (connection, &manager, &error);
		thumbnailer_do_init (connection, manager, &thumbnailer, &error);

		albumart_manager_do_init (connection, &a_manager, &error);
		albumart_do_init (connection, a_manager, &arter, &error);

		manager_proxy = dbus_g_proxy_new_for_name (connection, 
					   MANAGER_SERVICE,
					   MANAGER_PATH,
					   MANAGER_INTERFACE);

		registrations = init_plugins (connection, thumbnailer);
		outregistrations = init_outputplugins (connection, thumbnailer);

		file = g_file_new_for_path (PLUGINS_DIR);
		monitor =  g_file_monitor_directory (file, G_FILE_MONITOR_NONE, NULL, NULL);
		g_signal_connect (G_OBJECT (monitor), "changed", 
				  G_CALLBACK (on_plugin_changed), thumbnailer);

		fileo = g_file_new_for_path (OUTPUTPLUGINS_DIR);
		monitoro =  g_file_monitor_directory (fileo, G_FILE_MONITOR_NONE, NULL, NULL);
		g_signal_connect (G_OBJECT (monitoro), "changed", 
				  G_CALLBACK (on_outputplugin_changed), thumbnailer);


		thumb_hal_init ();

		main_loop = g_main_loop_new (NULL, FALSE);

		g_timeout_add_seconds (600, 
				       shut_down_after_timeout,
				       main_loop);

		g_main_loop_run (main_loop);

		thumb_hal_shutdown ();

		g_object_unref (monitor);
		g_object_unref (file);
		g_object_unref (monitoro);
		g_object_unref (fileo);

		stop_plugins (registrations, thumbnailer);
		stop_outputplugins (outregistrations, thumbnailer);

		g_hash_table_unref (registrations);
		g_hash_table_unref (outregistrations);

		albumart_do_stop ();
		thumbnailer_do_stop ();
		thumbnail_manager_do_stop ();
		albumart_manager_do_stop ();

		g_object_unref (thumbnailer);
		g_object_unref (manager);
		g_object_unref (arter);
		g_object_unref (a_manager);

		g_main_loop_unref (main_loop);
	}

#ifdef HAVE_OSSO
	osso_mem_saw_disable();
#endif

	return 0;
}
