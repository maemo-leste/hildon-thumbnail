/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * This file is part of hildon-albumart package
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

#include "hildon-albumart-factory.h"
#include "albumart-client.h"
#include "utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gfileutils.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>


#define ALBUMARTER_SERVICE      "com.nokia.albumart"
#define ALBUMARTER_PATH         "/com/nokia/albumart/Requester"
#define ALBUMARTER_INTERFACE    "com.nokia.albumart.Requester"


typedef struct {
	gchar *kind, *album, *artist;
	HildonAlbumartFactoryFinishedCallback callback;
	gpointer user_data;
	gboolean canceled;
	guint handle_id;

} ArtsItem;


#ifndef gdk_pixbuf_new_from_stream_at_scale
/* It's implemented in pixbuf-io-loader.c in this case */
GdkPixbuf* gdk_pixbuf_new_from_stream_at_scale (GInputStream *stream, gint width,
			gint height, gboolean preserve_aspect_ratio,
			GCancellable *cancellable, GError **error);
#endif

#ifndef gdk_pixbuf_new_from_stream
/* It's implemented in pixbuf-io-loader.c in this case */
GdkPixbuf * gdk_pixbuf_new_from_stream (GInputStream  *stream,
			    GCancellable  *cancellable,
			    GError       **error);
#endif 

#define ARTS_ITEM(handle) (ArtsItem*)(handle)
#define ARTS_HANDLE(item) (HildonAlbumartFactoryHandle)(item)
#define HILDON_ALBUMART_APPLICATION "hildon-albumart"
#define FACTORY_ERROR g_quark_from_static_string (HILDON_ALBUMART_APPLICATION)

static void init (void);

static gboolean show_debug = FALSE, had_init = FALSE;
static DBusGProxy *proxy;
static DBusGConnection *connection;
static GHashTable *tasks;

typedef struct {
	gchar *file;
	guint64 mtime;
	guint64 size;
} ArtsCacheFile;

static void thumb_item_free(ArtsItem* item)
{
	g_free(item->kind);
	g_free(item->artist);
	g_free(item->album);
	g_free(item);
}

gboolean
hildon_albumart_is_cached (const gchar *artist_or_title, const gchar *album, const gchar *kind)
{
	gchar *path;
	gboolean retval = FALSE;

	hildon_thumbnail_util_get_albumart_path (artist_or_title, album, kind, &path);

	retval = g_file_test (path, G_FILE_TEST_EXISTS);

	g_free (path);

	return retval;
}

gchar * 
hildon_albumart_get_path (const gchar *artist_or_title, const gchar *album, const gchar *kind)
{
	gchar *path;

	hildon_thumbnail_util_get_albumart_path (artist_or_title, album, kind, &path);

	return path;
}

static void
create_pixbuf_and_callback (ArtsItem *item, gchar *path)
{
		GFile *filei = NULL;
		GInputStream *stream = NULL;
		GdkPixbuf *pixbuf = NULL;
		GError *error = NULL;

		/* Open the original albumart as a stream */
		filei = g_file_new_for_path (path);
		stream = G_INPUT_STREAM (g_file_read (filei, NULL, &error));

		if (error)
			goto error_handler;

		/* Read the stream as a pixbuf at the requested exact scale */
		pixbuf = gdk_pixbuf_new_from_stream (stream, NULL, &error);

		error_handler:

		/* Callback user function, passing the pixbuf and error */

		item->callback (item, item->user_data, pixbuf, error);

		/* Cleanup */
		if (filei)
			g_object_unref (filei);

		if (stream) {
			g_input_stream_close (G_INPUT_STREAM (stream), NULL, NULL);
			g_object_unref (stream);
		}

		if (error)
			g_error_free (error);

		if (pixbuf)
			gdk_pixbuf_unref (pixbuf);
}

static void
on_task_finished (DBusGProxy *proxy_,
		  guint       handle,
		  gpointer    user_data)
{
	gchar *key = g_strdup_printf ("%d", handle);
	ArtsItem *item = g_hash_table_lookup (tasks, key);

	if (item) {
		gchar *path;

		/* Get the large small and cropped path for the original
		 * URI */

		hildon_thumbnail_util_get_albumart_path (item->artist, 
							 item->album, 
							 item->kind, &path);

		create_pixbuf_and_callback (item, path);

		g_free (path);

		/* Remove the key from the hash, which means that we declare it 
		 * handled. */
		g_hash_table_remove (tasks, key);
	}

	g_free (key);

}

