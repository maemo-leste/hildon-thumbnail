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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define DEFAULT_QUIT_TIMEOUT 30

#include "gst-thumb-thumber.h"

#include <glib.h>
#include <gio/gio.h>
#include <gst/gst.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib-lowlevel.h>

#ifdef HAVE_PLAYBACK
#include <libplayback/playback.h>
#endif

#include "gst-video-thumbnailer-marshal.h"
#include "gst-thumb-main.h"
#include "gst-thumb-pipe.h"

enum ThumberState {
	THUMBER_STATE_NULL,
	THUMBER_STATE_PENDING_STOP,
	THUMBER_STATE_STOPPED,
	THUMBER_STATE_PAUSED,
	THUMBER_STATE_WORKING,
};
typedef enum ThumberState ThumberState;


void thumber_dbus_method_create (Thumber *object, gchar *uri, gchar *mime_hint, DBusGMethodInvocation *context);
void thumber_dbus_method_create_many (Thumber *object, GStrv uris, gchar *mime_hint, DBusGMethodInvocation *context);

#define gst_video_thumbnailer_create thumber_dbus_method_create
#define gst_video_thumbnailer_create_many thumber_dbus_method_create_many

#include "gst-video-thumbnailer-glue.h"

static gboolean thumber_process_func (gpointer data);
static void     thumber_set_state (Thumber *thumber, ThumberState state);

static void     request_resources (Thumber *thumber);
static void     release_resources (Thumber *thumber);

G_DEFINE_TYPE (Thumber, thumber, G_TYPE_OBJECT)

typedef struct FileInfo FileInfo;
typedef struct TaskInfo TaskInfo;

typedef struct {
	/* Properties */
	gboolean         standard;
	gboolean         cropped;

	guint            idle_id;
	guint            quit_timeout_id;
	gint             quit_timeout;

	TaskInfo        *current_task;

	GQueue          *task_queue;
	GQueue          *file_queue;

	ThumberPipe     *pipe;

	ThumberState     state;
#ifdef HAVE_PLAYBACK
	pb_playback_t    *playback;
#endif

} ThumberPrivate;

struct FileInfo {
	gchar *uri;
};

struct TaskInfo {
	guint   id;
	gchar  *mime;
	GSList *files;
};

