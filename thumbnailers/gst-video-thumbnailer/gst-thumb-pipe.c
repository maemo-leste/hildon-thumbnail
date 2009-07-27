/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * This file is part of hildon-thumbnail package
 *
 * Copyright (C) 2009 Nokia Corporation.  All Rights reserved.
 *
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

#include "gst-thumb-pipe.h"

#include <string.h>

#include <gst/gst.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define THUMBER_PIPE_ERROR_DOMAIN "ThumberPipeError"
#define SEEK_TIMEOUT 5
#define PIPE_TIMEOUT 10
#define VALID_VARIANCE_THRESHOLD  256.0


static void           newpad_callback                  (GstElement       *decodebin,
							GstPad           *pad,
							gboolean          last,
							ThumberPipe      *pipe);

static gboolean       stream_continue_callback         (GstElement    *bin,
                                                        GstPad        *pad,
                                                        GstCaps       *caps,
                                                        ThumberPipe   *pipe);

static gboolean       wait_for_state_change            (ThumberPipe *pipe,
							GstState     state,
							GError     **error);

static gboolean       wait_for_image_buffer            (ThumberPipe *pipe,
							const gchar *uri,
							GError     **error);

static gboolean       create_thumbnails                (const gchar *uri,
							GdkPixbuf   *pixbuf,
							gboolean     standard,
							gboolean     cropped,
							GError     **error);

static gboolean       check_for_valid_thumb            (GdkPixbuf   *pixbuf);

static gboolean       initialize                       (ThumberPipe *pipe,
							const gchar *mime,
							guint size,
							GError **error);
static void           deinitialize                     (ThumberPipe *pipe);

G_DEFINE_TYPE (ThumberPipe, thumber_pipe, G_TYPE_OBJECT)

#define THUMBER_PIPE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TYPE_THUMBER_PIPE, ThumberPipePrivate))

typedef struct {
	GstElement     *pipeline;

	GstElement     *source;
	GstElement     *decodebin;

	GstElement     *sinkbin;
	GstElement     *video_scaler;
	GstElement     *video_filter;
	GstElement     *video_sink;

	gboolean        standard;
	gboolean        cropped;
} ThumberPipePrivate;

enum {
	PROP_0,
	PROP_STANDARD,
	PROP_CROPPED
};

enum {
	NO_ERROR,
	INITIALIZATION_ERROR,
	RUNNING_ERROR,
	THUMBNAIL_ERROR
};

GQuark
error_quark (void)
{
	return g_quark_from_static_string (THUMBER_PIPE_ERROR_DOMAIN);
}

ThumberPipe *
thumber_pipe_new ()
{
	return g_object_new (TYPE_THUMBER_PIPE, NULL);
}

static gboolean
thumber_pipe_get_standard (ThumberPipe *pipe)
{
	ThumberPipePrivate *priv;

	priv = THUMBER_PIPE_GET_PRIVATE (pipe);
	return priv->standard;
}

static void
thumber_pipe_set_standard (ThumberPipe *pipe, gboolean standard)
{
	ThumberPipePrivate *priv;

	priv = THUMBER_PIPE_GET_PRIVATE (pipe);
	priv->standard = standard;
}

static gboolean
thumber_pipe_get_cropped (ThumberPipe *pipe)
{
	ThumberPipePrivate *priv;

	priv = THUMBER_PIPE_GET_PRIVATE (pipe);
	return priv->cropped;
}

static void
thumber_pipe_set_cropped (ThumberPipe *pipe, gboolean cropped)
{
	ThumberPipePrivate *priv;

	priv = THUMBER_PIPE_GET_PRIVATE (pipe);
	priv->cropped = cropped;
}

