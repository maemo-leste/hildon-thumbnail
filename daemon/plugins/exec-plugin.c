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
static GHashTable *execs;

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
string_replace (const gchar *in, const gchar *large, const gchar *normal, const gchar *cropped, const gchar *mime_type, gboolean cropping, guint mtime)
{
	gchar *ptr;
	guint total = strlen (in);
	guint len, i, off = 0, z, in_len = total;
	guint large_len, normal_len, cropped_len, mime_len, mtime_len, cropping_len;
	gchar *s_mtime = g_strdup_printf ("%lu", mtime);
	gchar *ret;

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

				if (buf[0] == 'm' && buf[1] == 'i') {
					memcpy (ret + off, mime_type, mime_len);
					off += mime_len;
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
hildon_thumbnail_plugin_create (GStrv uris, GError **error)
{
	guint i = 0;
	GString *errors = NULL;

	while (uris[i] != NULL) {
		gchar *uri = uris[i];
		GError *nerror = NULL;
		gchar *large = NULL, 
		      *normal = NULL, 
		      *cropped = NULL;
		gchar *olarge = NULL, 
		      *onormal = NULL, 
		      *ocropped = NULL;
		gchar *exec = NULL;
		gchar *mime_type = NULL;
		GFile *file = NULL;
		GFileInfo *info = NULL;
		const gchar *content_type;
		guint64 mtime;
		gchar *r_exec = NULL;

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

		hildon_thumbnail_util_get_thumb_paths (uri, &olarge, &onormal, 
						       &ocropped, &nerror);

		if (nerror)
			goto nerror_handler;

		large = g_strdup_printf ("\"%s\"", olarge);
		cropped = g_strdup_printf ("\"%s\"", ocropped);
		normal = g_strdup_printf ("\"%s\"", onormal);
		mime_type = g_strdup_printf ("\"%s\"", content_type);

		exec = g_hash_table_lookup (execs, content_type);

		r_exec = string_replace (exec, large, normal, cropped, 
					 mime_type, do_cropped, mtime);

		g_free (exec);
		g_free (normal);
		g_free (large);
		g_free (cropped);
		g_free (mime_type);

		g_spawn_command_line_sync (r_exec, NULL, NULL, NULL, NULL);

		g_free (r_exec);

// TODO
//		if (on_error) {
//			if (!errors)
//				errors = g_string_new ("");
//			g_string_append_printf (errors, "error msg")
//		}


		nerror_handler:

		if (file)
			g_object_unref (file);
		if (info)
			g_object_unref (info);

		g_free (olarge);
		g_free (onormal);
		g_free (ocropped);

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
	g_hash_table_unref (execs);
	execs = NULL;
}

void 
hildon_thumbnail_plugin_init (gboolean *cropping, GError **error)
{
	gchar *config = g_build_filename (g_get_user_config_dir (), "hildon-thumbnailer", "exec-plugin.conf", NULL);
	GKeyFile *keyfile;
	GStrv mimetypes;
	guint i = 0, length;

	keyfile = g_key_file_new ();

	if (!g_key_file_load_from_file (keyfile, config, G_KEY_FILE_NONE, NULL)) {
		g_free (config);
		do_cropped = TRUE;
		*cropping = do_cropped;
		return;
	}

	execs = g_hash_table_new_full (g_str_hash, g_str_equal,
				       (GDestroyNotify) g_free,
				       (GDestroyNotify) g_free);

	do_cropped = g_key_file_get_boolean (keyfile, "Hildon Thumbnailer", "DoCropping", NULL);
	*cropping = do_cropped;

	mimetypes = g_key_file_get_string_list (keyfile, "Hildon Thumbnailer", "MimeTypes", &length, NULL);

	while (mimetypes && mimetypes[i] != NULL) {
		gchar *exec = g_key_file_get_string (keyfile, mimetypes[i], "Exec", NULL);
		g_hash_table_replace (execs, g_strdup (mimetypes[i]), exec);
		i++;
	}

	g_strfreev (mimetypes);
	g_free (config);
	g_key_file_free (keyfile);
}