enum {
	STARTED_SIGNAL,
	FINISHED_SIGNAL,
	READY_SIGNAL,
	ERROR_SIGNAL,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

FileInfo *
file_info_new (const gchar *uri)
{
	FileInfo *info;

	info            = g_slice_new0 (FileInfo);
	info->uri       = g_strdup (uri);
	return info;
}

void
file_info_free (FileInfo *info)
{
	if (info->uri) {
		g_free (info->uri);
	}

	g_slice_free (FileInfo, info);
}

TaskInfo *
task_info_new (const gchar *mime)
{
	TaskInfo     *info;
	static guint  id_counter = 0;

	info = g_slice_new0 (TaskInfo);
	info->id = ++id_counter;
	info->mime  = g_strdup(mime);
	info->files = NULL;

	return info;
}

void
task_info_add_uri (TaskInfo *info, gchar *uri)
{
	FileInfo *file;
	file = file_info_new (uri);
	info->files = g_slist_prepend (info->files, file);
}

void
task_info_add_uris (TaskInfo *info, GStrv uris)
{
	guint i;

	for (i=0;uris[i];i++) {
		task_info_add_uri (info, uris[i]);
	}
}

void
task_info_free (TaskInfo *info, gboolean free_content)
{
	if (info->mime) {
		g_free (info->mime);
	}

	if (free_content) {
		g_slist_foreach (info->files,
				 (GFunc)file_info_free,
				 NULL);
	}

	g_slist_free (info->files);

	g_slice_free (TaskInfo, info);
}

enum {
	PROP_0,
	PROP_STANDARD,
	PROP_CROPPED,
	PROP_TIMEOUT
};

#define THUMBER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj),TYPE_THUMBER, ThumberPrivate))

Thumber *
thumber_new ()
{
	return g_object_new (TYPE_THUMBER, NULL);
}

static gboolean
thumber_get_standard (Thumber *thumber)
{
	ThumberPrivate *priv;

	priv = THUMBER_GET_PRIVATE (thumber);
	return priv->standard;
}

static void
thumber_set_standard (Thumber *thumber, gboolean standard)
{
	ThumberPrivate *priv;

	priv = THUMBER_GET_PRIVATE (thumber);
	priv->standard = standard;
}

static gboolean
thumber_get_cropped (Thumber *thumber)
{
	ThumberPrivate *priv;

	priv = THUMBER_GET_PRIVATE (thumber);
	return priv->cropped;
}

static void
thumber_set_cropped (Thumber *thumber, gboolean cropped)
{
	ThumberPrivate *priv;

	priv = THUMBER_GET_PRIVATE (thumber);
	priv->cropped = cropped;
}

static gint
thumber_get_timeout (Thumber *thumber)
{
	ThumberPrivate *priv;

	priv = THUMBER_GET_PRIVATE (thumber);
	return priv->quit_timeout;
}

static void
thumber_set_timeout (Thumber *thumber, gint timeout)
{
	ThumberPrivate *priv;

	priv = THUMBER_GET_PRIVATE (thumber);
	priv->quit_timeout = timeout;
}

static void
thumber_set_property (GObject      *object,
		      guint         prop_id,
		      const GValue *value,
		      GParamSpec   *pspec)
{
	switch (prop_id) {
	case PROP_STANDARD:
		thumber_set_standard (THUMBER (object), g_value_get_boolean (value));
		break;
	case PROP_CROPPED:
		thumber_set_cropped (THUMBER (object), g_value_get_boolean (value));
		break;
	case PROP_TIMEOUT:
		thumber_set_timeout (THUMBER (object), g_value_get_int (value));
		break;		
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}


static void
thumber_get_property (GObject    *object,
		      guint       prop_id,
		      GValue     *value,
		      GParamSpec *pspec)
{
	ThumberPrivate *priv;

	priv = THUMBER_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_STANDARD:
		g_value_set_boolean (value,
				     thumber_get_standard (THUMBER (object)));
		break;
	case PROP_CROPPED:
		g_value_set_boolean (value,
				     thumber_get_cropped (THUMBER (object)));
		break;
	case PROP_TIMEOUT:
		g_value_set_int (value,
				 thumber_get_timeout (THUMBER (object)));
		break;		
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
thumber_finalize (GObject *object)
{
	ThumberPrivate *priv;

	priv = THUMBER_GET_PRIVATE (object);

#ifdef HAVE_PLAYBACK
	pb_playback_destroy (priv->playback); 
	priv->playback = NULL;
#endif
	if (priv->pipe) {
		g_object_unref (priv->pipe);
		priv->pipe = NULL;
	}

	g_queue_foreach (priv->task_queue, (GFunc) task_info_free, (gpointer)TRUE);
	g_queue_free (priv->task_queue);

	g_queue_foreach (priv->file_queue, (GFunc) file_info_free, NULL);
	g_queue_free (priv->file_queue);

	if (priv->idle_id) {
		g_source_remove (priv->idle_id);
	}

	gst_deinit ();

	G_OBJECT_CLASS (thumber_parent_class)->finalize (object);
}

static void
thumber_class_init (ThumberClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize     = thumber_finalize;
	object_class->set_property = thumber_set_property;
	object_class->get_property = thumber_get_property;

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

       g_object_class_install_property (object_class,
					PROP_TIMEOUT,
					g_param_spec_int ("timeout",
							  "Timeout",
							  "Time after which we quit if nothing to do",
							  -1,
							  3600,
							  DEFAULT_QUIT_TIMEOUT,
							  G_PARAM_READWRITE));


	signals[READY_SIGNAL] =
		g_signal_new ("ready",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ThumberClass, ready),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_STRING);

	signals[STARTED_SIGNAL] =
		g_signal_new ("started",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ThumberClass, started),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_UINT);

	signals[FINISHED_SIGNAL] =
		g_signal_new ("finished",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ThumberClass, finished),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__UINT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_UINT);
	
	signals[ERROR_SIGNAL] =
		g_signal_new ("error",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ThumberClass, error),
			      NULL, NULL,
			      gst_video_thumbnailer_marshal_VOID__STRING_INT_STRING,
			      G_TYPE_NONE,
			      3,
			      G_TYPE_STRING,
			      G_TYPE_INT,
			      G_TYPE_STRING);

	g_type_class_add_private (object_class, sizeof (ThumberPrivate));
}

