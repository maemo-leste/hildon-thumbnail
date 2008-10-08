/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Tracker - audio/video metadata extraction based on GStreamer
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

/*
 * TODO:
 * - cropped
 * - hardcoded supported
 * - checks, stability.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <glib.h>
#include <gst/gst.h>
#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "hildon-thumbnail-plugin.h"

#define GSTP_ERROR_DOMAIN	"HildonThumbnailerGStreamerVideoPlugin"
#define GSTP_ERROR		g_quark_from_static_string (GSTP_ERROR_DOMAIN)

#define HILDON_THUMBNAIL_OPTION_PREFIX "tEXt::Thumb::"
#define HILDON_THUMBNAIL_APPLICATION "hildon-thumbnail"
#define URI_OPTION HILDON_THUMBNAIL_OPTION_PREFIX "URI"
#define MTIME_OPTION HILDON_THUMBNAIL_OPTION_PREFIX "MTime"
#define SOFTWARE_OPTION "tEXt::Software"

static gchar *supported[] = { "video/mp4", "video/mpeg", NULL };
static gboolean do_cropped = TRUE;
static GFileMonitor *monitor = NULL;

typedef struct {
	const gchar    *uri;
	const gchar    *target;
	guint           size;

	guint           mtime;

	GMainLoop      *loop;

	GstElement     *pipeline;
	GstElement     *source;
	GstElement     *decodebin;

	GstElement     *bin;
	GstElement     *video_scaler;
	GstElement     *video_filter;
	GstElement     *video_sink;

	gboolean	has_audio;
	gboolean	has_video;

	gint		video_height;
	gint		video_width;
	gint		video_fps_n;
	gint		video_fps_d;
	gint		audio_channels;
	gint		audio_samplerate;
} VideoThumbnailer;


static gboolean callback_bus(GstBus *bus, GstMessage *message, VideoThumbnailer *thumber);

static gboolean 
save_thumb_file_meta (GdkPixbuf *pixbuf, const gchar *file, guint64 mtime, const gchar *uri, GError **error)
{
	gboolean ret;
	char mtime_str[64];

	const char *default_keys[] = {
	    URI_OPTION,
	    MTIME_OPTION,
	    SOFTWARE_OPTION,
	    NULL
	};

	const char *default_values[] = {
	    uri,
	    mtime_str,
	    HILDON_THUMBNAIL_APPLICATION "-" VERSION,
	    NULL
	};

	g_sprintf(mtime_str, "%lu", mtime);

	ret = gdk_pixbuf_savev (pixbuf, file, "png", 
				(char **) default_keys, 
				(char **) default_values, 
				error);

	return ret;
}

static gboolean
create_png(const gchar *target, unsigned char *data, guint width, guint height, guint bpp,
	   const gchar *uri, guint mtime)
{
	GdkPixbuf *pixbuf = NULL;
	GError *error = NULL;

	pixbuf = gdk_pixbuf_new_from_data(data,
			GDK_COLORSPACE_RGB, /* RGB-colorspace */
			FALSE, /* No alpha-channel */
			bpp/3, /* Bits per RGB-component */
			width, height, /* Dimensions */
			width*3, /* Number of bytes between lines (ie stride) */
			NULL, NULL); /* Callbacks */

	if(!save_thumb_file_meta(pixbuf, target, mtime, uri, &error))
	{
		g_warning("%s\n", error->message);
		g_error_free(error);
		g_object_unref(pixbuf);
		return FALSE;
	}
	
	g_object_unref(pixbuf);
	return TRUE;
}

static gboolean
callback_thumbnail (GstElement       *image_sink,
		    GstBuffer        *buffer,
		    GstPad           *pad, 
		    VideoThumbnailer *thumber)
{
	unsigned char *data_photo =
	    (unsigned char *) GST_BUFFER_DATA(buffer);

	/* Create a PNG of the data and check the status */
	if(!create_png(thumber->target, data_photo,
		       thumber->size, thumber->size,
		       24, thumber->uri, thumber->mtime)) {
		g_error ("Creation of thumbnail failed");
	}

	g_main_loop_quit (thumber->loop);

	return TRUE;
}

