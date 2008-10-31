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

#ifndef __LIBHILDONTHUMBNAILFACTORY_H__
#define __LIBHILDONTHUMBNAILFACTORY_H__

#include <unistd.h>
#include <sys/types.h>

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>


#define HILDON_TYPE_THUMBNAIL_FACTORY             (hildon_thumbnail_factory_get_type())
#define HILDON_THUMBNAIL_FACTORY(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), HILDON_TYPE_THUMBNAIL_FACTORY, HildonThumbnailFactory))
#define HILDON_THUMBNAIL_FACTORY_CLASS(c)         (G_TYPE_CHECK_CLASS_CAST ((c), HILDON_TYPE_THUMBNAIL_FACTORY, HildonThumbnailFactoryClass))
#define HILDON_THUMBNAIL_FACTORY_GET_CLASS(o)     (G_TYPE_INSTANCE_GET_CLASS ((o), HILDON_TYPE_THUMBNAIL_FACTORY, HildonThumbnailFactoryClass))

#define HILDON_TYPE_THUMBNAIL_REQUEST             (hildon_thumbnail_request_get_type())
#define HILDON_THUMBNAIL_REQUEST(o)               (G_TYPE_CHECK_INSTANCE_CAST ((o), HILDON_TYPE_THUMBNAIL_REQUEST, HildonThumbnailRequest))
#define HILDON_THUMBNAIL_REQUEST_CLASS(c)         (G_TYPE_CHECK_CLASS_CAST ((c), HILDON_TYPE_THUMBNAIL_REQUEST, HildonThumbnailRequestClass))
#define HILDON_THUMBNAIL_REQUEST_GET_CLASS(o)     (G_TYPE_INSTANCE_GET_CLASS ((o), HILDON_TYPE_THUMBNAIL_REQUEST, HildonThumbnailRequestClass))

G_BEGIN_DECLS

typedef struct _HildonThumbnailFactory HildonThumbnailFactory;
typedef struct _HildonThumbnailRequest HildonThumbnailRequest;

typedef struct _HildonThumbnailFactoryClass HildonThumbnailFactoryClass;
typedef struct _HildonThumbnailRequestClass HildonThumbnailRequestClass;

struct _HildonThumbnailFactory {
	GObject *parent;
};

struct _HildonThumbnailFactoryClass {
	GObjectClass *parent_class;
};

struct _HildonThumbnailRequest {
	GObject *parent;
};

struct _HildonThumbnailRequestClass {
	GObjectClass *parent_class;
};

GType hildon_thumbnail_factory_get_type (void);
GType hildon_thumbnail_request_get_type (void);

/** 
 * HildonThumbnailRequestPixbufCallback:
 * @self: the factory
 * @thumbnail: (allow-none): A pixbuf containing the thumbnail or %NULL. If application wishes to keep the structure, it must call g_object_ref() on it. The library does not cache returned pixbufs.
 * @error: (allow-none): The error or %NULL if there was none. Freed after callback returns.
 * @user_data: (allow-none): User-supplied data when thumbnail was requested
 *
 * Called when the thumbnailing process finishes or there is an error
 **/
typedef void (*HildonThumbnailRequestPixbufCallback)	(HildonThumbnailFactory *self,
		GdkPixbuf *thumbnail, GError *error, gpointer user_data);


/** 
 * HildonThumbnailRequestUriCallback:
 * @self: the factory
 * @thumbnail: (allow-none): A URI containing the best fit thumbnail or %NULL.
 * @error: (allow-none): The error or %NULL if there was none. Freed after callback returns.
 * @user_data: (allow-none): User-supplied data when thumbnail was requested
 *
 * Called when the thumbnailing process finishes or there is an error
 **/
typedef void (*HildonThumbnailRequestUriCallback)	(HildonThumbnailFactory *self,
		const gchar *thumbnail, GError *error, gpointer user_data);

/**
 * hildon_thumbnail_factory_get_instance:
 *
 * Request the factory singleton. You need to unref when finished with what got
 * returned to you.
 *
 * Returns: (transfer full): the factory
 **/
