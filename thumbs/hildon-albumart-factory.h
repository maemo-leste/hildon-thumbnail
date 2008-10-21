/*
 * This file is part of hildon-albumart package
 *
 * Copyright (C) 2005 Nokia Corporation.  All Rights reserved.
 *
 * Contact: Marius Vollmer <marius.vollmer@nokia.com>
 * Author: Philip Van Hoof <pvanhoof@gnome.org>
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

#ifndef __LIBHILDONALBUMARTFACTORY_H__
#define __LIBHILDONALBUMARTFACTORY_H__

#include <unistd.h>
#include <sys/types.h>

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>


#define HILDON_TYPE_ALBUMART_FACTORY             (hildon_albumart_factory_get_type())
#define HILDON_ALBUMART_FACTORY(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), HILDON_TYPE_ALBUMART_FACTORY, HildonAlbumartFactory))
#define HILDON_ALBUMART_FACTORY_CLASS(c)         (G_TYPE_CHECK_CLASS_CAST ((c), HILDON_TYPE_ALBUMART_FACTORY, HildonAlbumartFactoryClass))
#define HILDON_ALBUMART_FACTORY_GET_CLASS(o)     (G_TYPE_INSTANCE_GET_CLASS ((o), HILDON_TYPE_ALBUMART_FACTORY, HildonAlbumartFactoryClass))

#define HILDON_TYPE_ALBUMART_REQUEST             (hildon_albumart_request_get_type())
#define HILDON_ALBUMART_REQUEST(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), HILDON_TYPE_ALBUMART_REQUEST, HildonAlbumartRequest))
#define HILDON_ALBUMART_REQUEST_CLASS(c)         (G_TYPE_CHECK_CLASS_CAST ((c), HILDON_TYPE_ALBUMART_REQUEST, HildonAlbumartRequestClass))
#define HILDON_ALBUMART_REQUEST_GET_CLASS(o)     (G_TYPE_INSTANCE_GET_CLASS ((o), HILDON_TYPE_ALBUMART_REQUEST, HildonAlbumartRequestClass))

G_BEGIN_DECLS

typedef struct _HildonAlbumartFactory HildonAlbumartFactory;
typedef struct _HildonAlbumartRequest HildonAlbumartRequest;

typedef struct _HildonAlbumartFactoryClass HildonAlbumartFactoryClass;
typedef struct _HildonAlbumartRequestClass HildonAlbumartRequestClass;

struct _HildonAlbumartFactory {
	GObject *parent;
};

struct _HildonAlbumartFactoryClass {
	GObjectClass *parent_class;
};

struct _HildonAlbumartRequest {
	GObject *parent;
};

struct _HildonAlbumartRequestClass {
	GObjectClass *parent_class;
};

GType hildon_albumart_factory_get_type (void);
GType hildon_albumart_request_get_type (void);

typedef void (*HildonAlbumartRequestCallback)	(HildonAlbumartFactory *self,
		GdkPixbuf *albumart, GError *error, gpointer user_data);

HildonAlbumartRequest*
	 hildon_albumart_factory_queue (HildonAlbumartFactory *self,
									 const gchar *artist_or_title, const gchar *album, const gchar *kind,
									 HildonAlbumartRequestCallback callback,
									 gpointer user_data,
									 GDestroyNotify destroy);

HildonAlbumartRequest*
	 hildon_albumart_factory_queue_thumbnail (HildonAlbumartFactory *self,
									 const gchar *artist_or_title, const gchar *album, const gchar *kind,
									 guint width, guint height,
									 HildonAlbumartRequestCallback callback,
									 gpointer user_data,
									 GDestroyNotify destroy);

void hildon_albumart_factory_join (HildonAlbumartFactory *self);

void hildon_albumart_request_unqueue (HildonAlbumartRequest *self);

void hildon_albumart_request_join (HildonAlbumartRequest *self);

typedef gpointer HildonAlbumartFactoryHandle;

/**
 * HildonAlbumartFactoryFinishedCallback:
 * @handle: Handle of the albumart that was completed
 * @user_data: (null-ok): User-supplied data when callback was registered
 * @albumart: (null-ok): A pixbuf containing the albumart or %NULL. If application wishes to keep
 *      the structure, it must call g_object_ref() on it. The library does not cache
 *      returned pixbufs.
 * @error: (null-ok): The error or %NULL if there was none. Freed after callback returns.
 *
 * Called when the albumart process finishes or there is an error
 */