static void
callback_newpad (GstElement       *decodebin,
		 GstPad           *pad,
		 gboolean          last,
		 VideoThumbnailer *thumber)
{
	GstCaps      *caps;
	GstStructure *str;
	GstPad       *videopad;

	videopad = gst_element_get_pad (thumber->bin, "sink");
	if (GST_PAD_IS_LINKED (videopad)) {
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
}


static gboolean
callback_bus(GstBus           *bus,
	     GstMessage       *message, 
	     VideoThumbnailer *thumber)
{
	gchar       *message_str;
	GError      *error;
	GstState     old_state, new_state;
	gint64       duration = -1;
	gint64       position = -1;
	GstFormat    format;


	switch (GST_MESSAGE_TYPE(message)) {

	case GST_MESSAGE_ERROR:
		gst_message_parse_error(message, &error, &message_str);
		g_error("GStreamer error: %s\n", message_str);
		g_free(error);
		g_free(message_str);
		break;
	
	case GST_MESSAGE_WARNING:
		gst_message_parse_warning(message, &error, &message_str);
		g_warning("GStreamer warning: %s\n", message_str);
		g_free(error);
		g_free(message_str);
		break;

	case GST_MESSAGE_EOS:
		g_main_loop_quit (thumber->loop);
		break;

	case GST_MESSAGE_STATE_CHANGED:

		old_state = new_state = GST_STATE_NULL;

		if (GST_MESSAGE_SRC (message) != GST_OBJECT (thumber->decodebin)) {
			break;
		}

		gst_message_parse_state_changed (message, &old_state, &new_state, NULL);

		if (old_state == new_state) {
			break;
		}

		format = GST_FORMAT_TIME;
		gst_element_query_duration (thumber->pipeline, &format, &duration);
		
		if (duration != -1) {
			position = duration * 5 / 100;
		} else {
			position = 1 * GST_SECOND;
		}

		gst_element_query_duration (thumber->pipeline, &format, &duration);

		if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
			if (!gst_element_seek_simple (thumber->pipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH, position)) {
				g_error ("Seek failed");
			}
		}
		break;

	case GST_MESSAGE_APPLICATION:
	case GST_MESSAGE_TAG:
	default:
		/* unhandled message */
		break;
	}

	return TRUE;
}

static void
video_thumbnail_create (VideoThumbnailer *thumber, GError **error)
{
	GstBus            *bus;
	GstPad            *videopad;
	GstCaps           *caps;

	/* Resetting */
	thumber->loop         = NULL;
	thumber->source       = NULL;
	thumber->decodebin    = NULL;
	thumber->bin          = NULL;
	thumber->video_scaler = NULL;
	thumber->video_filter = NULL;
	thumber->video_sink   = NULL;

	/* Preparing the source, decodebin and pipeline */
	thumber->pipeline     = gst_pipeline_new("source pipeline");
	thumber->source       = gst_element_factory_make ("filesrc", "source");
	thumber->decodebin    = gst_element_factory_make ("decodebin", "decodebin");

	if (!(thumber->pipeline && thumber->source && thumber->decodebin)) {
		g_set_error (error, GSTP_ERROR, 0,
			     "Couldn't create pipeline elements");
		goto cleanup;
	}

	gst_bin_add_many (GST_BIN(thumber->pipeline),
			  thumber->source, thumber->decodebin,
			  NULL);

	bus = gst_pipeline_get_bus (GST_PIPELINE (thumber->pipeline));
	gst_bus_add_watch (bus, (GstBusFunc) callback_bus, thumber);
	gst_object_unref (bus);

	g_object_set (thumber->source, "location", 
		      g_filename_from_uri (thumber->uri, NULL, NULL), 
		      NULL);

	g_signal_connect (thumber->decodebin, "new-decoded-pad", 
			  G_CALLBACK (callback_newpad), thumber);

	if (!gst_element_link_many(thumber->source, thumber->decodebin, NULL)) {
		g_set_error (error, GSTP_ERROR, 0,
			"Failed to link source pipeline elements");
		goto cleanup;
	}

	/* Preparing the sink bin and elements */
	thumber->bin          = gst_bin_new("sink bin");
	thumber->video_scaler = gst_element_factory_make("videoscale", "video_scaler");
	thumber->video_filter = gst_element_factory_make("ffmpegcolorspace", "video_filter");
	thumber->video_sink   = gst_element_factory_make("fakesink", "video_sink");

	if (!(thumber->bin && thumber->video_scaler && thumber->video_filter && thumber->video_sink)) {
		g_set_error (error, GSTP_ERROR, 0,
			"Couldn't create sink bin elements");
		goto cleanup;
	}

	gst_bin_add_many (GST_BIN(thumber->bin),
			  thumber->video_scaler, 
			  thumber->video_filter, 
			  thumber->video_sink,
			  NULL);

	if (!gst_element_link_many(thumber->video_scaler, thumber->video_filter, NULL)) {
		g_set_error (error, GSTP_ERROR, 0,
			"Failed to link sink bin elements");
		goto cleanup;
	}

	caps = gst_caps_new_simple ("video/x-raw-rgb",
				    "width", G_TYPE_INT, thumber->size,
				    "height", G_TYPE_INT, thumber->size,
				    "bpp", G_TYPE_INT, 24,
				    "depth", G_TYPE_INT, 24,
				    NULL);

	if (!gst_element_link_filtered(thumber->video_filter, thumber->video_sink, caps)) {
		g_set_error (error, GSTP_ERROR, 0,
			"Failed to link sink bin elements");
		goto cleanup;
	}

	gst_caps_unref (caps);

	g_object_set (thumber->video_sink, "signal-handoffs", TRUE, NULL);

	g_signal_connect (thumber->video_sink, "preroll-handoff",
			  G_CALLBACK(callback_thumbnail), thumber);

	videopad = gst_element_get_pad (thumber->video_scaler, "sink");
	gst_element_add_pad (thumber->bin, gst_ghost_pad_new ("sink", videopad));
	gst_object_unref (videopad);
	gst_bin_add (GST_BIN (thumber->pipeline), thumber->bin);

	/* Run */
	thumber->loop = g_main_loop_new (NULL, FALSE);
	gst_element_set_state (thumber->pipeline, GST_STATE_PAUSED);
	g_main_loop_run (thumber->loop);

	cleanup:

	if (thumber->pipeline) {
		gst_element_set_state (thumber->pipeline, GST_STATE_NULL);

		/* This should free all the elements in the pipeline FIXME 
		 * Check that this is the case 
		 *
		 * Review by Philip: I'm assuming here that this is a correct
		 * assumption ;-) - I have not checked myself - */

		gst_object_unref (thumber->pipeline);
	} else {
		if (thumber->source)
			gst_object_unref (thumber->source);
		if (thumber->decodebin)
			gst_object_unref (thumber->decodebin);
		if (thumber->bin)
			gst_object_unref (thumber->bin);
		if (thumber->video_scaler)
			gst_object_unref (thumber->video_scaler);
		if (thumber->video_filter)
			gst_object_unref (thumber->video_filter);
		if (thumber->video_sink)
			gst_object_unref (thumber->video_sink);
	}

	if (thumber->loop)
		g_main_loop_unref (thumber->loop);
}

