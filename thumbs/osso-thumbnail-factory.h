/*
 * This file is part of osso-thumbnail package
 *
 * Copyright (C) 2005 Nokia Corporation.  All Rights reserved.
 *
 * Contact: Marius Vollmer <marius.vollmer@nokia.com>
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

#ifndef __LIBOSSOTHUMBNAILFACTORY_H__
#define __LIBOSSOTHUMBNAILFACTORY_H__

#include <unistd.h>
#include <sys/types.h>

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

G_BEGIN_DECLS

typedef gpointer OssoThumbnailFactoryHandle;

/**
 * OssoThumbnailFlags:
 *
 * Flags to use for thumbnail creation
 */
typedef enum {
    OSSO_THUMBNAIL_FLAG_CROP = 1 << 1,
    OSSO_THUMBNAIL_FLAG_RECREATE = 1 << 2
} OssoThumbnailFlags;

/**
 * OssoThumbnailFactoryFinishedCallback:
 * @handle: Handle of the thumbnail that was completed
 * @user_data: User-supplied data when callback was registered
 * @thumbnail: A pixbuf containing the thumbnail or %NULL. If application wishes to keep
 *      the structure, it must call g_object_ref() on it. The library does not cache
 *      returned pixbufs.
 *      The pixbuf may contain various options, which are prefixed with
 *      OSSO_THUMBNAIL_OPTION_PREFIX. The various options may be "Noimage" if there
 *      is no image but only metadata, "Title", "Artist" etc. To get the options,
 *      use gdk_pixbuf_get_option(thumbnail, OSSO_THUMBNAIL_OPTION_PREFIX "Option").
 * @error: The error or %NULL if there was none. Freed after callback returns.
 *
 * Called when the thumbnailing process finishes or there is an error
 */
typedef void (*OssoThumbnailFactoryFinishedCallback)(OssoThumbnailFactoryHandle handle,
    gpointer user_data, GdkPixbuf *thumbnail, GError *error);

/**
 * osso_thumbnail_factory_load:
 * @uri: Thumbnail will be created for this URI
 * @mime_type: MIME type of the resource the URI points to
 * @width: Desired thumbnail width
 * @height: Desired thumbnail height
 * @callback: Function to call when thumbnail creation finished or there was an error
 * @user_data: Optional data passed to the callback
 *
 * This function requests for the library to create a thumbnail, or load if from cache
 * if possible. The process is asynchronous, the function returns immediately. Right now
 * most processing is done in the idle callback and thumbnailing in a separate process.
 * If the process fails, the callback is called with the error.
 *
 * Returns: A #OssoThumbnailFactoryHandle if request succeeded or %NULL if there was
 *  a critical error
 */
OssoThumbnailFactoryHandle osso_thumbnail_factory_load(
            const gchar *uri, const gchar *mime_type,
            guint width, guint height,
            OssoThumbnailFactoryFinishedCallback callback,
            gpointer user_data);

/**
 * osso_thumbnail_factory_load_custom:
 * @flags: #OssoThumbnailFlags indicating which flags to use to create the thumbnail
 *
 * Same as osso_thumbnail_factory_load, but with custom options for thumbnail creation.
 * Argument list ends with key-value pairs for customizing.
 * Terminate argument list with -1.
 */
OssoThumbnailFactoryHandle osso_thumbnail_factory_load_custom(
            const gchar *uri, const gchar *mime_type,
            guint width, guint height,
            OssoThumbnailFactoryFinishedCallback callback,
            gpointer user_data, OssoThumbnailFlags flags, ...);


/**
 * osso_thumbnail_factory_cancel:
 * @handle: Handle to cancel
 *
 * Removes specified thumbnail request from the queue
 */
void osso_thumbnail_factory_cancel(OssoThumbnailFactoryHandle handle);

/**
 * osso_thumbnail_factory_move:
 * @src_uri: URI of the file that was moved
 * @dest_uri: URI of where the file was moved to
 *
 * Call to indicate the a file was moved and the thumbnail cache should be updated
 */
