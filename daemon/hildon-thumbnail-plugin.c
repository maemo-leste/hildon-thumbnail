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
 */

#include "hildon-thumbnail-plugin.h"

static GList *outplugs = NULL;
static GStaticRecMutex mutex = G_STATIC_REC_MUTEX_INIT;

typedef gboolean (*IsActiveFunc) (void);
typedef gboolean (*StopFunc) (void);

void
hildon_thumbnail_outplugin_unload (GModule *module)
{
	StopFunc stop_func;
	gboolean resident = FALSE;

	g_static_rec_mutex_lock (&mutex);

	if (g_module_symbol (module, "hildon_thumbnail_outplugin_stop", (gpointer *) &stop_func)) {
		resident = stop_func ();
	}

	outplugs = g_list_remove (outplugs, module);

	if (!resident)
		g_module_close (module);

	g_static_rec_mutex_unlock (&mutex);
}

GModule*
hildon_thumbnail_outplugin_load (const gchar *module_name)
{
	GModule *module;

	g_return_val_if_fail (module_name != NULL, NULL);

	g_static_rec_mutex_lock (&mutex);

	module = g_module_open (module_name, G_MODULE_BIND_LOCAL);

	if (!module) {
		g_warning ("Could not load thumbnailer module '%s', %s\n", 
			   module_name, 
			   g_module_error ());
	} else {
		/* g_module_make_resident (module); */
		outplugs = g_list_prepend (outplugs, module);
	}

	g_static_rec_mutex_unlock (&mutex);

	return module;
}

typedef void (*OutFunc) (const guchar *rgb8_pixmap, 
			 guint width, guint height,
			 guint rowstride, guint bits_per_sample,
			 OutType type,
			 guint64 mtime, 
			 const gchar *uri, 
			 GError **error);

void
hildon_thumbnail_outplugins_do_out (const guchar *rgb8_pixmap, 
				    guint width, guint height,
				    guint rowstride, guint bits_per_sample,
				    OutType type,
				    guint64 mtime, 
				    const gchar *uri, 
				    GError **error)
{
	GList *copy = outplugs;
	GString *errors = NULL;
	GQuark domain;

	g_static_rec_mutex_lock (&mutex);

	while (copy) {
		GModule *module = copy->data;
		OutFunc out_func;
		GError *nerror = NULL;

		if (g_module_symbol (module, "hildon_thumbnail_outplugin_out", (gpointer *) &out_func)) {

			IsActiveFunc isac_func;

			if (g_module_symbol (module, "hildon_thumbnail_outplugin_is_active", (gpointer *) &isac_func)) {
				if (isac_func ()) {
					out_func (rgb8_pixmap, width, height, rowstride, bits_per_sample, type, mtime, uri, &nerror);

					if (nerror) {
						if (!errors) {
							errors = g_string_new ("");
							domain = nerror->domain;
						}
						g_string_append (errors, nerror->message);
						g_error_free (nerror);
					}
				}
			}

		}
		copy = g_list_next (copy);
	}

	if (errors) {
		g_set_error (error, domain, 0, errors->str);
		g_string_free (errors, TRUE);
	}

	g_static_rec_mutex_unlock (&mutex);
}



GModule *
hildon_thumbnail_plugin_load (const gchar *module_name)
{
	GModule *module;

	g_return_val_if_fail (module_name != NULL, NULL);

	g_static_rec_mutex_lock (&mutex);

	module = g_module_open (module_name, G_MODULE_BIND_LOCAL);

	if (!module) {
		g_warning ("Could not load thumbnailer module '%s', %s\n", 
			   module_name, 
			   g_module_error ());
	} /* else {
		g_module_make_resident (module);
	} */

	g_static_rec_mutex_unlock (&mutex);

	return module;
}

typedef GStrv (*SupportedFunc) (void);

GStrv
hildon_thumbnail_plugin_get_supported (GModule *module) 
{
	GStrv supported = NULL;
	SupportedFunc supported_func;

	g_static_rec_mutex_lock (&mutex);

	if (g_module_symbol (module, "hildon_thumbnail_plugin_supported", (gpointer *) &supported_func)) {
		supported = (supported_func) ();
	}

	g_static_rec_mutex_unlock (&mutex);

	return supported;
}


typedef void (*InitFunc) (gboolean *cropping, register_func func, gpointer instance, GModule *module, GError **error);

void
hildon_thumbnail_plugin_do_init (GModule *module, gboolean *cropping, register_func in_func, gpointer instance, GError **error)
{
	InitFunc func;

	g_static_rec_mutex_lock (&mutex);

	if (g_module_symbol (module, "hildon_thumbnail_plugin_init", (gpointer *) &func)) {
		(func) (cropping, in_func, instance, module, error);
	}

	g_static_rec_mutex_unlock (&mutex);

}

typedef void (*CreateFunc) (GStrv uris, gchar *mime_hint, GStrv *failed_uris, GError **error);

void 
hildon_thumbnail_plugin_do_create (GModule *module, GStrv uris, gchar *mime_hint, GStrv *failed_uris, GError **error)
{
	CreateFunc func;

	g_static_rec_mutex_lock (&mutex);

	if (g_module_symbol (module, "hildon_thumbnail_plugin_create", (gpointer *) &func)) {
		(func) (uris, mime_hint, failed_uris, error);
	}

	g_static_rec_mutex_unlock (&mutex);
}

void
hildon_thumbnail_plugin_do_stop (GModule *module)
{
	StopFunc func;
	gboolean resident = FALSE;

	g_static_rec_mutex_lock (&mutex);

	if (g_module_symbol (module, "hildon_thumbnail_plugin_stop", (gpointer *) &func)) {
		resident = (func) ();
	}

	if (!resident)
		g_module_close (module);

	g_static_rec_mutex_unlock (&mutex);
}