static void 
read_cache_dir(gchar *path, GPtrArray *files)
{
	GDir *dir;
	const gchar *file;

	dir = g_dir_open(path, 0, NULL);

	if(dir) {
		while((file = g_dir_read_name(dir)) != NULL) {
			gchar *file_path;
			ArtsCacheFile *item;
			GFile *filei;
			GFileInfo *info;
			GError *error = NULL;
			guint64 mtime, size;

			file_path = g_build_filename (path, file, NULL);

			if(file[0] == '.' || !g_file_test(file_path, G_FILE_TEST_IS_REGULAR)) {
				g_free (file_path);
				continue;
			}

			filei = g_file_new_for_path (file_path);

			info = g_file_query_info (filei, G_FILE_ATTRIBUTE_TIME_MODIFIED ","
						  G_FILE_ATTRIBUTE_STANDARD_SIZE,
						  G_FILE_QUERY_INFO_NONE,
						  NULL, &error);

			if (error) {
				g_error_free (error);
				g_object_unref (filei);
				g_free (file_path);
				if (info)
					g_object_unref (info);
				continue;
			}

			mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
			size = g_file_info_get_size (info);

			g_object_unref (filei);
			g_object_unref (info);

			item = g_new(ArtsCacheFile, 1);
			item->file = file_path;
			item->mtime = mtime;
			item->size = size;

			g_ptr_array_add(files, item);
		}

		g_dir_close(dir);
	}
}

static void
cache_file_free(ArtsCacheFile *item)
{
	g_free (item->file);
	g_free (item);
}

static gint 
cache_file_compare(gconstpointer a, gconstpointer b)
{
	ArtsCacheFile *f1 = *(ArtsCacheFile**)a,
			        *f2 = *(ArtsCacheFile**)b;

	/* Sort in descending order */
	if(f2->mtime == f1->mtime) {
		return 0;
	} else if(f2->mtime < f1->mtime) {
		return -1;
	} else {
		return 1;
	}
}



void 
hildon_albumart_factory_clean_cache(gint max_size, time_t min_mtime)
{
	GPtrArray *files;
	int i, size = 0;
	gchar *a_dir =  g_build_filename (g_get_home_dir (), ".album_art", NULL);

	init ();

	files = g_ptr_array_new();

	read_cache_dir (a_dir, files);

	g_ptr_array_sort (files, cache_file_compare);

	for(i = 0; i < files->len; i++) {
		ArtsCacheFile *item = g_ptr_array_index (files, i);

		size += item->size;
		if ((max_size >= 0 && size >= max_size) || item->mtime < min_mtime) {
			unlink (item->file);
		}
	}

	g_ptr_array_foreach (files, (GFunc)cache_file_free, NULL);
	g_ptr_array_free (files, TRUE);

	g_free (a_dir);
}

static gboolean waiting_for_cb = FALSE;

static void 
on_got_handle (DBusGProxy *proxy_, guint OUT_handle, GError *error, gpointer userdata)
{
	ArtsItem *item = userdata;
	gchar *key = g_strdup_printf ("%d", OUT_handle);
	item->handle_id = OUT_handle;

	/* Register the item as being handled */
	g_hash_table_replace (tasks, key, item);

	waiting_for_cb = FALSE;
}

typedef struct {
	gchar *path;
	ArtsItem *item;
} ArtsItemAndPaths;

static void
free_thumbsitem_and_paths (ArtsItemAndPaths *info) 
{
	g_free (info->path);
	thumb_item_free (info->item);
	g_slice_free (ArtsItemAndPaths, info);
}

static gboolean
have_all_cb (gpointer user_data)
{
	ArtsItemAndPaths *info = user_data;
	ArtsItem *item = info->item;

	create_pixbuf_and_callback (item, info->path);

	return FALSE;
}