static void
thumber_init (Thumber *object)
{
	ThumberPrivate *priv;
	priv = THUMBER_GET_PRIVATE (object);

	gst_init (NULL, NULL);

	priv->task_queue = g_queue_new ();
	priv->file_queue = g_queue_new ();

	priv->idle_id    = 0;
	priv->quit_timeout_id = 0;

	priv->current_task = NULL;

	priv->pipe       = NULL;

	priv->state = THUMBER_STATE_NULL;
}

#ifdef HAVE_PLAYBACK
static void
libplayback_state_hint_handler (pb_playback_t *pb,
                                const int allowed_state[],
                                void *data)
{
	Thumber        *thumber;
	ThumberPrivate *priv;
	thumber = data;
	priv = THUMBER_GET_PRIVATE (thumber);

	switch (priv->state) {
	case THUMBER_STATE_PENDING_STOP:
		if (allowed_state[PB_STATE_STOP]) {
			release_resources (thumber);
		}
		break;
	case THUMBER_STATE_PAUSED:
		if (allowed_state[PB_STATE_PLAY]) {
			request_resources (thumber);
		}
		break;		
	default:
		/* Nothing */
		break;

	}
}
#endif

#ifdef HAVE_PLAYBACK
static void
libplayback_state_request_handler (pb_playback_t  *pb,
                                  enum pb_state_e  req_state,
                                  pb_req_t        *req,
                                  void            *data)
{
	Thumber        *thumber;
	ThumberPrivate *priv;
	thumber = data;
	priv = THUMBER_GET_PRIVATE (thumber);

	switch (req_state) {
		case PB_STATE_PLAY:
			if (priv->state == THUMBER_STATE_PAUSED) {
				thumber_set_state (thumber, THUMBER_STATE_WORKING);
			}

			break;
		case PB_STATE_STOP:
			if (priv->state == THUMBER_STATE_WORKING) {
				thumber_set_state (thumber, THUMBER_STATE_PAUSED);
			}

			break;
		default:
			break;
	}

	pb_playback_req_completed(pb, req);
}
#endif

#ifdef HAVE_PLAYBACK
static void
libplayback_state_reply (pb_playback_t   *pb,
			 enum pb_state_e  granted_state,
			 const char      *reason,
			 pb_req_t        *req,
			 void            *data)
{
	Thumber        *thumber;
	ThumberPrivate *priv;
	thumber = data;
	priv = THUMBER_GET_PRIVATE (thumber);

	switch (granted_state) {
		case PB_STATE_PLAY:
			if (priv->state == THUMBER_STATE_PAUSED) {
				thumber_set_state (thumber, THUMBER_STATE_WORKING);
			}

			break;
		case PB_STATE_STOP:
			if (priv->state == THUMBER_STATE_PENDING_STOP) {
				thumber_set_state (thumber, THUMBER_STATE_STOPPED);
			}

			break;
		default:
			break;
	}

	pb_playback_req_completed (pb, req);
}
#endif

static void
request_resources (Thumber *thumber)
{
	ThumberPrivate *priv;
	priv = THUMBER_GET_PRIVATE (thumber);

	if (priv->state != THUMBER_STATE_WORKING) {
#ifdef HAVE_PLAYBACK
		pb_playback_req_state (priv->playback,
				       PB_STATE_PLAY,
				       libplayback_state_reply,
				       thumber);
		thumber_set_state (thumber, THUMBER_STATE_PAUSED);
#else	
		thumber_set_state (thumber, THUMBER_STATE_WORKING);
#endif
	}
}

static void
release_resources (Thumber *thumber)
{
	ThumberPrivate *priv;
	priv = THUMBER_GET_PRIVATE (thumber);

	if (priv->state != THUMBER_STATE_STOPPED) {
#ifdef HAVE_PLAYBACK
		pb_playback_req_state (priv->playback,
				       PB_STATE_STOP,
				       libplayback_state_reply,
				       thumber);
		thumber_set_state (thumber, THUMBER_STATE_PENDING_STOP);
#else	
		thumber_set_state (thumber, THUMBER_STATE_STOPPED);
#endif
	}
}


