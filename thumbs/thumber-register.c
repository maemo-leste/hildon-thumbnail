/*
 * This file is part of hildon-fm package
 *
 * Copyright (C) 2005 Nokia Corporation.
 *
 * Contact: Marius Vollmer <marius.vollmer@nokia.com>
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

enum {
    THUMBER_REGISTER_ERROR = 10
} ThumberRegisterError;

GQuark reg_quark = 0;

void thumber_register(char *cmd, char *mime_type, GError **err)
{
 
}

void thumber_unregister(char *cmd, GError **err)
{
 
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

        if(strcmp(argv[1], "-u") == 0) {
            thumber_unregister(argv[2], &err);
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