HildonThumbnailFactory* hildon_thumbnail_factory_get_instance (void);

/**
 * hildon_thumbnail_factory_request_pixbuf:
 * @self: the factory
 * @uri: an URI to the file which you want to request a thumbnail for
 * @width: Wanted width of the thumbnail
 * @height: Wanted height of the thumbnail
 * @mime_type: (allow-none): A MIME type hint for @uri
 * @callback: (allow-none): A callback that will be executed when the thumbnail is ready
 * @user_data: (allow-none): User-supplied data for @callback and @destroy
 * @destroy: (allow-none): a #GDestroyNotify to destroy @user_data as soon as @callback has ran
 *
 * Request the preparation, if needed, of a thumbnail and return it in @callback
 * as a #GdkPixbuf as soon as it's ready. This operation is asynchronous, the
 * creation of the thumbnail will not affect your running mainloop unless you
 * use one of the join methods on either #HildonThumbnailFactory or the 
 * #HildonThumbnailRequest being returned.
 *
 * Returns: (transfer full): A handle for the request that got created on a queue
 **/
HildonThumbnailRequest*
	 hildon_thumbnail_factory_request_pixbuf (HildonThumbnailFactory *self,
									 const gchar *uri,
									 guint width, guint height,
									 gboolean cropped,
									 const gchar *mime_type,
									 HildonThumbnailRequestPixbufCallback callback,
									 gpointer user_data,
									 GDestroyNotify destroy);


/**
 * hildon_thumbnail_factory_request_uri:
 * @self: the factory
 * @uri: an URI to the file which you want to request a thumbnail for
 * @width: Wanted width of the thumbnail
 * @height: Wanted height of the thumbnail
 * @mime_type: (allow-none): A MIME type hint for @uri
 * @callback: (allow-none): A callback that will be executed when the thumbnail is ready
 * @user_data: (allow-none): User-supplied data for @callback and @destroy
 * @destroy: (allow-none): a #GDestroyNotify to destroy @user_data as soon as @callback has ran
 *
 * Request the preparation, if needed, of a thumbnail and return it in @callback
 * as a URI as soon as it's ready. This operation is asynchronous, the
 * creation of the thumbnail will not affect your running mainloop unless you
 * use one of the join methods on either #HildonThumbnailFactory or the 
 * #HildonThumbnailRequest being returned.
 *
 * Returns: (transfer full): A handle for the request that got created on a queue
 **/
HildonThumbnailRequest*
	 hildon_thumbnail_factory_request_uri (HildonThumbnailFactory *self,
									 const gchar *uri,
									 guint width, guint height,
									 gboolean cropped,
									 const gchar *mime_type,
									 HildonThumbnailRequestUriCallback callback,
									 gpointer user_data,
									 GDestroyNotify destroy);
/**
 * hildon_thumbnail_factory_join:
 * @self: the factory
 *
 * Waits until all queued requests in @self are completed.
 **/
void hildon_thumbnail_factory_join (HildonThumbnailFactory *self);

/**
 * hildon_thumbnail_request_unqueue:
 * @self: the request
 *
 * Attempts to unqueue a request. If the request is already active, then this
 * method has no guaranteed effect.
 **/
void hildon_thumbnail_request_unqueue (HildonThumbnailRequest *self);

/**
 * hildon_thumbnail_request_join:
 * @self: the request
 *
 * Waits until the queued requests is completed.
 **/
void hildon_thumbnail_request_join (HildonThumbnailRequest *self);

typedef gpointer HildonThumbnailFactoryHandle;


/**
 * hildon_thumbnail_is_cached:
 * @uri: URI of the original
 * @width: width
 * @height: height
 * @is_cropped: is cropped
 *
 * Determines whether or not the there's already a thumbnail for @uri
 *
 * Returns: Whether or not a thumbnail was once made
 **/

gboolean hildon_thumbnail_is_cached (const gchar *uri, guint width, guint height, gboolean is_cropped);

