 /*
  * This file is part of osso-thumbnail package
  *
  * Copyright (C) 2005, 2006 Nokia Corporation.  All rights reserved.
  *
  * Contact: Marius Vollmer <marius.vollmer@nokia.com>
  *
  * This library is free software; you can redistribute it and/or
  * modify it under the terms of the GNU Lesser General Public License
  * version 2.1 as published by the Free Software Foundation.
  *
  * This library is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  * Lesser General Public License for more details.
  *
  * You should have received a copy of the GNU Lesser General Public
  * License along with this library; if not, write to the Free
  * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
  * 02110-1301 USA
  *
  */

#include "config.h"
#include "osso-thumbnail-factory.h"
#include "osso-thumber-common.h"

#include <osso-mem.h>
#include <osso-log.h>
#include <unistd.h>
#include <sys/resource.h>
#include <stdio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

GdkPixbuf *crop_resize(GdkPixbuf *src, int width, int height) {
    int x = width, y = height;
    int a = gdk_pixbuf_get_width(src);
    int b = gdk_pixbuf_get_height(src);

    GdkPixbuf *dest;

    // This is the automagic cropper algorithm 
    // It is an optimized version of a system of equations
    // Basically it maximizes the final size while minimizing the scale
    
    int nx, ny;
    double na, nb;
    double offx = 0, offy = 0;
    double scax, scay;

    na = a;
    nb = b;

    if(a < x && b < y) {
        //nx = a;
        //ny = b;
        g_object_ref(src);
        return src;
    } else {
        int u, v;

        nx = u = x;
        ny = v = y;

        if(a < x) {
            nx = a;
            u = a;
        }

        if(b < y) {
            ny = b;
            v = b;
        }

        //printf("na=%f, nb=%f, nx=%d, ny=%d, u=%d, v=%d\n", na, nb, nx, ny, u, v);

        if(a * y < b * x) {
            nb = (double)a * v / u;
            // Center
            offy = (double)(b - nb) / 2;
        } else {
            na = (double)b * u / v;
            // Center
            offx = (double)(a - na) / 2;
        }
    }

    // gdk_pixbuf_scale has crappy inputs
    scax = scay = (double)nx / na;

    offx = -offx * scax;
    offy = -offy * scay;

    /*
    printf("(%d, %d) -> (%d, %d) => (%f, %f) -> (%d, %d)\n",
        a, b, x, y, na, nb, nx, ny);
    printf("offx=%f, offy=%f, scax=%f, scay=%f\n",
        offx, offy, scax, scay);
    */

    dest = gdk_pixbuf_new(gdk_pixbuf_get_colorspace(src),
        gdk_pixbuf_get_has_alpha(src), gdk_pixbuf_get_bits_per_sample(src),
        nx, ny);

    gdk_pixbuf_scale(src, dest, 0, 0, nx, ny, offx, offy, scax, scay,
        GDK_INTERP_BILINEAR);

    return dest;
}

static void size_prepared(GdkPixbufLoader *loader,
  gint width, gint height, gpointer user_data)
{
  gint pixels, desired_max_area;

  desired_max_area = GPOINTER_TO_INT(user_data);
  pixels = width * height;

  /* Do we want to downscale by factor 2 or greater while unpacking */
  if (pixels >= desired_max_area) {
      GdkPixbufFormat *format = NULL;
      gchar *format_name = NULL;
      gint   factor = 1;

      format = gdk_pixbuf_loader_get_format (loader);
      if (format) {
          format_name = gdk_pixbuf_format_get_name(format);
      }

      if (format_name && g_ascii_strcasecmp(format_name, "jpeg") == 0) {

          for (factor = 1;
               factor < 3 && ((pixels >> (2 * factor)) > desired_max_area);
               factor++);

          g_message("Scaling jpeg down by factor %d", 1 << factor);
          gdk_pixbuf_loader_set_size (loader, width >> factor, height >> factor);
      }

      g_free (format_name);
  }
}

GdkPixbuf *create_thumb(const gchar *local_file, const gchar *mime_type,
    guint width, guint height, OssoThumbnailFlags flags,
    gchar ***opt_keys, gchar ***opt_values, GError **error)
{
    if((flags & OSSO_THUMBNAIL_FLAG_CROP)) {
       GdkPixbuf *pixbuf, *result = NULL;
       GdkPixbufLoader *loader;
       guchar buffer[2048];
       FILE *f; 
       size_t bytes_read;
       size_t desired_max_area;

       f = fopen(local_file, "r");
       if (!f) return NULL;
     
       desired_max_area = (width * height * 4) - 1;
       loader = gdk_pixbuf_loader_new ();
       g_signal_connect(loader, "size-prepared", G_CALLBACK(size_prepared),
                        GINT_TO_POINTER(desired_max_area));

       do
       {
         bytes_read = fread(buffer, 1, sizeof(buffer), f);
         if (!gdk_pixbuf_loader_write(loader, buffer, bytes_read, error))
         { /* We have to call close before unreffing */
           gdk_pixbuf_loader_close(loader, NULL);
           goto cleanup;
         }
       } while (bytes_read >= sizeof(buffer));

       if (!gdk_pixbuf_loader_close(loader, error))
         goto cleanup;
       
       /* Loader owns reference to this pixbuf */
       pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);

       if(pixbuf)
         result = crop_resize(pixbuf, width, height);
cleanup:
       fclose(f);
       g_object_unref(loader);
       return result;

    } else {
       GdkPixbuf *pixbuf;
       GError *error = NULL;

       pixbuf = gdk_pixbuf_new_from_file_at_size (local_file, width, height, 
	 					  &error);
       if (error) {
 	   ULOG_ERR ("can't create thumb: %s", error->message);
	   g_error_free (error);
       }
       return pixbuf;
    }
    
    return NULL;
}

int main(int argc, char **argv)
{
    int result;

    setpriority(PRIO_PROCESS, getpid(), 10);
    result = osso_mem_saw_enable(4 << 20, 64, NULL, NULL);
    if (result != 0)
      ULOG_ERR ("can't install memory watchdog: code %d\n", result);
    else
      {
	result = osso_thumber_main(&argc, &argv, create_thumb);
	osso_mem_saw_disable();
      }

    return result;
}
