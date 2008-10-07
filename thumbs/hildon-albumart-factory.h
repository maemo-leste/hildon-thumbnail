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

G_BEGIN_DECLS


typedef gpointer HildonAlbumartFactoryHandle;

/**
 * HildonAlbumartFactoryFinishedCallback:
 * @handle: Handle of the albumart that was completed
 * @user_data: User-supplied data when callback was registered
 * @albumart: A pixbuf containing the albumart or %NULL. If application wishes to keep
 *      the structure, it must call g_object_ref() on it. The library does not cache
 *      returned pixbufs.
 *      The pixbuf may contain various options, which are prefixed with
 *      HILDON_ALBUMART_OPTION_PREFIX. The various options may be "Noimage" if there
 *      is no image but only metadata, "Title", "Artist" etc. To get the options,
 *      use gdk_pixbuf_get_option(albumart, HILDON_ALBUMART_OPTION_PREFIX "Option").
 * @error: The error or %NULL if there was none. Freed after callback returns.
 *
 * Called when the albumarting process finishes or there is an error
 */
typedef void (*HildonAlbumartFactoryFinishedCallback)(HildonAlbumartFactoryHandle handle,
    gpointer user_data, GdkPixbuf *albumart, GError *error);

/**
 * hildon_albumart_factory_load:
 * @uri: Albumart will be created for this URI
 * @mime_type: MIME type of the resource the URI points to
 * @width: Desired albumart width
 * @height: Desired albumart height
 * @callback: Function to call when albumart creation finished or there was an error
 * @user_data: Optional data passed to the callback
 *
 * This function requests for the library to create a albumart, or load if from cache
 * if possible. The process is asynchronous, the function returns immediately. Right now
 * most processing is done in the idle callback and albumarting in a separate process.
 * If the process fails, the callback is called with the error.
 *
 * Returns: A #HildonAlbumartFactoryHandle if request succeeded or %NULL if there was
 *  a critical error
 */
HildonAlbumartFactoryHandle hildon_albumart_factory_load(
            const gchar *artist, const gchar *album, const gchar *uri,
            HildonAlbumartFactoryFinishedCallback callback,
            gpointer user_data);



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
void hildon_albumart_factory_remove(const gchar *artist, const gchar *album, const gchar *uri);

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
