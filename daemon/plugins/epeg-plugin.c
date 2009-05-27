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
 *
 * Orientation code got copied in part from LGPL Gtk+'s gdk-pixbuf/io-jpeg.c,
 * original author of Gtk+'s io-jpeg.c is is Michael Zucchi
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <dbus/dbus-glib-bindings.h>

#include <Epeg.h>

#ifdef HAVE_LIBEXIF
#include <setjmp.h>
#include <jpeglib.h>
#include <jerror.h>
#include <libexif/exif-data.h>
#include "epeg_private.h"
#define EXIF_JPEG_MARKER   JPEG_APP0+1
#define EXIF_IDENT_STRING  "Exif\000\000"
#endif /* HAVE_LIBEXIF */

#define EPEG_ERROR_DOMAIN	"HildonThumbnailerEpeg"
#define EPEG_ERROR		g_quark_from_static_string (EPEG_ERROR_DOMAIN)

#include "utils.h"
#include "epeg-plugin.h"

#include <hildon-thumbnail-plugin.h>

#ifdef LARGE_THUMBNAILS
	#define LARGE	LARGE
#else
	#ifdef NORMAL_THUMBNAILS
		#define LARGE	128
	#else
		#define LARGE	124
	#endif
#endif


static gchar **supported = NULL;
static gboolean do_cropped = TRUE;
static GFileMonitor *monitor = NULL;

#ifdef HAVE_LIBEXIF
/* All this orientation stuff is copied from Gtk+'s gdk-pixbuf/io-jpeg.c */

const char leth[]  = {0x49, 0x49, 0x2a, 0x00};	// Little endian TIFF header
const char beth[]  = {0x4d, 0x4d, 0x00, 0x2a};	// Big endian TIFF header
const char types[] = {0x00, 0x01, 0x01, 0x02, 0x04, 0x08, 0x00, 
		      0x08, 0x00, 0x04, 0x08}; 	// size in bytes for EXIF types
 
#define DE_ENDIAN16(val) endian == G_BIG_ENDIAN ? GUINT16_FROM_BE(val) : GUINT16_FROM_LE(val)
#define DE_ENDIAN32(val) endian == G_BIG_ENDIAN ? GUINT32_FROM_BE(val) : GUINT32_FROM_LE(val)
 
#define ENDIAN16_IT(val) endian == G_BIG_ENDIAN ? GUINT16_TO_BE(val) : GUINT16_TO_LE(val)
#define ENDIAN32_IT(val) endian == G_BIG_ENDIAN ? GUINT32_TO_BE(val) : GUINT32_TO_LE(val)
 
#define EXIF_JPEG_MARKER   JPEG_APP0+1
#define EXIF_IDENT_STRING  "Exif\000\000"

static unsigned short de_get16(void *ptr, guint endian)
{
       unsigned short val;

       memcpy(&val, ptr, sizeof(val));
       val = DE_ENDIAN16(val);

       return val;
}

static unsigned int de_get32(void *ptr, guint endian)
{
       unsigned int val;

       memcpy(&val, ptr, sizeof(val));
       val = DE_ENDIAN32(val);

       return val;
}