const gchar** 
hildon_thumbnail_plugin_supported (void)
{
	GstFormatDefinition def;
	GstIterator *iter;

/* 	iter = gst_format_iterate_definitions(); */
	
/* 	while (gst_iterator_next(iter, (gpointer) &def) == 1) { */
/* 		g_debug ("Got: %s", def.nick); */
/* 	} */

	/* FIXME: Returning hardcoded values for now */

	return (const gchar**) supported;
}

void
hildon_thumbnail_plugin_create (GStrv uris, GError **error)
{
	VideoThumbnailer *thumber;
	gchar *large    = NULL;
	gchar *normal   = NULL;
	gchar *cropped  = NULL;
	guint i         = 0;
	GString *errors = NULL;

	while (uris[i] != NULL) {
		GError *nerror = NULL;

		hildon_thumbnail_util_get_thumb_paths (uris[i], &large, &normal, 
						       &cropped);

		/* Create the thumbnailer struct */
		thumber = g_slice_new0 (VideoThumbnailer);

		thumber->has_audio    = thumber->has_video = FALSE;
		thumber->video_fps_n  = thumber->video_fps_d = -1;
		thumber->video_height = thumber->video_width = -1;
		thumber->uri          = uris[i];
		thumber->target       = normal;
		thumber->size         = 128;

		video_thumbnail_create (thumber, &nerror);

		if (nerror)
			goto nerror_handler;

		thumber->target       = large;
		thumber->size         = 256;

		video_thumbnail_create (thumber, &nerror);

		nerror_handler:

		g_slice_free (VideoThumbnailer, thumber);

		if (nerror) {
			if (!errors)
				errors = g_string_new ("");
			g_string_append_printf (errors, "[`%s': %s] ", 
						uris[i], nerror->message);
			g_error_free (nerror);
			nerror = NULL;
		}

		g_free (large);
		g_free (normal);
		g_free (cropped);

		i++;
	}

	if (errors) {
		g_set_error (error, GSTP_ERROR, 0,
			     errors->str);
		g_string_free (errors, TRUE);
	}
}


void 
hildon_thumbnail_plugin_stop (void)
{
	if (monitor)
		g_object_unref (monitor);
}

static void
reload_config (const gchar *config)
{
	GKeyFile *keyfile;

	keyfile = g_key_file_new ();

	if (!g_key_file_load_from_file (keyfile, config, G_KEY_FILE_NONE, NULL)) {
		do_cropped = TRUE;
		return;
	}

	do_cropped = g_key_file_get_boolean (keyfile, "Hildon Thumbnailer", "DoCropping", NULL);
	g_key_file_free (keyfile);
}

static void 
on_file_changed (GFileMonitor *monitor, GFile *file, GFile *other_file, GFileMonitorEvent event_type, gpointer user_data)
{
	if (event_type == G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT || event_type == G_FILE_MONITOR_EVENT_CREATED) {
		gchar *config = g_file_get_path (file);
		reload_config (config);
		g_free (config);
	}
}

void 
hildon_thumbnail_plugin_init (gboolean *cropping, register_func func, gpointer thumbnailer, GModule *module, GError **error)
{
	gchar *config = g_build_filename (g_get_user_config_dir (), "hildon-thumbnailer", "gstreamer-video-plugin.conf", NULL);
	GFile *file = g_file_new_for_path (config);
	guint i = 0;
	const gchar **supported;

	g_type_init ();

	gst_init (NULL, NULL);

	monitor =  g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);

	g_signal_connect (G_OBJECT (monitor), "changed", 
			  G_CALLBACK (on_file_changed), NULL);

	reload_config (config);

	*cropping = do_cropped;

	if (func) {
		supported = hildon_thumbnail_plugin_supported ();
		if (supported) {
			while (supported[i] != NULL) {
				func (thumbnailer, supported[i], module);
				i++;
			}
		}
	}

	g_free (config);

}