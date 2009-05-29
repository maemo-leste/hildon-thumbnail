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

static void           _thumber_pipe_newpad_callback    (GstElement       *decodebin,
							GstPad           *pad,
							gboolean          last,
							ThumberPipe      *pipe);

static gboolean       _thumber_pipe_thumbnail_callback (GstElement       *image_sink,
							GstBuffer        *buffer,
							GstPad           *pad, 
							ThumberPipe      *pipe);

static gboolean       _thumber_pipe_poll_for_state_change (ThumberPipe *pipe,
							   GstState     state,
							   GError     **error);


static gboolean       create_thumbnails                (const gchar *uri,
							guchar      *buffer,
							gboolean     standard,
							gboolean     cropped,
							GError     **error);

static gboolean       thumber_pipe_initialize          (ThumberPipe *pipe,
							const gchar *mime,
							guint size,
							GError **error);
static void           thumber_pipe_deinitialize        (ThumberPipe *pipe);

G_DEFINE_TYPE (ThumberPipe, thumber_pipe, G_TYPE_OBJECT)

typedef struct {
	GstElement     *pipeline;

	GstElement     *source;
	GstElement     *decodebin;

	GstElement     *sinkbin;
	GstElement     *video_scaler;
	GstElement     *video_filter;
	GstElement     *video_sink;

	gchar          *uri;
	gboolean        success;
	GError         *error;

	gboolean        standard;
	gboolean        cropped;
} ThumberPipePrivate;

enum {
	PROP_0,
	PROP_STANDARD,
	PROP_CROPPED
};

#define THUMBER_PIPE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TYPE_THUMBER_PIPE, ThumberPipePrivate))

enum {
	NO_ERROR,
	INITIALIZATION_ERROR,
	RUNNING_ERROR,
	THUMBNAIL_ERROR
};

GQuark
thumber_pipe_error_quark (void)
{
	return g_quark_from_static_string (THUMBER_PIPE_ERROR_DOMAIN);
}

ThumberPipe *
thumber_pipe_new ()
{
	return g_object_new (TYPE_THUMBER_PIPE, NULL);
}

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
	gchar              *filename;
	gboolean            success = FALSE;
	GError             *lerror  = NULL;

	priv = THUMBER_PIPE_GET_PRIVATE (pipe);

	priv->uri = g_strdup (uri);
	priv->success = FALSE;

	if (!thumber_pipe_initialize (pipe,
				      "dummy",
				      256,
				      &lerror)) {
		g_propagate_error (error, lerror);
		g_free (priv->uri);
		return FALSE;
	}

	gst_element_set_state (priv->pipeline, GST_STATE_READY);

	if (!_thumber_pipe_poll_for_state_change (pipe, GST_STATE_READY, &lerror)) {
		g_propagate_error (error, lerror);
		g_free (priv->uri);
		return FALSE;
	}

	filename = g_filename_from_uri (uri, NULL, NULL);

	g_object_set (priv->source, "location",
		      filename,
		      NULL);

	g_free (filename);

	gst_element_set_state (priv->pipeline, GST_STATE_PAUSED);
	if (!_thumber_pipe_poll_for_state_change (pipe, GST_STATE_PAUSED, &lerror)) {
		g_propagate_error (error, lerror);
		g_free (priv->uri);
		return FALSE;
	}

	g_signal_connect (priv->video_sink, "preroll-handoff",
			  G_CALLBACK(_thumber_pipe_thumbnail_callback), pipe);


	gst_element_seek (priv->pipeline, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH,
			  GST_SEEK_TYPE_SET, 3 * GST_SECOND,
			  GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);

	gst_element_get_state (priv->pipeline, NULL, NULL, 100 * GST_MSECOND);

	success = priv->success;

	if (!success) {
		if (priv->error != NULL) {
			g_propagate_error (error, priv->error);
		} else {
			g_set_error (error,
				     thumber_pipe_error_quark (),
				     THUMBNAIL_ERROR,
				     "Thumbnail creation failed.");
		}
		g_free (priv->uri);
		return FALSE;
	}

	thumber_pipe_deinitialize (pipe);

	g_free (priv->uri);
	priv->uri = NULL;

	return success;
}