static gint 
get_orientation (j_decompress_ptr cinfo)
{
	/* This function looks through the meta data in the libjpeg decompress structure to
	   determine if an EXIF Orientation tag is present and if so return its value (1-8). 
	   If no EXIF Orientation tag is found 0 (zero) is returned. */

 	guint   i;              /* index into working buffer */
 	guint   orient_tag_id;  /* endianed version of orientation tag ID */
	guint   ret;            /* Return value */
 	guint   offset;        	/* de-endianed offset in various situations */
 	guint   tags;           /* number of tags in current ifd */
 	guint   type;           /* de-endianed type of tag used as index into types[] */
 	guint   count;          /* de-endianed count of elements in a tag */
        guint   tiff = 0;   	/* offset to active tiff header */
        guint   endian = 0;   	/* detected endian of data */

	jpeg_saved_marker_ptr exif_marker;  /* Location of the Exif APP1 marker */
	jpeg_saved_marker_ptr cmarker;	    /* Location to check for Exif APP1 marker */

	/* check for Exif marker (also called the APP1 marker) */
	exif_marker = NULL;
	cmarker = cinfo->marker_list;
	while (cmarker) {
		if (cmarker->marker == EXIF_JPEG_MARKER) {
			/* The Exif APP1 marker should contain a unique
			   identification string ("Exif\0\0"). Check for it. */
			if (!memcmp (cmarker->data, EXIF_IDENT_STRING, 6)) {
				exif_marker = cmarker;
				}
			}
		cmarker = cmarker->next;
	}
	  
	/* Did we find the Exif APP1 marker? */
	if (exif_marker == NULL)
		return 0;

	/* Do we have enough data? */
	if (exif_marker->data_length < 32)
		return 0;

        /* Check for TIFF header and catch endianess */
 	i = 0;

	/* Just skip data until TIFF header - it should be within 16 bytes from marker start.
	   Normal structure relative to APP1 marker -
		0x0000: APP1 marker entry = 2 bytes
	   	0x0002: APP1 length entry = 2 bytes
		0x0004: Exif Identifier entry = 6 bytes
		0x000A: Start of TIFF header (Byte order entry) - 4 bytes  
		    	- This is what we look for, to determine endianess.
		0x000E: 0th IFD offset pointer - 4 bytes

		exif_marker->data points to the first data after the APP1 marker
		and length entries, which is the exif identification string.
		The TIFF header should thus normally be found at i=6, below,
		and the pointer to IFD0 will be at 6+4 = 10.
 	*/
		    
 	while (i < 16) {
 
 		/* Little endian TIFF header */
 		if (memcmp (&exif_marker->data[i], leth, 4) == 0){ 
 			endian = G_LITTLE_ENDIAN;
                }
 
 		/* Big endian TIFF header */
 		else if (memcmp (&exif_marker->data[i], beth, 4) == 0){ 
 			endian = G_BIG_ENDIAN;
                }
 
 		/* Keep looking through buffer */
 		else {
 			i++;
 			continue;
 		}
 		/* We have found either big or little endian TIFF header */
 		tiff = i;
 		break;
        }

 	/* So did we find a TIFF header or did we just hit end of buffer? */
 	if (tiff == 0) 
		return 0;
 
        /* Endian the orientation tag ID, to locate it more easily */
        orient_tag_id = ENDIAN16_IT(0x112);
 
        /* Read out the offset pointer to IFD0 */
        offset  = de_get32(&exif_marker->data[i] + 4, endian);
 	i       = i + offset;

	/* Check that we still are within the buffer and can read the tag count */
	if ((i + 2) > exif_marker->data_length)
		return 0;

	/* Find out how many tags we have in IFD0. As per the TIFF spec, the first
	   two bytes of the IFD contain a count of the number of tags. */
	tags    = de_get16(&exif_marker->data[i], endian);
	i       = i + 2;

	/* Check that we still have enough data for all tags to check. The tags
	   are listed in consecutive 12-byte blocks. The tag ID, type, size, and
	   a pointer to the actual value, are packed into these 12 byte entries. */
	if ((i + tags * 12) > exif_marker->data_length)
		return 0;

	/* Check through IFD0 for tags of interest */
	while (tags--){
		type   = de_get16(&exif_marker->data[i + 2], endian);
		count  = de_get32(&exif_marker->data[i + 4], endian);

		/* Is this the orientation tag? */
		if (memcmp (&exif_marker->data[i], (char *) &orient_tag_id, 2) == 0){ 
 
			/* Check that type and count fields are OK. The orientation field 
			   will consist of a single (count=1) 2-byte integer (type=3). */
			if (type != 3 || count != 1) return 0;

			/* Return the orientation value. Within the 12-byte block, the
			   pointer to the actual data is at offset 8. */
			ret =  de_get16(&exif_marker->data[i + 8], endian);
			return ret <= 8 ? ret : 0;
		}
		/* move the pointer to the next 12-byte tag field. */
		i = i + 12;
	}

	return 0; /* No EXIF Orientation tag found */
}

struct tej_error_mgr {
	struct jpeg_error_mgr jpeg;
	jmp_buf setjmp_buffer;
};


static void 
on_jpeg_error_exit (j_common_ptr cinfo)
{
	struct tej_error_mgr *h = (struct tej_error_mgr *) cinfo->err;
	/* (*cinfo->err->output_message)(cinfo); */
	longjmp (h->setjmp_buffer, 1);
}

#endif