HildonAlbumartFactoryHandle hildon_albumart_factory_load(
				const gchar *artist_or_title,
				const gchar *album,
				const gchar *kind,
				HildonAlbumartFactoryFinishedCallback callback,
				gpointer user_data)
{
	const gchar *artist = artist_or_title;
	gchar *path;
	ArtsItem *item;
	gboolean have_all = FALSE;

	g_debug ("Albumart request for %s,%s,%s\n",
		 artist_or_title, album, kind);

	hildon_thumbnail_util_get_albumart_path (artist, album, kind, &path);

	have_all = g_file_test (path, G_FILE_TEST_EXISTS);

	item = g_new0 (ArtsItem, 1);

	if (kind)
		item->kind = g_strdup(kind);
	if (album)
		item->album = g_strdup(album);
	if (artist)
		item->artist = g_strdup(artist);
	item->callback = callback;
	item->user_data = user_data;
	item->canceled = FALSE;
	item->handle_id = 0;

	if (have_all) {
		ArtsItemAndPaths *info = g_slice_new (ArtsItemAndPaths);

		info->item = item;
		info->path = path;

		g_idle_add_full (G_PRIORITY_DEFAULT, have_all_cb, info,
						 (GDestroyNotify) free_thumbsitem_and_paths);

		return item;
	}

	g_free (path);

	if (!have_all) {

		init ();

		waiting_for_cb = TRUE;
		com_nokia_albumart_Requester_queue_async (proxy, artist, 
							  album, 
							  kind, 0, 
							  on_got_handle,
							  item);

	}

	return ARTS_HANDLE (item);
}


static void 
on_cancelled (DBusGProxy *proxy_, GError *error, gpointer userdata)
{
	ArtsItem *item = userdata;
	gchar *key = g_strdup_printf ("%d", item->handle_id);

	/* Unregister the item */
	g_hash_table_remove (tasks, key);
	g_free (key);
}

void hildon_albumart_factory_cancel(HildonAlbumartFactoryHandle handle)
{
	ArtsItem *item = ARTS_ITEM (handle);

	init();

	if (item->handle_id == 0)
		return;

	/* We don't do real canceling, we just do unqueing */

	com_nokia_albumart_Requester_unqueue_async (proxy, 
						    item->handle_id,
						    on_cancelled, item);

}

void hildon_albumart_factory_wait()
{
	init();

	while (waiting_for_cb)
		g_main_context_iteration (NULL, FALSE);

	while(g_hash_table_size (tasks) != 0) {
		g_main_context_iteration(NULL, TRUE);
	}
}

static void file_opp_reply  (DBusGProxy *proxy_, GError *error, gpointer userdata)
{
}

void hildon_albumart_factory_remove(const gchar *artist_or_title, const gchar *album, const gchar *kind)
{
	const gchar *artist = artist_or_title;

	init();

	com_nokia_albumart_Requester_delete_async (proxy, 
						   artist, 
						   album, 
						   kind, 
						   file_opp_reply, NULL);

}

void hildon_albumart_factory_set_debug(gboolean debug);

void hildon_albumart_factory_set_debug(gboolean debug)
{
	init ();

	show_debug = debug;
}



static void init (void) {
	if (!had_init) {
		GError *error = NULL;

		tasks = g_hash_table_new_full (g_str_hash, g_str_equal,
					       (GDestroyNotify) g_free,
					       (GDestroyNotify) thumb_item_free);

		connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

		if (error) {
			g_critical ("Can't initialize HildonAlbumarter: %s\n", error->message);
			g_error_free (error);
			g_hash_table_unref (tasks);
			tasks = NULL;
			connection = NULL;
			had_init = FALSE;
			abort ();
			return;
		}

		proxy = dbus_g_proxy_new_for_name (connection, 
					   ALBUMARTER_SERVICE,
					   ALBUMARTER_PATH,
					   ALBUMARTER_INTERFACE);

		dbus_g_proxy_add_signal (proxy, "Finished", 
					G_TYPE_UINT, G_TYPE_INVALID);

		dbus_g_proxy_connect_signal (proxy, "Finished",
				     G_CALLBACK (on_task_finished),
				     NULL,
				     NULL);
	}
	had_init = TRUE;
}