static void
thumber_pipe_deinitialize (ThumberPipe *pipe)
{
	ThumberPipePrivate *priv;

	priv = THUMBER_PIPE_GET_PRIVATE (pipe);

	gst_element_set_state (priv->pipeline, GST_STATE_NULL);
	gst_element_get_state (priv->pipeline, NULL, NULL, -1);

	g_object_unref (priv->pipeline);

	priv->pipeline  = NULL;

	priv->source    = NULL;
	priv->decodebin = NULL;

	priv->sinkbin      = NULL;
	priv->video_scaler = NULL;
	priv->video_filter = NULL;
	priv->video_sink   = NULL;
}

static gboolean
thumber_pipe_initialize (ThumberPipe *pipe, const gchar *mime, guint size, GError **error)
{
	ThumberPipePrivate *priv;
	GstPad             *videopad;
	GstCaps            *caps;

	priv = THUMBER_PIPE_GET_PRIVATE (pipe);

	priv->pipeline     = gst_pipeline_new ("source pipeline");

	if (!(priv->pipeline)) {
		g_set_error (error,
			     thumber_pipe_error_quark (),
			     INITIALIZATION_ERROR,
			     "Failed to create pipeline element");
		
		return FALSE;
	}
	
	priv->source       = gst_element_factory_make ("filesrc", "source");
	priv->decodebin    = gst_element_factory_make ("decodebin2", "decodebin2");

	if (!(priv->source &&
	      priv->decodebin)) {
		g_set_error (error,
			     thumber_pipe_error_quark (),
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
			     thumber_pipe_error_quark (),
			     INITIALIZATION_ERROR,
			     "Failed to link source and decoding components");
		gst_object_unref (priv->pipeline);
		return FALSE;
	}

	priv->sinkbin      = gst_bin_new("sink bin");
	priv->video_scaler = gst_element_factory_make("videoscale", "video_scaler");
	priv->video_filter = gst_element_factory_make("ffmpegcolorspace", "video_filter");
	priv->video_sink   = gst_element_factory_make("fakesink", "video_sink");

	if (!(priv->sinkbin && 
	      priv->video_scaler && 
	      priv->video_filter && 
	      priv->video_sink)) {
		g_set_error (error,
			     thumber_pipe_error_quark (),
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
			     thumber_pipe_error_quark (),
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
			     thumber_pipe_error_quark (),
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

	g_object_set (priv->video_sink, "signal-handoffs", TRUE, NULL);

	/* Connect signal for new pads */
	g_signal_connect (priv->decodebin, "new-decoded-pad", 
			  G_CALLBACK (_thumber_pipe_newpad_callback), pipe);

	return TRUE;
}



static void
_thumber_pipe_newpad_callback (GstElement       *decodebin,
			       GstPad           *pad,
			       gboolean          last,
			       ThumberPipe      *pipe)
{
	ThumberPipePrivate *priv;

	GstCaps      *caps;
	GstStructure *str;
	GstPad       *videopad;

	priv = THUMBER_PIPE_GET_PRIVATE (pipe);
	
	videopad = gst_element_get_static_pad (priv->sinkbin, "sink");
	if (!videopad || GST_PAD_IS_LINKED (videopad)) {
		g_object_unref (videopad);
		return;
	}
	
	caps = gst_pad_get_caps (pad);
	str  = gst_caps_get_structure (caps, 0);

	if (!g_strrstr (gst_structure_get_name (str), "video")) {
		g_object_unref (videopad);
		gst_caps_unref (caps);
		return;
	}
	gst_caps_unref (caps);
	gst_pad_link (pad, videopad);

	gst_object_unref (videopad);

}

static gboolean
_thumber_pipe_thumbnail_callback (GstElement       *image_sink,
				  GstBuffer        *buffer,
				  GstPad           *pad, 
				  ThumberPipe      *pipe)
{
	ThumberPipePrivate *priv;

	priv = THUMBER_PIPE_GET_PRIVATE (pipe);

	if (!create_thumbnails (priv->uri,
				GST_BUFFER_DATA (buffer),
				priv->standard,
				priv->cropped,
				&(priv->error))) {
		priv->success = FALSE;
	} else {
		priv->success = TRUE;
	}

	return TRUE;
}

static gboolean
_thumber_pipe_poll_for_state_change (ThumberPipe *pipe,
				     GstState     state,
				     GError     **error)
{
	ThumberPipePrivate *priv;
	GstBus             *bus;
	gchar              *error_message;
	gint64              timeout = 5 * GST_SECOND;
	
	priv = THUMBER_PIPE_GET_PRIVATE (pipe);
	
	bus = gst_element_get_bus (priv->pipeline);

	while (TRUE) {
		GstMessage *message;
		GstElement *src;
		
		message = gst_bus_timed_pop (bus, timeout);
		
		if (!message) {
			g_set_error (error,
				     thumber_pipe_error_quark (),
				     RUNNING_ERROR,
				     "Pipeline timed out");
			return FALSE;
		}
		
		src = (GstElement*)GST_MESSAGE_SRC (message);
		
		switch (GST_MESSAGE_TYPE (message)) {
		case GST_MESSAGE_STATE_CHANGED: {
			GstState old, new, pending;
			
			if (src == priv->pipeline) {
				gst_message_parse_state_changed (message, &old, &new, &pending);
				if (new == state) {
					gst_message_unref (message);
					return TRUE;
				}
			}
			break;
		}
		case GST_MESSAGE_ERROR: {
			GError *lerror = NULL;
			gst_message_parse_error (message, &lerror, &error_message);
			gst_message_unref (message);
			g_free (error_message);

			if (lerror != NULL) {
				g_propagate_error (error, lerror);
			} else {
				g_set_error (error,
					     thumber_pipe_error_quark (),
					     RUNNING_ERROR,
					     "Undefined error running the pipeline");
			}

			return FALSE;
			break;
		}
		case GST_MESSAGE_EOS: {
			gst_message_unref (message);

			g_set_error (error,
				     thumber_pipe_error_quark (),
				     RUNNING_ERROR,
				     "Reached end-of-file without proper content");
			return FALSE;
			break;
		}
		default:
			/* Nothing to do here */
			break;
		}
		
		gst_message_unref (message);
	}
	
	g_assert_not_reached ();
	
	return FALSE;
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
create_thumbnails (const gchar *uri, guchar *buffer, gboolean standard, gboolean cropped, GError **error)
{
	GdkPixbuf *pixbuf;
	GdkPixbuf *pic;
	gchar *png_name;
	gchar *jpg_name;
	gchar *filename;
	gchar *checksum;
	GError *lerror = NULL;

	pixbuf = gdk_pixbuf_new_from_data ((gchar *)buffer,
					   GDK_COLORSPACE_RGB,
					   FALSE, 8, 256, 256,
					   GST_ROUND_UP_4 (256 * 3),
					   NULL, NULL);

	if(!pixbuf) {
		g_set_error (error,
			     thumber_pipe_error_quark (),
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
			g_object_unref (pixbuf);
			
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
			g_object_unref (pixbuf);
			
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
			g_object_unref (pixbuf);
			
			g_free (png_name);
			g_free (jpg_name);

			g_object_unref (pic);

			return FALSE;
		}
		g_object_unref (pic);
		
		g_free (filename);
	}

	g_object_unref (pixbuf);
	
	g_free (png_name);
	g_free (jpg_name);

	return TRUE;
}