static void
restore_orientation (const gchar *path, GdkPixbuf *pixbuf)
{
#ifdef HAVE_LIBEXIF
	FILE *f = fopen (path, "r");
	if (f) {
		int is_otag;
		char otag_str[5];
		struct jpeg_decompress_struct  cinfo;
		struct tej_error_mgr	       tejerr;
		struct jpeg_marker_struct     *marker;
		
		cinfo.err = jpeg_std_error (&tejerr.jpeg);
		tejerr.jpeg.error_exit = on_jpeg_error_exit;

		if (setjmp (tejerr.setjmp_buffer)) {
			goto fail;
		}

		jpeg_create_decompress (&cinfo);
		jpeg_stdio_src (&cinfo, f);

		jpeg_save_markers (&cinfo, EXIF_JPEG_MARKER, 0xffff);
		jpeg_read_header (&cinfo, TRUE);

		is_otag = get_orientation (&cinfo);

		if (is_otag) {
			g_snprintf (otag_str, sizeof (otag_str), "%d", is_otag);
			gdk_pixbuf_set_option (pixbuf, "orientation", otag_str);
		}

		jpeg_finish_decompress (&cinfo);

		fail:
		jpeg_destroy_decompress (&cinfo);
		fclose (f);
	}

#endif
}

const gchar** 
hildon_thumbnail_plugin_supported (void)
{
	if (!supported) {
		supported = (gchar **) g_malloc0 (sizeof (gchar *) * 2);
		supported[0] = g_strdup ("image/jpeg");
		supported[1] = NULL;
	}

	return (const gchar**) supported;
}

static GdkPixbuf*
crop_resize (GdkPixbuf *src, int width, int height) {
	return hildon_thumbnail_crop_resize (src, width, height);
}




static void
wanted_size (int a, int b, int width, int height, int *w, int *h) {

	if(a < width && b < height) {
		*w = width;
		*h = height;
	} else {
		int rw, rh;

		rw = a / width;
		rh = b / height;
		if (rw > rh) {
		    *h = b / rw;
		    *w = width;
		} else {
		    *w =  a / rh;
		    *h = height;
		}
	}

	return;
}


static void
destroy_pixbuf (guchar *pixels, gpointer data)
{
	epeg_pixels_free ((Epeg_Image *) data, pixels);
	epeg_close ((Epeg_Image *) data);
}