/**
 * hildon_thumbnail_get_uri:
 * @uri: URI of the original
 * @width: width
 * @height: height
 * @is_cropped: is cropped
 *
 * Gives you the absolute predicted URI to the thumbnail for @uri. This function
 * doesn't care about the thumbnail having been created (cached) already or not,
 * it just returns the predicted path.
 *
 * Returns: (caller-owns): the uri to the thumbnail. You must free this.
 **/
gchar * hildon_thumbnail_get_uri (const gchar *uri, guint width, guint height, gboolean is_cropped);



/**
 * HildonThumbnailFlags:
 *
 * Flags to use for thumbnail creation
 */
typedef enum {
    HILDON_THUMBNAIL_FLAG_CROP = 1 << 1,
    HILDON_THUMBNAIL_FLAG_RECREATE = 1 << 2
} HildonThumbnailFlags;

/**
 * HildonThumbnailFactoryFinishedCallback:
 * @handle: Handle of the thumbnail that was completed
 * @user_data: User-supplied data when callback was registered
 * @thumbnail: A pixbuf containing the thumbnail or %NULL. If application wishes to keep
 *      the structure, it must call g_object_ref() on it. The library does not cache
 *      returned pixbufs.
 *      The pixbuf may contain various options, which are prefixed with
 *      HILDON_THUMBNAIL_OPTION_PREFIX. The various options may be "Noimage" if there
 *      is no image but only metadata, "Title", "Artist" etc. To get the options,
 *      use gdk_pixbuf_get_option(thumbnail, HILDON_THUMBNAIL_OPTION_PREFIX "Option").
 * @error: The error or %NULL if there was none. Freed after callback returns.
 *
 * Called when the thumbnailing process finishes or there is an error
 *
 * Decprecated use hildon_thumbnail_factory_request and HildonThumbnailRequestCallback instead
 */
typedef void (*HildonThumbnailFactoryFinishedCallback)(HildonThumbnailFactoryHandle handle,
    gpointer user_data, GdkPixbuf *thumbnail, GError *error);

/**
 * hildon_thumbnail_factory_load:
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
 * Returns: A #HildonThumbnailFactoryHandle if request succeeded or %NULL if there was
 *  a critical error
 *
 * Decprecated use hildon_thumbnail_factory_request instead
 */
HildonThumbnailFactoryHandle hildon_thumbnail_factory_load(
            const gchar *uri, const gchar *mime_type,
            guint width, guint height,
            HildonThumbnailFactoryFinishedCallback callback,
            gpointer user_data);

/**
 * hildon_thumbnail_factory_load_custom:
 * @flags: #HildonThumbnailFlags indicating which flags to use to create the thumbnail
 *
 * Same as hildon_thumbnail_factory_load, but with custom options for thumbnail creation.
 * Argument list ends with key-value pairs for customizing.
 * Terminate argument list with -1.
 *
 * Decprecated use hildon_thumbnail_factory_request instead
 */
HildonThumbnailFactoryHandle hildon_thumbnail_factory_load_custom(
            const gchar *uri, const gchar *mime_type,
            guint width, guint height,
            HildonThumbnailFactoryFinishedCallback callback,
            gpointer user_data, HildonThumbnailFlags flags, ...);


/**
 * hildon_thumbnail_factory_cancel:
 * @handle: Handle to cancel
 *
 * Removes specified thumbnail request from the queue
 *
 * Deprecated use hildon_thumbnail_request_unqueue
 */
void hildon_thumbnail_factory_cancel(HildonThumbnailFactoryHandle handle);

/**
 * hildon_thumbnail_factory_move:
 * @src_uri: URI of the file that was moved
 * @dest_uri: URI of where the file was moved to
 *
 * Call to indicate the a file was moved and the thumbnail cache should be updated
 */
void hildon_thumbnail_factory_move(const gchar *src_uri, const gchar *dest_uri);

/**
 * hildon_thumbnail_factory_copy:
 * @src_uri: URI of the file that was copied
 * @dest_uri: URI of where the file was copied to
 *
 * Call to indicate the a file was copied and the thumbnail cache should be updated
 */
