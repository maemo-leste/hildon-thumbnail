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

#include <string.h>
#include "md5.h"
#include "utils.h"

#define UTILS_ERROR_DOMAIN	"HildonThumbnailerUtils"
#define UTILS_ERROR		g_quark_from_static_string (UTILS_ERROR_DOMAIN)

static void 
md5_digest_to_ascii(guchar digest[16], gchar str[33])
{
	gint i;
	gchar *cursor = str;
	for(i = 0; i < 16; i++) {
		g_sprintf(cursor, "%02x", digest[i]);
		cursor += 2;
	}
}

static void 
md5_c_string(const gchar *str, gchar ascii_digest[33])
{
	md5_state_t md5;
	guchar digest[16];

	md5_init(&md5);
	md5_append(&md5, (const unsigned char *)str, strlen(str));
	md5_finish(&md5, digest);
	md5_digest_to_ascii(digest, ascii_digest);
}

void
hildon_thumbnail_util_get_thumb_paths (const gchar *uri, gchar **large, gchar **normal, GError **error)
{
	gchar ascii_digest[33];
	gchar thumb_filename[128];

	static gchar *large_dir = NULL;
	static gchar *normal_dir = NULL;

	/* I know we leak, but it's better than doing memory fragementation on 
	 * these strings ... */

	if (!large_dir)
		large_dir = g_build_filename (g_get_home_dir (), ".thumbnails", "large", NULL);

	if (!normal_dir)
		normal_dir = g_build_filename (g_get_home_dir (), ".thumbnails", "normal", NULL);

	*large = NULL;
	*normal = NULL;

	if(!g_file_test(large_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (large_dir, 0770);
	if(!g_file_test(normal_dir, G_FILE_TEST_EXISTS))
		g_mkdir_with_parents (normal_dir, 0770);

	md5_c_string (uri, ascii_digest);

	g_sprintf (thumb_filename, "%s.png", ascii_digest);

	*large = g_build_filename (large_dir, thumb_filename, NULL);
	*normal = g_build_filename (normal_dir, thumb_filename, NULL);

}