gboolean
thumber_dbus_register(Thumber *thumber,
		      const gchar *bus_name,
		      const gchar *bus_path,
		      GError **error)
{
	GError          *lerror     = NULL;
	DBusGConnection *connection = NULL;
	DBusConnection  *conn       = NULL;
	DBusGProxy      *proxy      = NULL;	
	guint            result;
	ThumberPrivate  *priv;
	priv = THUMBER_GET_PRIVATE (thumber);

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &lerror);

	if (lerror) {
		g_printerr ("Problem with dbus, %s\n", lerror->message);
		g_error_free (lerror);
		return FALSE;
	}

	proxy = dbus_g_proxy_new_for_name (connection, 
					   DBUS_SERVICE_DBUS,
					   DBUS_PATH_DBUS,
					   DBUS_INTERFACE_DBUS);

	org_freedesktop_DBus_request_name (proxy, bus_name,
					   DBUS_NAME_FLAG_DO_NOT_QUEUE,
					   &result, &lerror);

	g_object_unref (proxy);

	if (lerror) {
		g_printerr ("Problem requesting name, %s\n", lerror->message);
		g_error_free (lerror);
		return FALSE;
	}

	dbus_g_object_type_install_info (G_OBJECT_TYPE (thumber),
					 &dbus_glib_gst_video_thumbnailer_object_info);

	dbus_g_connection_register_g_object (connection, 
					     bus_path, 
					     G_OBJECT (thumber));

#ifdef HAVE_PLAYBACK
	/* playback control */
	conn = dbus_g_connection_get_connection (connection);

	priv->playback = pb_playback_new_2 (conn,
					    PB_CLASS_BACKGROUND,
					    PB_FLAG_VIDEO,
					    PB_STATE_STOP,
					    libplayback_state_request_handler,
					    thumber);

	pb_playback_set_state_hint (priv->playback,
				    libplayback_state_hint_handler,
				    thumber);
#endif



	return TRUE;
}

void
thumber_start (Thumber *thumber)
{
	thumber_set_state (thumber, THUMBER_STATE_STOPPED);
}

static void
add_file (Thumber *thumber,
	  FileInfo *info)
{
	ThumberPrivate *priv;
	priv = THUMBER_GET_PRIVATE (thumber);

	g_queue_push_tail (priv->file_queue, info);

}

static void
add_task (Thumber *thumber,
	  TaskInfo *info)
{
	ThumberPrivate *priv;
	priv = THUMBER_GET_PRIVATE (thumber);

	g_queue_push_tail (priv->task_queue, info);
}

static gboolean
quit_timeout_cb (gpointer user_data)
{
	gst_thumb_main_quit ();
	return FALSE;
}

static void
thumber_set_state (Thumber *thumber,
		   ThumberState state)
{
	ThumberPrivate *priv;
	priv = THUMBER_GET_PRIVATE (thumber);

	switch (state) {
	case THUMBER_STATE_WORKING:
		if (priv->idle_id == 0) {
			priv->idle_id = g_idle_add (thumber_process_func, 
						    thumber);
			g_source_set_can_recurse (g_main_context_find_source_by_id (NULL, priv->idle_id),
						  FALSE);
		}
		if (priv->quit_timeout_id != 0) {
			g_source_remove (priv->quit_timeout_id);
			priv->quit_timeout_id = 0;
		}
		break;
	case THUMBER_STATE_PAUSED:
		if (priv->idle_id != 0) {
			g_source_remove (priv->idle_id);
			priv->idle_id = 0;
		}
		if (priv->quit_timeout_id != 0) {
			g_source_remove (priv->quit_timeout_id);
			priv->quit_timeout_id = 0;
		}
		break;
	case THUMBER_STATE_PENDING_STOP:
		if (priv->idle_id != 0) {
			g_source_remove (priv->idle_id);
			priv->idle_id = 0;
		}
		if (priv->quit_timeout_id == 0) {
			if (priv->quit_timeout >= 0) {
				priv->quit_timeout_id = g_timeout_add_seconds (priv->quit_timeout,
									       quit_timeout_cb, 
									       thumber);
			}
		}
		break;
	case THUMBER_STATE_STOPPED:
		if (priv->idle_id != 0) {
			g_source_remove (priv->idle_id);
			priv->idle_id = 0;
		}
		if (priv->quit_timeout_id == 0) {
			priv->quit_timeout_id = g_timeout_add_seconds (priv->quit_timeout, 
								       quit_timeout_cb, 
								       thumber);
		}
		break;
	default:
		/* Nothing */
		break;
	}

	priv->state = state;
}

