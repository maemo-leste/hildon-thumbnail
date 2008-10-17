/*
 * This file is part of hildon-thumbnail package
 *
 * Copyright (C) 2005 Nokia Corporation.  All Rights reserved.
 *
 * Contact: Marius Vollmer <marius.vollmer@nokia.com>
 * Author: Philip Van Hoof <pvanhoof@gnome.org>
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

#include <string.h>
#include <glib.h>
#include <gio/gio.h>
#include <dbus/dbus-glib-bindings.h>


#define EXEC_ERROR_DOMAIN	"HildonThumbnailerExec"
#define EXEC_ERROR		g_quark_from_static_string (EXEC_ERROR_DOMAIN)

#include "utils.h"
#include "exec-plugin.h"
#include "hildon-thumbnail-plugin.h"


static gchar **supported = NULL;
static gboolean do_cropped = TRUE;
static GHashTable *execs = NULL;
static GFileMonitor *monitor = NULL;

const gchar** 
hildon_thumbnail_plugin_supported (void)
{
	if (!supported) {
		GList *formats = g_hash_table_get_keys (execs);
		GList *copy;
		guint i = 0;

		supported = (gchar **) g_malloc0 (sizeof (gchar *) * (g_list_length (formats) + 1));

		copy = formats;
		while (copy) {
			supported[i] =  g_strdup (copy->data);
			i++;
			copy = g_list_next (copy);
		}

		g_list_free (formats);
	}

	return (const gchar**) supported;
}


static gchar*
string_replace (const gchar *in, const gchar *uri, const gchar *large, const gchar *normal, const gchar *cropped, const gchar *mime_type, const gchar *mime_type_at, gboolean cropping, guint mtime)
{
	gchar *ptr;
	guint total = strlen (in);
	guint len, i, off = 0, z, in_len = total;
	guint large_len, normal_len, cropped_len, mime_len, 
		mtime_len, cropping_len, mime_at_len, uri_len;
	gchar *s_mtime = g_strdup_printf ("%u", mtime);
	gchar *ret;


	ptr = (gchar *) in;
	len = strlen (uri);
	while (ptr) {
		ptr = strstr ("{uri}", ptr);
		total += len;
	}
	uri_len = len;

	ptr = (gchar *) in;
	len = strlen (normal);
	while (ptr) {
		ptr = strstr ("{normal}", ptr);
		total += len;
	}
	normal_len = len;

	ptr = (gchar *) in;
	len = strlen (large);
	while (ptr) {
		ptr = strstr ("{large}", ptr);
		total += len;
	}
	large_len = len;

	ptr = (gchar *) in;
	len = strlen (cropped);
	while (ptr) {
		ptr = strstr ("{cropped}", ptr);
		total += len;
	}
	cropped_len = len;

	ptr = (gchar *) in;
	len = strlen (mime_type);
	while (ptr) {
		ptr = strstr ("{mime}", ptr);
		total += len;
	}
	mime_len = len;

	ptr = (gchar *) in;
	len = strlen (mime_type_at);
	while (ptr) {
		ptr = strstr ("{mime_at}", ptr);
		total += len;
	}
	mime_at_len = len;

	ptr = (gchar *) in;
	len = strlen (s_mtime);
	while (ptr) {
		ptr = strstr ("{mtime}", ptr);
		total += len;
	}
	mtime_len = len;

	ptr = (gchar *) in;
	len = cropping?3:2;
	while (ptr) {
		ptr = strstr ("{docrop}", ptr);
		total += len;
	}
	cropping_len = len;

	ret = (gchar *) g_malloc0 (sizeof (gchar) * total + 5);

	i = 0;
	off = 0;

	for (i = 0; i < in_len && (off < total + 1); i++) {
		if (in[i] == '{') {
			gchar buf[11];
			gboolean okay = FALSE;

			i++; /*A*/

			memset (buf, 0, 11);

			for (z = i; z < i + 10; z++) {
				if (in[z] == '}') {
					okay = TRUE;
					break;
				}
				buf[z-i] = in[z];
			}

			if (okay) {

				if (buf[0] == 'm' && buf[1] == 't') {
					memcpy (ret + off, s_mtime, mtime_len);
					off += mtime_len;
				}

				if (buf[0] == 'm' && buf[1] == 'i' && buf[4] != '_') {
					memcpy (ret + off, mime_type, mime_len);
					off += mime_len;
				}

				if (buf[0] == 'm' && buf[1] == 'i' && buf[4] == '_') {
					memcpy (ret + off, mime_type_at, mime_at_len);
					off += mime_at_len;
				}

				if (buf[0] == 'u') {
					memcpy (ret + off, uri, uri_len);
					off += uri_len;
				}

				if (buf[0] == 'n') {
					memcpy (ret + off, normal, normal_len);
					off += normal_len;
				}


				if (buf[0] == 'l') {
					memcpy (ret + off, large, large_len);
					off += large_len;
				}

				if (buf[0] == 'c') {
					memcpy (ret + off, cropped, cropped_len);
					off += cropped_len;
				}

				if (buf[0] == 'd' && cropping) {
					memcpy (ret + off, "yes", 3);
					off += 3;
				}

				if (buf[0] == 'd' && !cropping) {
					memcpy (ret + off, "no", 2);
					off += 2;
				}

				i += z - i;

			} else
				i--; /*A*/

		} else {
			ret[off] = in[i];
			off++;
		}
	}

	g_free (s_mtime);

	return ret;
}