void osso_thumbnail_factory_move(const gchar *src_uri, const gchar *dest_uri);

/**
 * osso_thumbnail_factory_copy:
 * @src_uri: URI of the file that was copied
 * @dest_uri: URI of where the file was copied to
 *
 * Call to indicate the a file was copied and the thumbnail cache should be updated
 */
void osso_thumbnail_factory_copy(const gchar *src_uri, const gchar *dest_uri);

/**
 * osso_thumbnail_factory_remove:
 * @uri: URI of the file that was deleted
 *
 * Call to indicate the a file was removed and the thumbnail cache should be updated
 */
void osso_thumbnail_factory_remove(const gchar *uri);

/**
 * osso_thumbnail_factory_move_front:
 * @handle: Handle of thumbnail request to move
 *
 * Move the thumbnail for @handle to the front of the queue, so it will
 * be processed next
 */
void osso_thumbnail_factory_move_front(OssoThumbnailFactoryHandle handle);

/**
 * osso_thumbnail_factory_move_front_all_from:
 * @handle: Handle of the start of thumbnail requests to move
 *
 * Move all thumbnails starting from and including @handle to
 * the front of the queue
 * Thumbnail order is the sequence in which they were added
 */
void osso_thumbnail_factory_move_front_all_from(OssoThumbnailFactoryHandle handle);

/**
 * osso_thumbnail_factory_wait:
 *
 * Wait until all thumbnailing processes have finished
 */
void osso_thumbnail_factory_wait();

/**
 * osso_thumbnail_factory_clean_cache:
 * @max_size: Maximum size of cache in bytes. Set to -1 to disable. 0 deletes all entries.
 * @min_mtime: Minimum creation time of thumbnails. (usually now() - 30 days)
 *      Set to 0 to disable.
 *
 * Clean the thumbnail cache, deletes oldest entries first (based on thumbnail
 * creation date)
 */
void osso_thumbnail_factory_clean_cache(gint max_size, time_t min_mtime);

/**
 * osso_thumbnail_factory_set_debug:
 * @debug: boolean flag whether to enable debugging
 *
 * Enable/disable debugging. When debugging is enabled, some spam is outputted to notify
 * of thumbnails being created
 */
void osso_thumbnail_factory_set_debug(gboolean debug);

/**
 * OSSO_THUMBNAIL_OPTION_PREFIX:
 *
 * Prefix used in gdkpixbuf options (URL, mtime etc.)
 */
#define OSSO_THUMBNAIL_OPTION_PREFIX "tEXt::Thumb::"

#define OSSO_THUMBNAIL_APPLICATION "osso-thumbnail"

GQuark osso_thumbnail_error_quark();

/**
 * OSSO_THUMBNAIL_ERROR_DOMAIN:
 *
 * The error quark used by GErrors returned by the library
 */
#define OSSO_THUMBNAIL_ERROR_DOMAIN (osso_thumbnail_error_quark())

/**
 * OssoThumbnailError:
 *
 * GError codes returned by library
 */
typedef enum {
    OSSO_THUMBNAIL_ERROR_ILLEGAL_SIZE = 1,
    OSSO_THUMBNAIL_ERROR_NO_MIME_HANDLER,
    OSSO_THUMBNAIL_ERROR_NO_THUMB_DIR,
    OSSO_THUMBNAIL_ERROR_TEMP_FILE_FAILED,
    OSSO_THUMBNAIL_ERROR_SPAWN_FAILED,
    OSSO_THUMBNAIL_ERROR_CHILD_WATCH_FAILED,
    OSSO_THUMBNAIL_ERROR_PIXBUF_LOAD_FAILED,
    OSSO_THUMBNAIL_ERROR_NO_PIXBUF_OPTIONS,
    OSSO_THUMBNAIL_ERROR_THUMB_EXPIRED,
    OSSO_THUMBNAIL_ERROR_FAILURE_CACHED
} OssoThumbnailError;

G_END_DECLS

#endif