void
thumber_dbus_method_create (Thumber *object,
			    gchar *uri,
			    gchar *mime_hint,
			    DBusGMethodInvocation *context)
{
	TaskInfo    *task   = NULL;

	task = task_info_new (mime_hint);
	task_info_add_uri (task, uri);

	add_task (object, task);

	request_resources (object);

	dbus_g_method_return (context);
}

void
thumber_dbus_method_create_many (Thumber *object,
				 GStrv uris,
				 gchar *mime_hint,
				 DBusGMethodInvocation *context)
{
	TaskInfo    *task   = NULL;

	task = task_info_new (mime_hint);

	task_info_add_uris (task, uris);

	add_task (object, task);

	request_resources (object);

	dbus_g_method_return (context, task->id);
}

void
thumber_populate_file_queue (Thumber *thumber, TaskInfo *task) {

	GSList *list = NULL;

	list = task->files;

	for (list = task->files; list; list = g_slist_next (list)) {
		FileInfo *info;
		info = list->data;

		add_file (thumber, info);
	}
}

/* Recurrency is not allowed so there is no risk of new item being started
   before the previous is done even though the pipeline bus is polled
   in the mainloop
*/

static gboolean
thumber_process_func (gpointer data)
{
	Thumber *thumber;
	FileInfo *file;
	TaskInfo *task;
	ThumberPrivate *priv;
	GError *error = NULL;
	thumber = THUMBER (data);
	priv = THUMBER_GET_PRIVATE (thumber);

	if (priv && priv->pipe &&
	    ((file = g_queue_pop_head (priv->file_queue)) != NULL)) {

		if (!thumber_pipe_run (priv->pipe,
				       file->uri,
				       &error)) {
			if (error) {
				g_signal_emit (thumber,
					       signals[ERROR_SIGNAL],
					       0,
					       file->uri,
					       1,
					       error->message);
				g_error_free (error);
				error = NULL;
			} else {
				g_signal_emit (thumber,
					       signals[ERROR_SIGNAL],
					       0,
					       file->uri,
					       0,
					       "Undefined error");
			}
		} else {
			g_signal_emit (thumber,
				       signals[READY_SIGNAL],
				       0,
				       file->uri);
		}

		file_info_free (file);
			
	} else {

		if (priv->current_task) {
			g_signal_emit (thumber,
				       signals[FINISHED_SIGNAL],
				       0,
				       priv->current_task->id);
			task_info_free (priv->current_task, FALSE);
			priv->current_task = NULL;
		}

		if ((task = g_queue_pop_head (priv->task_queue)) != NULL) {
			GValue val = {0, };


			if (priv->pipe) {
				g_object_unref (priv->pipe);
				priv->pipe = NULL;
			}
			
			priv->pipe = thumber_pipe_new ();		       			

			g_value_init (&val, G_TYPE_BOOLEAN);
			g_value_set_boolean (&val, priv->standard);
			g_object_set_property (G_OBJECT(priv->pipe), "standard", &val);
			g_value_unset (&val);
			
			g_value_init (&val, G_TYPE_BOOLEAN);
			g_value_set_boolean (&val, priv->cropped);
			g_object_set_property (G_OBJECT(priv->pipe), "cropped", &val);
			g_value_unset (&val);

			thumber_populate_file_queue (thumber, task);
			
			priv->current_task = task;
			g_signal_emit (thumber,
				       signals[STARTED_SIGNAL],
				       0,
				       priv->current_task->id);
		} else {
			if (priv->pipe) {
				g_object_unref (priv->pipe);
				priv->pipe = NULL;
			}

			release_resources (thumber);

			priv->idle_id = 0;
			return FALSE;
		}
	}
	
	return TRUE;
}