static void
thumber_pipe_set_property (GObject      *object,
			   guint         prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_STANDARD:
		thumber_pipe_set_standard (THUMBER_PIPE (object), g_value_get_boolean (value));
		break;
	case PROP_CROPPED:
		thumber_pipe_set_cropped (THUMBER_PIPE (object), g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}


static void
thumber_pipe_get_property (GObject    *object,
			   guint       prop_id,
			   GValue     *value,
			   GParamSpec *pspec)
{
	ThumberPipePrivate *priv;
	
	priv = THUMBER_PIPE_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_STANDARD:
		g_value_set_boolean (value,
				     thumber_pipe_get_standard (THUMBER_PIPE (object)));
		break;
	case PROP_CROPPED:
		g_value_set_boolean (value,
				     thumber_pipe_get_cropped (THUMBER_PIPE (object)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
thumber_pipe_finalize (GObject *object)
{
	ThumberPipePrivate *priv;
	priv = THUMBER_PIPE_GET_PRIVATE (object);

	G_OBJECT_CLASS (thumber_pipe_parent_class)->finalize (object);
}

static void
thumber_pipe_class_init (ThumberPipeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize     = thumber_pipe_finalize;
	object_class->set_property = thumber_pipe_set_property;
	object_class->get_property = thumber_pipe_get_property;

	g_object_class_install_property (object_class,
					 PROP_STANDARD,
					 g_param_spec_boolean ("standard",
							       "Standard",
							       "Whether we create the standard normal/large thumbnails",
							       FALSE,
							       G_PARAM_READWRITE));
	
	g_object_class_install_property (object_class,
					 PROP_CROPPED,
					 g_param_spec_boolean ("cropped",
							       "Cropped",
							       "Whether we create the cropped thumbnail",
							       FALSE,
							       G_PARAM_READWRITE));
	
	g_type_class_add_private (object_class, sizeof (ThumberPipePrivate));
}

static void
thumber_pipe_init (ThumberPipe *object)
{
	ThumberPipePrivate *priv;
	priv = THUMBER_PIPE_GET_PRIVATE (object);
}

gboolean
thumber_pipe_run (ThumberPipe *pipe,
		  const gchar *uri,
		  GError     **error)
{
	ThumberPipePrivate *priv;
	GstBuffer          *buffer = NULL;
	gchar              *filename;
	gboolean            success = FALSE;
	GError             *lerror  = NULL;
	gint64              duration = 0;
	gint64              seek;
	GstFormat           format = GST_FORMAT_TIME;

	priv = THUMBER_PIPE_GET_PRIVATE (pipe);

	g_return_val_if_fail (pipe != NULL, FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	if (!initialize (pipe,
			 "dummy",
			 256,
			 &lerror)) {
		g_propagate_error (error, lerror);
		return FALSE;
	}

	filename = g_filename_from_uri (uri, NULL, NULL);

	g_object_set (priv->source, "location",
		      filename,
		      NULL);

	g_free (filename);

	gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
	if (!wait_for_state_change (pipe, GST_STATE_PAUSED, &lerror)) {
		g_propagate_error (error, lerror);
		success = FALSE;
		goto cleanup;
	}

	if (!gst_element_query_duration (priv->pipeline, &format, &duration))
		goto skip_seek;
	
	if (duration > 120 * GST_SECOND) {
		seek = 45 * GST_SECOND;
	} else {
		seek = duration/3;
	}
	
	if (gst_element_seek (priv->pipeline, 1.0, GST_FORMAT_TIME,
			      GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
			      GST_SEEK_TYPE_SET, seek,
			      GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE)) {
		/* Wait for the seek to finish */
		gst_element_get_state (priv->pipeline, NULL, NULL,
				       SEEK_TIMEOUT * GST_SECOND);
	}

skip_seek:

	gst_element_set_state (priv->pipeline, GST_STATE_PLAYING);
	if (!wait_for_image_buffer (pipe, uri, &lerror)) {
		g_propagate_error (error, lerror);
		success = FALSE;
		goto cleanup;
        }

	success = TRUE;

 cleanup:

	deinitialize (pipe);

	return success;
}

static void
deinitialize (ThumberPipe *pipe)
{
	ThumberPipePrivate *priv;

	priv = THUMBER_PIPE_GET_PRIVATE (pipe);

	gst_element_set_state (priv->pipeline, GST_STATE_NULL);
	/* State changes to NULL are synchronous */
	gst_object_unref (priv->pipeline);

	priv->pipeline  = NULL;

	priv->source    = NULL;
	priv->decodebin = NULL;

	priv->sinkbin      = NULL;
	priv->video_scaler = NULL;
	priv->video_filter = NULL;
	priv->video_sink   = NULL;
}

static gboolean
initialize (ThumberPipe *pipe, const gchar *mime, guint size, GError **error)
{
	ThumberPipePrivate *priv;
	GstPad             *videopad;
	GstCaps            *caps;

	priv = THUMBER_PIPE_GET_PRIVATE (pipe);

	priv->pipeline     = gst_pipeline_new ("source pipeline");

	if (!(priv->pipeline)) {
		g_set_error (error,
			     error_quark (),
			     INITIALIZATION_ERROR,
			     "Failed to create pipeline element");
		
		return FALSE;
	}
	
	priv->source       = gst_element_factory_make ("filesrc", "source");
	priv->decodebin    = gst_element_factory_make ("decodebin2", "decodebin2");

	if (!(priv->source &&
	      priv->decodebin)) {
		g_set_error (error,
			     error_quark (),
			     INITIALIZATION_ERROR,
			     "Failed to create source and decodebin elements");
		gst_object_unref (priv->pipeline);
		if (priv->source)
			gst_object_unref (priv->source);
		if (priv->decodebin)
			gst_object_unref (priv->decodebin);
		return FALSE;
	}

	gst_bin_add_many (GST_BIN(priv->pipeline),
			  priv->source,
			  priv->decodebin,
			  NULL);

	if (!gst_element_link_many(priv->source, priv->decodebin, NULL)) {
		g_set_error (error,
			     error_quark (),
			     INITIALIZATION_ERROR,
			     "Failed to link source and decoding components");
		gst_object_unref (priv->pipeline);
		return FALSE;
	}

	priv->sinkbin      = gst_bin_new("sink bin");
	priv->video_scaler = gst_element_factory_make("videoscale", "video_scaler");
	priv->video_filter = gst_element_factory_make("ffmpegcolorspace", "video_filter");
	priv->video_sink   = gst_element_factory_make("gdkpixbufsink", "video_sink");

	if (!(priv->sinkbin && 
	      priv->video_scaler && 
	      priv->video_filter && 
	      priv->video_sink)) {
		g_set_error (error,
			     error_quark (),
			     INITIALIZATION_ERROR,
			     "Failed to create scaler and sink elements");
		gst_object_unref (priv->pipeline);
		if (priv->sinkbin)
			gst_object_unref (priv->sinkbin);
		if (priv->video_scaler)
			gst_object_unref (priv->video_scaler);
		if (priv->video_filter)
			gst_object_unref (priv->video_filter);
		if (priv->video_sink)
			gst_object_unref (priv->video_sink);
		return FALSE;
	}

	gst_bin_add_many (GST_BIN(priv->sinkbin),
			  priv->video_scaler, 
			  priv->video_filter, 
			  priv->video_sink,
			  NULL);

	if (!gst_element_link_many(priv->video_scaler, priv->video_filter, NULL)) {
		g_set_error (error,
			     error_quark (),
			     INITIALIZATION_ERROR,
			     "Failed to link scaler and filter elements");
		gst_object_unref (priv->pipeline);
		gst_object_unref (priv->sinkbin);
		return FALSE;
	}

	caps = gst_caps_new_simple ("video/x-raw-rgb",
				    "width", G_TYPE_INT, size,
				    "height", G_TYPE_INT, size,
				    "bpp", G_TYPE_INT, 24,
				    "depth", G_TYPE_INT, 24,
				    NULL);

	if (!gst_element_link_filtered(priv->video_filter, priv->video_sink, caps)) {
		g_set_error (error,
			     error_quark (),
			     INITIALIZATION_ERROR,
			     "Failed to link filter and sink elements with caps");
		gst_object_unref (priv->pipeline);
		gst_object_unref (priv->sinkbin);
		gst_caps_unref (caps);
		return FALSE;
	}

	gst_caps_unref (caps);

	videopad = gst_element_get_pad (priv->video_scaler, "sink");
	gst_element_add_pad (priv->sinkbin, gst_ghost_pad_new ("sink", videopad));
	gst_object_unref (videopad);

	gst_bin_add (GST_BIN (priv->pipeline), priv->sinkbin);

	/* Connect signal for new pads */
	g_signal_connect (priv->decodebin, "new-decoded-pad", 
			  G_CALLBACK (newpad_callback), pipe);


	/* Connect signal for analysing new streams (we only care about video) */
	g_signal_connect (priv->decodebin, "autoplug-continue", 
			  G_CALLBACK (stream_continue_callback), pipe);


	return TRUE;
}

static void
newpad_callback (GstElement       *decodebin,
		 GstPad           *pad,
		 gboolean          last,
		 ThumberPipe      *pipe)
{
	ThumberPipePrivate *priv;

	GstCaps      *caps;
	GstStructure *str;
	GstPad       *videopad;

	g_return_if_fail (decodebin != NULL);
	g_return_if_fail (pad != NULL);
	g_return_if_fail (pipe != NULL);

	priv = THUMBER_PIPE_GET_PRIVATE (pipe);

	if (!pad) {
		return;
	}
	
	videopad = gst_element_get_static_pad (priv->sinkbin, "sink");

	if (!videopad) {
		return;
	}

	if (GST_PAD_IS_LINKED (videopad)) {
		gst_object_unref (videopad);
		return;
	}
	
	caps = gst_pad_get_caps (pad);
	str  = gst_caps_get_structure (caps, 0);

	if (!g_strrstr (gst_structure_get_name (str), "video")) {
		gst_object_unref (videopad);
		gst_caps_unref (caps);
		return;
	}
	gst_caps_unref (caps);
	gst_pad_link (pad, videopad);

	gst_object_unref (videopad);
}

static gboolean
stream_continue_callback (GstElement    *bin,
			  GstPad        *pad,
			  GstCaps       *caps,
			  ThumberPipe   *pipe)
{
	GstStructure *str;

	g_return_val_if_fail (bin != NULL, FALSE);
	g_return_val_if_fail (pad != NULL, FALSE);	
	g_return_val_if_fail (caps != NULL, FALSE);
	g_return_val_if_fail (pipe != NULL, FALSE);

	str  = gst_caps_get_structure (caps, 0);

	/* Because of some inconsistencies (audio/?? container 
	   containing video) we have a blacklist here. */

	if (strcasecmp (gst_structure_get_name (str), "audio/mpeg") == 0 ||
	    strcasecmp (gst_structure_get_name (str), "audio/amr") == 0||
	    strcasecmp (gst_structure_get_name (str), "audio/amr-wb") == 0 ) {
		return FALSE;
	}

	return TRUE;
}


static gboolean
wait_for_state_change (ThumberPipe *pipe,
		       GstState     state,
		       GError     **error)
{
	ThumberPipePrivate *priv;
	GstBus             *bus;
	gint64              timeout = PIPE_TIMEOUT * GST_SECOND;
	
	priv = THUMBER_PIPE_GET_PRIVATE (pipe);
	
	bus = gst_element_get_bus (priv->pipeline);

	while (TRUE) {
		GstMessage *message;
		GstElement *src;
		
		message = gst_bus_timed_pop (bus, timeout);
		
		if (!message) {
			g_set_error (error,
				     error_quark (),
				     RUNNING_ERROR,
				     "Pipeline timed out");
			goto error;
		}
		
		src = (GstElement*)GST_MESSAGE_SRC (message);
		
		switch (GST_MESSAGE_TYPE (message)) {
		case GST_MESSAGE_STATE_CHANGED: {
			GstState old, new, pending;
			
			if (src == priv->pipeline) {
				gst_message_parse_state_changed (message, &old, &new, &pending);
				if (new == state) {
					gst_message_unref (message);
					goto success;
				}
			}
			break;
		}
		case GST_MESSAGE_ERROR: {
			GError *lerror = NULL;

			gst_message_parse_error (message, &lerror, NULL);

			if (lerror != NULL) {
				g_propagate_error (error, lerror);
			} else {
				g_set_error (error,
					     error_quark (),
					     RUNNING_ERROR,
					     "Undefined error running the pipeline");
			}
			
			gst_message_unref (message);
			goto error;

			break;
		}
		case GST_MESSAGE_EOS: {
			g_set_error (error,
				     error_quark (),
				     RUNNING_ERROR,
				     "Reached end-of-file without proper content");

			gst_message_unref (message);
			goto error;

			break;
		}
		default:
			/* Nothing to do here */
			break;
		}
		
		gst_message_unref (message);
	}
	
	g_assert_not_reached ();
	
 error:
	gst_object_unref (bus);
	return FALSE;

 success:
	gst_object_unref (bus);
	return TRUE;
}

static gboolean
wait_for_image_buffer (ThumberPipe *pipe,
		       const gchar *uri,
		       GError     **error)
{
       ThumberPipePrivate *priv;
       GstBus             *bus;
       gint64              timeout = PIPE_TIMEOUT * GST_SECOND;

       priv = THUMBER_PIPE_GET_PRIVATE (pipe);

       bus = gst_element_get_bus (priv->pipeline);

       while (TRUE) {
               GstMessage *message;
               GstElement *src;

               message = gst_bus_timed_pop (bus, timeout);

               if (!message) {
                       g_set_error (error,
                                    error_quark (),
                                    RUNNING_ERROR,
                                    "Pipeline timed out");
                       goto error;
               }

               src = (GstElement*)GST_MESSAGE_SRC (message);

               switch (GST_MESSAGE_TYPE (message)) {
               case GST_MESSAGE_ELEMENT: {
                       const GstStructure *s = gst_message_get_structure (message);
                       const gchar *name = gst_structure_get_name (s);

		       if (strcmp (name, "preroll-pixbuf") == 0 ||
                           strcmp (name, "pixbuf") == 0 ) {
                               GdkPixbuf *pix;
			       
                               g_object_get (G_OBJECT (priv->video_sink), "last-pixbuf", &pix, NULL);
			       
                               if (!pix) {
                                       g_set_error (error,
                                                    error_quark (),
                                                    RUNNING_ERROR,
                                                    "Non-existing image buffer returned by pipeline");
				       gst_message_unref (message);
                                       goto error;
                               }
			       
			       if (check_for_valid_thumb (pix)) {
				       if (!create_thumbnails (uri,
							       pix,
							       priv->standard,
							       priv->cropped,
							       error)) {
					       g_object_unref (pix);
					       gst_message_unref (message);
					       goto error;
				       } else {
					       g_object_unref (pix);
					       gst_message_unref (message);
					       goto success;
				       }
			       } else {
                                       /* If not good, continue until we get better */
                                       g_object_unref (pix);
			       }
                       }
		       break;
               }
		       
               case GST_MESSAGE_ERROR: {
                       GError *lerror = NULL;

                       gst_message_parse_error (message, &lerror, NULL);

                       if (lerror != NULL) {
                               g_propagate_error (error, lerror);
                       } else {
                               g_set_error (error,
                                            error_quark (),
                                            RUNNING_ERROR,
                                            "Undefined error running the pipeline");
                       }
		       
		       gst_message_unref (message);
		       goto error;
                       break;
               }
               case GST_MESSAGE_EOS: {
                       g_set_error (error,
				    error_quark (),
                                 RUNNING_ERROR,
                                    "Reached end-of-file without proper content");
		       gst_message_unref (message);
		       goto error;
                       break;
               }
               default:
                       /* Nothing to do here */
                       break;
               }

               gst_message_unref (message);
       }

       g_assert_not_reached ();

 error:
       gst_object_unref (bus);
       return FALSE;

 success:
       gst_object_unref (bus);
       return TRUE;
}



static gchar *
compute_checksum (GChecksumType  checksum_type,
		  const guchar  *data,
		  gsize          length)
{
  GChecksum *checksum;
  gchar *retval;

  checksum = g_checksum_new (checksum_type);
  if (!checksum)
    return NULL;

  g_checksum_update (checksum, data, length);
  retval = g_strdup (g_checksum_get_string (checksum));
  g_checksum_free (checksum);

  return retval;
}

static GdkPixbuf*
crop_resize (GdkPixbuf *src, int width, int height) {
	int x = width, y = height;
	int a = gdk_pixbuf_get_width(src);
	int b = gdk_pixbuf_get_height(src);

	GdkPixbuf *dest;

	// This is the automagic cropper algorithm 
	// It is an optimized version of a system of equations
	// Basically it maximizes the final size while minimizing the scale

	int nx, ny;
	double na, nb;
	double offx = 0, offy = 0;
	double scax, scay;

	g_return_val_if_fail (src != NULL, NULL);

	na = a;
	nb = b;

	if(a < x && b < y) {
		//nx = a;
		//ny = b;
		g_object_ref(src);
		return src;
	} else {
		int u, v;

		nx = u = x;
		ny = v = y;

		if(a < x) {
			nx = a;
			u = a;
		}

		if(b < y) {
			ny = b;
		 	v = b;
		}

		if(a * y < b * x) {
			nb = (double)a * v / u;
			// Center
			offy = (double)(b - nb) / 2;
		} else {
			na = (double)b * u / v;
			// Center
			offx = (double)(a - na) / 2;
		}
	}

	// gdk_pixbuf_scale has crappy inputs
	scax = scay = (double)nx / na;

	offx = -offx * scax;
	offy = -offy * scay;

	dest = gdk_pixbuf_new (gdk_pixbuf_get_colorspace(src),
			       gdk_pixbuf_get_has_alpha(src), 
			       gdk_pixbuf_get_bits_per_sample(src), 
			       nx, ny);

	gdk_pixbuf_scale (src, dest, 0, 0, nx, ny, offx, offy, scax, scay,
			  GDK_INTERP_BILINEAR);

	return dest;
}

static gboolean
check_for_valid_thumb (GdkPixbuf   *pixbuf)
{
       guchar* pixels = gdk_pixbuf_get_pixels(pixbuf);
       int     rowstride = gdk_pixbuf_get_rowstride(pixbuf);
       int     height = gdk_pixbuf_get_height(pixbuf);
       int     samples = (rowstride * height);
       int     i;
       float   avg = 0.0f;
       float   variance = 0.0f;

       g_return_val_if_fail (pixbuf != NULL, FALSE);

       /* We calculate the variance of one element (bpp=24) */

       for (i = 0; i < samples; i=i+3) {
               avg += (float) pixels[i];
       }
       avg /= ((float) samples);

       /* Calculate the variance */
       for (i = 0; i < samples; i=i+3) {
               float tmp = ((float) pixels[i] - avg);
	       variance += tmp * tmp;
       }
       variance /= ((float) (samples - 1));

       return (variance > VALID_VARIANCE_THRESHOLD);
}

static gboolean
create_thumbnails (const gchar *uri,
		   GdkPixbuf *pixbuf,
		   gboolean standard,
		   gboolean cropped,
		   GError **error)
{
	GdkPixbuf *pic;
	gchar *png_name;
	gchar *jpg_name;
	gchar *filename;
	gchar *checksum;
	GError *lerror = NULL;

	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (pixbuf != NULL, FALSE);

	if (!pixbuf) {
		g_set_error (error,
			     error_quark (),
			     THUMBNAIL_ERROR,
			     "Failed to create thumbnail from buffer");
		return FALSE;
	}

	checksum = compute_checksum (G_CHECKSUM_MD5, (const guchar *)uri, strlen (uri));
	png_name = g_strdup_printf ("%s.png", checksum);
	jpg_name = g_strdup_printf ("%s.jpeg", checksum);

	g_free (checksum);

	if (standard) {
		filename = g_build_filename (g_get_home_dir (), ".thumbnails", "large", png_name, NULL);
		if (!gdk_pixbuf_save (pixbuf,
				      filename,
				      "jpeg",
				      &lerror,
				      NULL)) {
			g_propagate_error (error, lerror);
			
			g_free (png_name);
			g_free (jpg_name);
			return FALSE;
		}	
		g_free (filename);

		pic = gdk_pixbuf_scale_simple (pixbuf,
					       128,
					       128,
					       GDK_INTERP_BILINEAR);
		
		filename = g_build_filename (g_get_home_dir (), ".thumbnails", "normal", png_name, NULL);
		if (!gdk_pixbuf_save (pic,
				      filename,
				      "jpeg",
				      &lerror,
				      NULL)) {
			g_propagate_error (error, lerror);
			
			g_free (png_name);
			g_free (jpg_name);

			g_object_unref (pic);

			return FALSE;
		}
		g_object_unref (pic);
	}

	if (cropped) {
		pic = crop_resize (pixbuf, 124, 124);
		
		filename = g_build_filename (g_get_home_dir (), ".thumbnails", "cropped", jpg_name, NULL);
		if (!gdk_pixbuf_save (pic,
				      filename,
				      "jpeg",
				      &lerror,
				      NULL)) {
			g_propagate_error (error, lerror);
			
			g_free (png_name);
			g_free (jpg_name);

			g_object_unref (pic);

			return FALSE;
		}
		g_object_unref (pic);
		
		g_free (filename);
	}

	g_free (png_name);
	g_free (jpg_name);

	return TRUE;
}

