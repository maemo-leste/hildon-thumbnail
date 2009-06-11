/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* -*- mode: C; c-file-style: "linux" -*- */
/* GdkPixbuf library - Main loading interface.
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define LOAD_BUFFER_SIZE 65536

typedef struct {
	guint max_pix, max_w, max_h;
	gboolean stop;
} LoadInfo;

#define DEFAULT_ERROR_DOMAIN	"HildonThumbnailerGdkPixbuf"
#define DEFAULT_ERROR		g_quark_from_static_string (DEFAULT_ERROR_DOMAIN)

static GdkPixbuf *
load_from_stream (GdkPixbufLoader  *loader,
		  GInputStream     *stream,
		  GCancellable     *cancellable,
		  LoadInfo         *info,
		  GError          **error)
{
	GdkPixbuf *pixbuf;
	gssize n_read;
	guchar buffer[LOAD_BUFFER_SIZE];
	gboolean res;

  	res = TRUE;
	while (!info->stop) { 
		n_read = g_input_stream_read (stream, 
					      buffer, 
					      sizeof (buffer), 
					      cancellable, 
					      error);
		if (n_read < 0) {
			res = FALSE;
			error = NULL; /* Ignore further errors */
			break;
		}

		if (n_read == 0)
			break;

		if (info->stop)
			break;

		if (!gdk_pixbuf_loader_write (loader, 
					      buffer, 
					      n_read, 
					      error)) {
			res = FALSE;
			error = NULL;
			break;
		}
	}

	if (!gdk_pixbuf_loader_close (loader, error)) {
		res = FALSE;
		error = NULL;
	}

	if (info->stop) {
		res = FALSE;
		g_set_error (error, DEFAULT_ERROR, 0,
			     "original image too large");
	}

	pixbuf = NULL;
	if (res) {
		pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
		if (pixbuf)
			g_object_ref (pixbuf);
	}

	return pixbuf;
}

typedef	struct {
	gint width;
	gint height;
	gboolean preserve_aspect_ratio;
} AtScaleData; 


static void
max_pix_check_cb (GdkPixbufLoader *loader, 
	 		   int              width,
		  	   int              height,
		  	   gpointer         data)
{
	LoadInfo *linfo = data;

	if (linfo->max_pix == 0 &&
	    linfo->max_w == 0 &&
	    linfo->max_h == 0) {
		return;
	}

	if (width * height > linfo->max_pix) {
		linfo->stop = TRUE;
	}

	if (width > linfo->max_w) {
		linfo->stop = TRUE;
	}

	if (height > linfo->max_h) {
		linfo->stop = TRUE;
	}

}

static void
at_scale_size_prepared_cb (GdkPixbufLoader *loader, 
	 		   int              width,
		  	   int              height,
		  	   gpointer         data)
{
	AtScaleData *info = data;

	g_return_if_fail (width > 0 && height > 0);

	if (info->preserve_aspect_ratio && 
	    (info->width > 0 || info->height > 0)) {
		if (info->width < 0)
		{
			width = width * (double)info->height/(double)height;
			height = info->height;
		}
		else if (info->height < 0)
		{
			height = height * (double)info->width/(double)width;
			width = info->width;
		}
		else if ((double)height * (double)info->width >
			 (double)width * (double)info->height) {
			width = 0.5 + (double)width * (double)info->height / (double)height;
			height = info->height;
		} else {
			height = 0.5 + (double)height * (double)info->width / (double)width;
			width = info->width;
		}
	} else {
		if (info->width > 0)
			width = info->width;
		if (info->height > 0)
			height = info->height;
	}
	
	width = MAX (width, 1);
        height = MAX (height, 1);

	gdk_pixbuf_loader_set_size (loader, width, height);
}

