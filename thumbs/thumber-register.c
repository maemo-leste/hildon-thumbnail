/*
 * This file is part of hildon-fm package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Luc Pionchon <luc.pionchon@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include "osso-thumbnail-factory.h"
#include "thumbs-private.h"

#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <gconf/gconf-client.h>

enum {
    THUMBER_REGISTER_ERROR = 10
} ThumberRegisterError;

GQuark reg_quark = 0;

GConfClient *client = NULL;

gboolean init_gconf()
{
    client = gconf_client_get_default();

    return client != NULL;
}

void destroy_gconf()
{
    g_object_unref(client);
}

void thumber_register(char *cmd, char *mime_type, GError **err)
{
    char *q_mime = g_strdup(mime_type);
    char *slash_pos = strchr(q_mime, '/');
    char *cmd_dir, *cmd_path;
    GError *conf_error = NULL;

    if(slash_pos) {
        *slash_pos = '@';
    }

    if(strchr(q_mime, '/')) {
        g_set_error(err, reg_quark, THUMBER_REGISTER_ERROR,
            "MIME type contains more than one slash: %s", mime_type);

        g_free(q_mime);
        return;
    }

    cmd_dir = g_strconcat(THUMBS_GCONF_DIR, "/", q_mime, NULL);
    cmd_path = get_conf_cmd_path(cmd_dir);

    gconf_client_set_string(client, cmd_path, cmd, &conf_error);

    if(conf_error) {
        g_propagate_error(err, conf_error);
    }

    g_free(cmd_dir);
    g_free(cmd_path);
    g_free(q_mime);
}

void thumber_unregister(char *cmd, GError **err)
{
    GSList *mime_dirs;
    GSList *dir;

    mime_dirs = gconf_client_all_dirs(client, THUMBS_GCONF_DIR, NULL);

    if(!mime_dirs) {
        g_set_error(err, reg_quark, THUMBER_REGISTER_ERROR,
            "Unregistering failed, no conf dir found");
        return;
    }

    for(dir = mime_dirs; dir; dir = dir->next) {
        gchar *dirname = dir->data;
        gchar *cmd_path;
        gchar *basename;
        gchar *conf_cmd;

        cmd_path = get_conf_cmd_path(dirname);
        basename = g_strdup(strrchr(dirname, '/') + 1);

        conf_cmd = gconf_client_get_string(client, cmd_path, NULL);

        if(conf_cmd) {
            unquote_mime_dir(basename);

            //g_message("Thumber dir, cmd: %s, %s", basename, conf_cmd);

            if(strcmp(conf_cmd, cmd) == 0) {
                //g_message("Deleting %s", dirname);

                gconf_client_unset(client, cmd_path, NULL);
                gconf_client_unset(client, dirname, NULL);
            }

            g_free(conf_cmd);
        } else {
            g_warning("Directory contains no command: %s", dirname);
        }

        g_free(basename);
        g_free(dirname);
        g_free(cmd_path);
    }

    g_slist_free(mime_dirs);
}

int main(int argc, char **argv)
{
    int status = 0;

    g_type_init();
    reg_quark = g_quark_from_static_string("osso-thumber-register");

    if(argc != 3) {
        printf( "Usage:\n"
                "    osso-thumber-register <handler-cmd> <mime-type>\n"
                "    osso-thumber-register -u <handler-cmd>\n"
                "Options:\n"
                "    -u : unregister specified thumber command\n"
        );

        return 1;
    } else {
        GError *err = NULL;

        if(!init_gconf()) {
            g_warning("GConf init failed");
            return 2;
        }

        if(strcmp(argv[1], "-u") == 0) {
            thumber_unregister(argv[2], &err);
        } else {
            thumber_register(argv[1], argv[2], &err);
        }

        destroy_gconf();

        if(err) {
            g_warning("Error in osso-thumber-register, code %d: %s",
                err->code, err->message);

            status = err->code ? err->code : 3;
        }

        g_clear_error(&err);
    }

    return status;
}
