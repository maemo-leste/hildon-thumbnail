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


#ifndef __GST_THUMB_PIPE_H__
#define __GST_THUMB_PIPE_H__

#include <glib-object.h>
#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#define TYPE_THUMBER_PIPE         (thumber_pipe_get_type())
#define THUMBER_PIPE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TYPE_THUMBER_PIPE, ThumberPipe))
#define THUMBER_PIPE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), TYPE_THUMBER_PIPE, ThumberPipeClass))
#define THUMBER_PIPE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TYPE_THUMBER_PIPE, ThumberPipeClass))

typedef struct ThumberPipe ThumberPipe;
typedef struct ThumberPipeClass ThumberPipeClass;

struct ThumberPipe {
	GObject parent;
};

struct ThumberPipeClass {
	GObjectClass parent;
};

GType		thumber_pipe_get_type	            (void) G_GNUC_CONST;
ThumberPipe    *thumber_pipe_new                    (void);
gboolean        thumber_pipe_run                    (ThumberPipe *pipe,
						     const gchar *uri,
						     GError     **error);

#endif