void
hildon_thumbnail_plugin_create (GStrv uris, gchar *mime_hint, GStrv *failed_uris, GError **error)
{
	guint i = 0;
	GString *errors = NULL;
	GList *failed = NULL;

	while (uris[i] != NULL) {
		Epeg_Image *im;
		gchar *uri = uris[i];
		GFile *file = NULL;
		gchar *path;
		gboolean had_err = FALSE;
		guchar *data;
		GdkPixbuf *pixbuf_large = NULL, 
			  *pixbuf_normal, *pixbuf_large1 = NULL,
			  *pixbuf_cropped;
		guint64 mtime;
		GFileInfo *finfo = NULL;
		GError *nerror = NULL;
		guint ow, oh;
		const guchar *rgb8_pixels;
		guint width; guint height;
		guint rowstride; 
		gboolean err_file = FALSE;
		int ww, wh;

		file = g_file_new_for_uri (uri);
		path = g_file_get_path (file);

		if (!path) {
			had_err = TRUE;
			goto nerror_handler;
		}

		finfo = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED,
					   G_FILE_QUERY_INFO_NONE,
					   NULL, &nerror);

		if (nerror)
			goto nerror_handler;

		mtime = g_file_info_get_attribute_uint64 (finfo, G_FILE_ATTRIBUTE_TIME_MODIFIED);


		if (
#ifdef LARGE_THUMBNAILS
		    !hildon_thumbnail_outplugins_needs_out (HILDON_THUMBNAIL_PLUGIN_OUTTYPE_LARGE, mtime, uri, &err_file) &&
#endif
#ifdef NORMAL_THUMBNAILS
		    !hildon_thumbnail_outplugins_needs_out (HILDON_THUMBNAIL_PLUGIN_OUTTYPE_NORMAL, mtime, uri, &err_file) &&
#endif
		    !hildon_thumbnail_outplugins_needs_out (HILDON_THUMBNAIL_PLUGIN_OUTTYPE_CROPPED, mtime, uri, &err_file))
			goto nerror_handler;

		im = epeg_file_open (path);

		if (!im) {
			had_err = TRUE;
			goto nerror_handler;
		}

		epeg_size_get (im, &ow, &oh);

		wanted_size (ow, oh, 256 , 256, &ww, &wh);

		// printf ("%dx%d -> %dx%d\n", ow, oh, ww, wh);

		if (ow < LARGE || oh < LARGE) {
			/* Epeg doesn't behave as expected when the destination is larger
			 * than the source */

			pixbuf_large1 = gdk_pixbuf_new_from_file_at_scale (path, 
									 256, 256, 
									  TRUE,
									 &nerror);
			epeg_close (im);

			if (nerror) {
				pixbuf_large = pixbuf_large1;
				goto nerror_handler;
			}

		} else {
			//gchar *large=NULL, *normal=NULL, *cropped=NULL;

			epeg_decode_colorspace_set (im, EPEG_RGB8);
			epeg_decode_size_set (im, ww, wh);
			// epeg_quality_set (im, 75);
			epeg_thumbnail_comments_enable (im, 0);

			//hildon_thumbnail_util_get_thumb_paths (uri, &large, &normal, &cropped,
			//				       NULL, NULL, NULL, FALSE);

			//epeg_file_output_set (im, large);
			//epeg_encode (im);
			//epeg_close (im);

			//pixbuf_large = gdk_pixbuf_new_from_file (large, &nerror);

			//if (nerror) {
			//	pixbuf_large = pixbuf_large1;
			//	goto nerror_handler;
			// }

			data = (guchar *) epeg_pixels_get (im, 0, 0, ww, wh);

			pixbuf_large1 = gdk_pixbuf_new_from_data ((const guchar*) data, 
									  GDK_COLORSPACE_RGB, FALSE, 
									  8, ww, wh, ww*3,
									  destroy_pixbuf, im);

			restore_orientation (path, pixbuf_large1);

		}

		pixbuf_large = gdk_pixbuf_apply_embedded_orientation (pixbuf_large1);

		g_object_unref (pixbuf_large1);

#ifdef LARGE_THUMBNAILS
		if (hildon_thumbnail_outplugins_needs_out (HILDON_THUMBNAIL_PLUGIN_OUTTYPE_LARGE, mtime, uri, &err_file)) {

			rgb8_pixels = gdk_pixbuf_get_pixels (pixbuf_large);
			width = gdk_pixbuf_get_width (pixbuf_large);
			height = gdk_pixbuf_get_height (pixbuf_large);
			rowstride = gdk_pixbuf_get_rowstride (pixbuf_large);

			hildon_thumbnail_outplugins_do_out (rgb8_pixels, 
							    width,
							    height,
							    rowstride, 
							    gdk_pixbuf_get_bits_per_sample (pixbuf_large),
							    gdk_pixbuf_get_has_alpha (pixbuf_large),
							    HILDON_THUMBNAIL_PLUGIN_OUTTYPE_LARGE,
							    mtime, uri, 
							    &nerror);

			if (nerror)
				goto nerror_handler;

		}
#endif

		if (do_cropped && hildon_thumbnail_outplugins_needs_out (HILDON_THUMBNAIL_PLUGIN_OUTTYPE_CROPPED, mtime, uri, &err_file)) {

			pixbuf_cropped = crop_resize (pixbuf_large, 124, 124);

			rgb8_pixels = gdk_pixbuf_get_pixels (pixbuf_cropped);
			width = gdk_pixbuf_get_width (pixbuf_cropped);
			height = gdk_pixbuf_get_height (pixbuf_cropped);
			rowstride = gdk_pixbuf_get_rowstride (pixbuf_cropped);

			hildon_thumbnail_outplugins_do_out (rgb8_pixels, 
							    width,
							    height,
							    rowstride, 
							    gdk_pixbuf_get_bits_per_sample (pixbuf_cropped),
							    gdk_pixbuf_get_has_alpha (pixbuf_cropped),
							    HILDON_THUMBNAIL_PLUGIN_OUTTYPE_CROPPED,
							    mtime, uri, 
							    &nerror);

			g_object_unref (pixbuf_cropped);

			if (nerror)
				goto nerror_handler;
		}

#ifdef NORMAL_THUMBNAILS
		if (hildon_thumbnail_outplugins_needs_out (HILDON_THUMBNAIL_PLUGIN_OUTTYPE_NORMAL, mtime, uri, &err_file)) {

			pixbuf_normal = gdk_pixbuf_scale_simple (pixbuf_large,
								 128, 128,
								 GDK_INTERP_HYPER);

			rgb8_pixels = gdk_pixbuf_get_pixels (pixbuf_normal);
			width = gdk_pixbuf_get_width (pixbuf_normal);
			height = gdk_pixbuf_get_height (pixbuf_normal);
			rowstride = gdk_pixbuf_get_rowstride (pixbuf_normal);

			hildon_thumbnail_outplugins_do_out (rgb8_pixels, 
							    width,
							    height,
							    rowstride, 
							    gdk_pixbuf_get_bits_per_sample (pixbuf_normal),
							    gdk_pixbuf_get_has_alpha (pixbuf_normal),
							    HILDON_THUMBNAIL_PLUGIN_OUTTYPE_NORMAL,
							    mtime, uri, 
							    &nerror);

			g_object_unref (pixbuf_normal);

			if (nerror)
				goto nerror_handler;

		}
#endif

		nerror_handler:

		if (had_err || nerror || err_file) {
			gchar *msg;
			if (nerror) {
				msg = g_strdup (nerror->message);
				g_error_free (nerror);
				nerror = NULL;
			} else if (err_file)
				msg = g_strdup ("Failed before");
			else
				msg = g_strdup_printf ("Can't open %s", uri);
			if (!errors)
				errors = g_string_new ("");
			g_string_append_printf (errors, "[`%s': %s] ", 
								    uri, msg);
			if (!err_file) {
				GFile *file;
				GFileInfo *info;
				file = g_file_new_for_uri (uri);
				info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED,
							  G_FILE_QUERY_INFO_NONE,
							  NULL, NULL);
				if (info) {
					guint64 mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
					hildon_thumbnail_outplugins_put_error (mtime, uri, nerror);
					g_object_unref (info);
				}

				g_object_unref (file);
			}
			failed = g_list_prepend (failed, g_strdup (uri));
			g_free (msg);
		}

		if (pixbuf_large)
			g_object_unref (pixbuf_large);
		if (file)
			g_object_unref (file);
		if (finfo)
			g_object_unref (finfo);

		g_free (path);

		i++;
	}

	if (errors && failed) {
		guint t = 0;
		GStrv furis = (GStrv) g_malloc0 (sizeof (gchar*) * (g_list_length (failed) + 1));
		GList *copy = failed;

		t = 0;

		while (copy) {
			furis[t] = copy->data;
			copy = g_list_next (copy);
			t++;
		}

		furis[t] = NULL;

		*failed_uris = furis;

		g_list_free (failed);

		g_set_error (error, EPEG_ERROR, 0,
			     "%s", errors->str);

		g_string_free (errors, TRUE);
	}

	return;
}