void
hildon_thumbnail_plugin_create (GStrv uris, gchar *mime_hint, gchar *VFS_id, GError **error)
{
	guint i = 0;
	GString *errors = NULL;

	while (uris[i] != NULL) {
		gchar *uri = uris[i];
		GError *nerror = NULL;
		gchar *large = NULL, 
		      *normal = NULL, 
		      *cropped = NULL;
		gchar *exec = NULL;
		gchar *mime_type = NULL;
		gchar *mime_type_at = NULL;
		GFile *file = NULL;
		GFileInfo *info = NULL;
		const gchar *content_type;
		guint64 mtime;
		gchar *r_exec = NULL;
		gchar *slash_pos;

		file = g_file_new_for_uri (uri);
		info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				  G_FILE_ATTRIBUTE_TIME_MODIFIED,
				  G_FILE_QUERY_INFO_NONE,
				  NULL, &nerror);

		if (nerror)
			goto nerror_handler;

		mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
		content_type = g_file_info_get_content_type (info);

		hildon_thumbnail_util_get_thumb_paths (uri, &large, &normal, 
						       &cropped);

		mime_type = g_strdup (content_type);
		mime_type_at = g_strdup (content_type);
		slash_pos = strchr(mime_type_at, '/');
		if(slash_pos)
			*slash_pos = '@';

		exec = g_hash_table_lookup (execs, content_type);

		r_exec = string_replace (exec, uri, large, normal, cropped, 
					 mime_type, mime_type_at, do_cropped, mtime);

		g_free (exec);
		g_free (mime_type);
		g_free (mime_type_at);

		g_spawn_command_line_sync (r_exec, NULL, NULL, NULL, NULL);

		g_free (r_exec);

		nerror_handler:


		if (nerror) {
			if (!errors)
				errors = g_string_new ("");
			g_string_append_printf (errors, "[`%s': %s] ", 
								    uri, nerror->message);
			g_error_free (nerror);
			nerror = NULL;
		}

		if (file)
			g_object_unref (file);
		if (info)
			g_object_unref (info);

		g_free (large);
		g_free (normal);
		g_free (cropped);

		i++;
	}

	if (errors) {
		g_set_error (error, EXEC_ERROR, 0,
			     errors->str);
		g_string_free (errors, TRUE);
	}

	return;
}

void 
hildon_thumbnail_plugin_stop (void)
{
	if (supported)
		g_strfreev (supported);
	supported = NULL;
	if (monitor)
		g_object_unref (monitor);
	g_hash_table_unref (execs);
	execs = NULL;
}

static void
reload_config (const gchar *config) 
{
	GKeyFile *keyfile;
	GStrv mimetypes;
	guint i = 0, length;

	if (!execs)
		execs = g_hash_table_new_full (g_str_hash, g_str_equal,
					       (GDestroyNotify) g_free,
					       (GDestroyNotify) g_free);

	keyfile = g_key_file_new ();

	if (!g_key_file_load_from_file (keyfile, config, G_KEY_FILE_NONE, NULL)) {
		do_cropped = TRUE;
		return;
	}

	do_cropped = g_key_file_get_boolean (keyfile, "Hildon Thumbnailer", "DoCropping", NULL);

	mimetypes = g_key_file_get_string_list (keyfile, "Hildon Thumbnailer", "MimeTypes", &length, NULL);

	while (mimetypes && mimetypes[i] != NULL) {
		gchar *exec = g_key_file_get_string (keyfile, mimetypes[i], "Exec", NULL);
		g_hash_table_replace (execs, g_strdup (mimetypes[i]), exec);
		i++;
	}

	g_strfreev (mimetypes);
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
	gchar *config = g_build_filename (g_get_user_config_dir (), "hildon-thumbnailer", "exec-plugin.conf", NULL);
	GFile *file = g_file_new_for_path (config);
	guint i = 0;
	const gchar **supported;

	monitor =  g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);

	g_signal_connect (G_OBJECT (monitor), "changed", 
			  G_CALLBACK (on_file_changed), NULL);

	reload_config (config);

	*cropping = do_cropped;

	if (func) {
		supported = hildon_thumbnail_plugin_supported ();
		if (supported) {
			while (supported[i] != NULL) {
				func (thumbnailer, supported[i], "GIO", module);
				i++;
			}
		}
	}

	g_free (config);
}
