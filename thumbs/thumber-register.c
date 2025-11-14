/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * This file is part of hildon-fm package
 *
 * Copyright (C) 2005 Nokia Corporation.
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

#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <glib-object.h>


#define CONVERT_CMD BIN_PATH G_DIR_SEPARATOR_S "hildon-thumbnailer-wrap.sh \"%s\" \"{uri}\" \"{large}\" \"{normal}\" \"{cropped}\" \"{mime_at}\" \"{mime}\""


static void
write_keyfile (const gchar *filen, GKeyFile *keyfile)
{
	FILE* file = fopen (filen, "w");

	if (file) {
		gsize len;
		char *str = g_key_file_to_data (keyfile, &len, NULL);
		fputs (str, file);
		g_free (str);
		fclose (file);
	}
}

static void 
thumber_register(char *cmd, char *mime_type, GError **err)
{
	gchar *config = g_build_filename (g_get_user_config_dir (), "hildon-thumbnailer", "exec-plugin.conf", NULL);
	GKeyFile *keyfile;
	gchar *r_cmd;

	gchar *d = g_build_filename (g_get_user_config_dir (), "hildon-thumbnailer", NULL);

	g_mkdir_with_parents (d, 0770);
	g_free (d);

	keyfile = g_key_file_new ();
	if (!g_key_file_load_from_file (keyfile, config, G_KEY_FILE_NONE, NULL)) {
		gchar **mimetypes;

		mimetypes = (gchar **) g_malloc0 (sizeof (gchar *) * 2);
		mimetypes[0] = g_strdup (mime_type);
		g_key_file_set_boolean (keyfile, "Hildon Thumbnailer", "DoCropping", FALSE);
		g_key_file_set_string_list (keyfile, "Hildon Thumbnailer", "MimeTypes", 
					    (const gchar **) mimetypes, (gsize) 1);

		g_strfreev (mimetypes);

	} else {
		guint i, z = 0;
		gsize length;
		gchar **o;
		gchar **mimetypes;

		o = g_key_file_get_string_list (keyfile, "Hildon Thumbnailer", "MimeTypes", 
							&length, NULL);

		mimetypes = (gchar **) g_malloc0 (sizeof (gchar *) * (length + 2));
		for (i = 0; i< length; i++) {
			if (strcmp (o[i], mime_type) != 0) {
				mimetypes[z] = g_strdup (o[i]);
				z++;
			}
		}

		mimetypes[z] = g_strdup (mime_type);
		g_strfreev (o);

		g_key_file_set_string_list (keyfile, "Hildon Thumbnailer", "MimeTypes", 
					    (const gchar **) mimetypes, (gsize) length+1);

		g_strfreev (mimetypes);
	}

	r_cmd = g_strdup_printf (CONVERT_CMD, cmd);

	g_key_file_set_string (keyfile, mime_type, "Exec", r_cmd);

	write_keyfile (config, keyfile);

	g_free (r_cmd);
	g_free (config);
	g_key_file_free (keyfile);
}

static void 
thumber_unregister(char *cmd, GError **err)
{
	gchar *config = g_build_filename (g_get_user_config_dir (), "hildon-thumbnailer", "exec-plugin.conf", NULL);
	GKeyFile *keyfile;

	keyfile = g_key_file_new ();

	if (g_key_file_load_from_file (keyfile, config, G_KEY_FILE_NONE, NULL)) {
		guint i, z;
		gsize length;
		gchar **o;
		gchar **mimetypes = NULL;

		o = g_key_file_get_string_list (keyfile, "Hildon Thumbnailer", "MimeTypes", 
							&length, NULL);

		if (length > 0) {
			mimetypes = (gchar **) g_malloc0 (sizeof (gchar *) * length);

			z = 0;

			for (i = 0; i< length; i++) {
				gboolean doit = FALSE;
				gchar *exec = g_key_file_get_string (keyfile, o[i], "Exec", NULL);

				if (exec) {
					gchar *ptr = strchr (exec, '"');
					if (ptr) {
						gchar *check;
						ptr++;
						check = ptr;
						ptr = strchr (ptr, '"');
						if (ptr) {
							*ptr = '\0';
							if (strcmp (check, cmd) == 0) {
								g_key_file_remove_group (keyfile, o[i], NULL);
								doit = FALSE;
							} else
								doit = TRUE;
						} else
							doit = TRUE;
					} else
						doit = TRUE;
				} else
					doit = TRUE;

				if (doit) {
					mimetypes[z] = g_strdup (o[i]);
					z++;
				}
			}
		}

		g_strfreev (o);

		if (mimetypes) {
			g_key_file_set_string_list (keyfile, "Hildon Thumbnailer", "MimeTypes", 
						    (const gchar **) mimetypes, (gsize) length+1);

			g_strfreev (mimetypes);
		}

		write_keyfile (config, keyfile);
	}

	g_free (config);
	g_key_file_free (keyfile);
}


static void 
thumber_unregister_mime (char *mime, GError **err)
{
	gchar *config = g_build_filename (g_get_user_config_dir (), "hildon-thumbnailer", "exec-plugin.conf", NULL);
	GKeyFile *keyfile;
	gchar **mimetypes = NULL;
	guint i = 0;
	gsize length;

	keyfile = g_key_file_new ();

	if (g_key_file_load_from_file (keyfile, config, G_KEY_FILE_NONE, NULL)) {
		guint z;
		gchar **o;

		o = g_key_file_get_string_list (keyfile, "Hildon Thumbnailer", "MimeTypes", 
							&length, NULL);

		if (length > 0) {
			mimetypes = (gchar **) g_malloc0 (sizeof (gchar *) * length);

			z = 0;

			for (i = 0; i< length; i++) {
				if (strcmp (o[i], mime) == 0) {
					g_key_file_remove_group (keyfile, o[i], NULL);
				} else {
					mimetypes[z] = g_strdup (o[i]);
					z++;
				}
			}
		}

		g_strfreev (o);

		if (mimetypes) {
			g_key_file_set_string_list (keyfile, "Hildon Thumbnailer", "MimeTypes", 
					    (const gchar **) mimetypes, (gsize) length+1);

			g_strfreev (mimetypes);
		}

		write_keyfile (config, keyfile);
	}

	g_free (config);
	g_key_file_free (keyfile);
}

int main(int argc, char **argv)
{
    int status = 0;

    g_type_init();
 
    if(argc != 3) {
        printf( "Usage:\n"
                "    osso-thumber-register <handler-cmd> <mime-type>\n"
                "    osso-thumber-register -u <handler-cmd>\n"
                "    osso-thumber-register -um <mime>\n"
                "Options:\n"
                "    -u : unregister specified thumber command\n"
        );

        return 1;
    } else {
        GError *err = NULL;

        if(strcmp(argv[1], "-u") == 0) {
            thumber_unregister(argv[2], &err);
        } else if(strcmp(argv[1], "-um") == 0) {
            thumber_unregister_mime(argv[2], &err);
        } else {
            thumber_register(argv[1], argv[2], &err);
        }

        if(err) {
            g_warning("Error in osso-thumber-register, code %d: %s",
                err->code, err->message);

            status = err->code ? err->code : 3;
        }

        g_clear_error(&err);
    }

    return status;
}