gboolean  
hildon_thumbnail_plugin_stop (void)
{
	if (supported)
		g_strfreev (supported);
	supported = NULL;
	if (monitor)
		g_object_unref (monitor);
	return FALSE;
}

static void
reload_config (const gchar *config) 
{
	GKeyFile *keyfile;
	GError *error = NULL;

	keyfile = g_key_file_new ();

	if (!g_key_file_load_from_file (keyfile, config, G_KEY_FILE_NONE, NULL)) {
		do_cropped = TRUE;
		g_key_file_free (keyfile);
		return;
	}

	do_cropped = g_key_file_get_boolean (keyfile, "Hildon Thumbnailer", "DoCropping", &error);

	if (error) {
		do_cropped = TRUE;
		g_error_free (error);
	}

	g_key_file_free (keyfile);
}

static void 
on_file_changed (GFileMonitor *monitor_, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data)
{
	if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT || event_type == G_FILE_MONITOR_EVENT_CREATED) {
		gchar *config = g_file_get_path (file);
		reload_config (config);
		g_free (config);
	}
}

void 
hildon_thumbnail_plugin_init (gboolean *cropping, hildon_thumbnail_register_func func, gpointer thumbnailer, GModule *module, GError **error)
{
	gchar *config = g_build_filename (g_get_user_config_dir (), "hildon-thumbnailer", "epeg-plugin.conf", NULL);
	GFile *file = g_file_new_for_path (config);
	guint i = 0;
	const gchar **supported_;
	const gchar *uri_schemes[2] = { "file", NULL };

	monitor =  g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);

	g_signal_connect (G_OBJECT (monitor), "changed", 
			  G_CALLBACK (on_file_changed), NULL);

	g_object_unref (file);

	reload_config (config);

	*cropping = do_cropped;

	if (func) {
		supported_ = hildon_thumbnail_plugin_supported ();
		if (supported_) {
			while (supported_[i] != NULL) {
				func (thumbnailer, supported_[i], module, (const GStrv) uri_schemes, 1);
				i++;
			}
		}
	}

	g_free (config);
}