typedef void (*HildonAlbumartFactoryFinishedCallback)(HildonAlbumartFactoryHandle handle,
    gpointer user_data, GdkPixbuf *albumart, GError *error);

/**
 * hildon_albumart_factory_load:
 * @artist_or_title: (null-ok): Artist or media title
 * @album: (null-ok): Album of the media or NULL if not applicable 
 * @kind: "album", "podcast" or "radio" (depending on what the albumart downloaders support)
 * @callback: (null-ok): Function to call when albumart creation finished or there was an error
 * @user_data: (null-ok): Optional data passed to the callback
 *
 * This function requests for the library to create albumart, or load if from cache
 * if possible. The process is asynchronous, the function returns immediately.
 * If the process fails, the callback is called with the error.
 *
 * Returns: A #HildonAlbumartFactoryHandle if request succeeded or %NULL if there was
 *  a critical error
 */
HildonAlbumartFactoryHandle hildon_albumart_factory_load(
            const gchar *artist_or_title, const gchar *album, const gchar *kind,
            HildonAlbumartFactoryFinishedCallback callback,
            gpointer user_data);



/**
 * hildon_albumart_is_cached:
 * @artist_or_title: (null-ok): Artist or media title
 * @album: (null-ok): Album of the media or NULL if not applicable 
 * @kind: "album", "podcast" or "radio" (depending on what the albumart downloaders support)
 *
 * Determines whether or not the album art is already cached.
 *
 * Returns: Whether or not the album art is cached already
 **/

gboolean hildon_albumart_is_cached (const gchar *artist_or_title, const gchar *album, const gchar *kind);

/**
 * hildon_albumart_get_path:
 * @artist_or_title: (null-ok): Artist or media title
 * @album: (null-ok): Album of the media or NULL if not applicable 
 * @kind: "album", "podcast" or "radio" (depending on what the albumart downloaders support)
 *
 * Gives you the absolute predicted path to the art for this media. This function
 * doesn't care about the media having been downloaded (cached) already or not,
 * it just returns the predicted path.
 *
 * Returns: (caller-owns): the path to the media. You must free this.
 **/
gchar * hildon_albumart_get_path (const gchar *artist_or_title, const gchar *album, const gchar *kind);


/**
 * hildon_albumart_factory_cancel:
 * @handle: Handle to cancel
 *
 * Removes specified albumart request from the queue
 */
void hildon_albumart_factory_cancel(HildonAlbumartFactoryHandle handle);

/**
 * hildon_albumart_factory_remove:
 * @uri: URI of the file that was deleted
 *
 * Call to indicate the a file was removed and the albumart cache should be updated
 */
void hildon_albumart_factory_remove(const gchar *artist_or_title, const gchar *album, const gchar *kind);

/**
 * hildon_albumart_factory_wait:
 *
 * Wait until all albumarting processes have finished
 */
void hildon_albumart_factory_wait();

/**
 * hildon_albumart_factory_clean_cache:
 * @max_size: Maximum size of cache in bytes. Set to -1 to disable. 0 deletes all entries.
 * @min_mtime: Minimum creation time of albumarts. (usually now() - 30 days)
 *      Set to 0 to disable.
 *
 * Clean the albumart cache, deletes oldest entries first (based on albumart
 * creation date)
 */
void hildon_albumart_factory_clean_cache(gint max_size, time_t min_mtime);


G_END_DECLS

#endif