/**
 * my_gdk_pixbuf_new_from_stream_at_scale:
 * @stream:  a #GInputStream to load the pixbuf from
 * @width: The width the image should have or -1 to not constrain the width
 * @height: The height the image should have or -1 to not constrain the height
 * @preserve_aspect_ratio: %TRUE to preserve the image's aspect ratio
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @error: Return location for an error
 *
 * Creates a new pixbuf by loading an image from an input stream.  
 *
 * The file format is detected automatically. If %NULL is returned, then 
 * @error will be set. The @cancellable can be used to abort the operation
 * from another thread. If the operation was cancelled, the error 
 * %GIO_ERROR_CANCELLED will be returned. Other possible errors are in 
 * the #GDK_PIXBUF_ERROR and %G_IO_ERROR domains. 
 *
 * The image will be scaled to fit in the requested size, optionally 
 * preserving the image's aspect ratio. When preserving the aspect ratio, 
 * a @width of -1 will cause the image to be scaled to the exact given 
 * height, and a @height of -1 will cause the image to be scaled to the 
 * exact given width. When not preserving aspect ratio, a @width or 
 * @height of -1 means to not scale the image at all in that dimension.
 *
 * The stream is not closed.
 *
 * Return value: A newly-created pixbuf, or %NULL if any of several error 
 * conditions occurred: the file could not be opened, the image format is 
 * not supported, there was not enough memory to allocate the image buffer, 
 * the stream contained invalid data, or the operation was cancelled.
 *
 * Since: 2.14
 */
GdkPixbuf *
my_gdk_pixbuf_new_from_stream_at_scale (GInputStream  *stream,
				     gint	    width,
				     gint 	    height,
				     gboolean       preserve_aspect_ratio,
				     GCancellable  *cancellable,
		  	    	     GError       **error)
{
	GdkPixbufLoader *loader;
	GdkPixbuf *pixbuf;
	AtScaleData info;
	LoadInfo linfo;

	loader = gdk_pixbuf_loader_new ();

	info.width = width;
	info.height = height;
	info.preserve_aspect_ratio = preserve_aspect_ratio;

	linfo.stop = FALSE;
	linfo.max_pix = 0;
	linfo.max_h = 0;
	linfo.max_w = 0;

	g_signal_connect (loader, "size-prepared", 
			  G_CALLBACK (at_scale_size_prepared_cb), &info);

	pixbuf = load_from_stream (loader, stream, cancellable, &linfo, error);
	g_object_unref (loader);

	return pixbuf;
}

/**
 * my_gdk_pixbuf_new_from_stream:
 * @stream:  a #GInputStream to load the pixbuf from
 * @cancellable: optional #GCancellable object, %NULL to ignore
 * @error: Return location for an error
 *
 * Creates a new pixbuf by loading an image from an input stream.
 *
 * The file format is detected automatically. If %NULL is returned, then
 * @error will be set. The @cancellable can be used to abort the operation
 * from another thread. If the operation was cancelled, the error
 * %GIO_ERROR_CANCELLED will be returned. Other possible errors are in
 * the #GDK_PIXBUF_ERROR and %G_IO_ERROR domains.
 *
 * The stream is not closed.
 *
 * Return value: A newly-created pixbuf, or %NULL if any of several error
 * conditions occurred: the file could not be opened, the image format is
 * not supported, there was not enough memory to allocate the image buffer,
 * the stream contained invalid data, or the operation was cancelled.
 *
 * Since: 2.14
 **/
GdkPixbuf *
my_gdk_pixbuf_new_from_stream (GInputStream  *stream,
			    GCancellable  *cancellable,
			    guint          max_pix,
			    guint max_w, guint max_h,
			    GError       **error)
{
	GdkPixbuf *pixbuf;
	GdkPixbufLoader *loader;
	LoadInfo linfo;

	loader = gdk_pixbuf_loader_new ();

	g_signal_connect (loader, "size-prepared", 
			  G_CALLBACK (max_pix_check_cb), &linfo);

	linfo.stop = FALSE;
	linfo.max_pix = max_pix;
	linfo.max_w = max_w;
	linfo.max_h = max_h;

	pixbuf = load_from_stream (loader, stream, cancellable, &linfo, error);
	g_object_unref (loader);

	return pixbuf;
}