void hildon_thumbnail_factory_copy(const gchar *src_uri, const gchar *dest_uri);

/**
 * hildon_thumbnail_factory_remove:
 * @uri: URI of the file that was deleted
 *
 * Call to indicate the a file was removed and the thumbnail cache should be updated
 */
void hildon_thumbnail_factory_remove(const gchar *uri);

/**
 * hildon_thumbnail_factory_move_front:
 * @handle: Handle of thumbnail request to move
 *
 * Move the thumbnail for @handle to the front of the queue, so it will
 * be processed next
 *
 * Deprecated
 */
void hildon_thumbnail_factory_move_front(HildonThumbnailFactoryHandle handle);

/**
 * hildon_thumbnail_factory_move_front_all_from:
 * @handle: Handle of the start of thumbnail requests to move
 *
 * Move all thumbnails starting from and including @handle to
 * the front of the queue
 * Thumbnail order is the sequence in which they were added
 *
 * Deprecated
 */
void hildon_thumbnail_factory_move_front_all_from(HildonThumbnailFactoryHandle handle);

/**
 * hildon_thumbnail_factory_wait:
 *
 * Wait until all thumbnailing processes have finished
 *
 * Deprecated use hildon_thumbnail_factory_join instead
 */
void hildon_thumbnail_factory_wait();

/**
 * hildon_thumbnail_factory_clean_cache:
 * @max_size: Maximum size of cache in bytes. Set to -1 to disable. 0 deletes all entries.
 * @min_mtime: Minimum creation time of thumbnails. (usually now() - 30 days)
 *      Set to 0 to disable.
 *
 * Clean the thumbnail cache, deletes oldest entries first (based on thumbnail
 * creation date)
 */
void hildon_thumbnail_factory_clean_cache(gint max_size, time_t min_mtime);

/**
 * hildon_thumbnail_factory_set_debug:
 * @debug: boolean flag whether to enable debugging
 *
 * Enable/disable debugging. When debugging is enabled, some spam is outputted to notify
 * of thumbnails being created
 */
void hildon_thumbnail_factory_set_debug(gboolean debug);

/**
 * HILDON_THUMBNAIL_OPTION_PREFIX:
 *
 * Prefix used in gdkpixbuf options (URL, mtime etc.)
 */
#define HILDON_THUMBNAIL_OPTION_PREFIX "tEXt::Thumb::"

#define HILDON_THUMBNAIL_APPLICATION "hildon-thumbnail"

GQuark hildon_thumbnail_error_quark(void);

/**
 * HILDON_THUMBNAIL_ERROR_DOMAIN:
 *
 * The error quark used by GErrors returned by the library
 */
#define HILDON_THUMBNAIL_ERROR_DOMAIN (hildon_thumbnail_error_quark())

/**
 * HildonThumbnailError:
 *
 * GError codes returned by library
 */
typedef enum {
	/* Deprecated */
    HILDON_THUMBNAIL_ERROR_ILLEGAL_SIZE = 1,
	/* Deprecated */
    HILDON_THUMBNAIL_ERROR_NO_MIME_HANDLER,
	/* Deprecated */
    HILDON_THUMBNAIL_ERROR_NO_THUMB_DIR,
	/* Deprecated */
    HILDON_THUMBNAIL_ERROR_TEMP_FILE_FAILED,
	/* Deprecated */
    HILDON_THUMBNAIL_ERROR_SPAWN_FAILED,
	/* Deprecated */
    HILDON_THUMBNAIL_ERROR_CHILD_WATCH_FAILED,
	/* Deprecated */
    HILDON_THUMBNAIL_ERROR_PIXBUF_LOAD_FAILED,
	/* Deprecated */
    HILDON_THUMBNAIL_ERROR_NO_PIXBUF_OPTIONS,
	/* Deprecated */
    HILDON_THUMBNAIL_ERROR_THUMB_EXPIRED,
	/* Deprecated */
    HILDON_THUMBNAIL_ERROR_FAILURE_CACHED
} HildonThumbnailError;

G_END_DECLS

#endif
