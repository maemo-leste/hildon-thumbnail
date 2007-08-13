 /*
  * This file is part of osso-thumbnail package
  *
  * Copyright (C) 2005-2007 Nokia Corporation.  All rights reserved.
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
#include "hildon-thumbnail-factory.h"
#include "hildon-thumber-common.h"

#include <osso-mem.h>
#include <osso-log.h>
#include <unistd.h>
#include <stdlib.h>
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

#define BLK 4

GdkPixbuf *create_thumb(const gchar *local_file, const gchar *mime_type,
    guint width, guint height, HildonThumbnailFlags flags,
    gchar ***opt_keys, gchar ***opt_values, GError **error)
{
    if((flags & HILDON_THUMBNAIL_FLAG_CROP)) {
       GdkPixbuf *pixbuf, *result = NULL;
       GdkPixbufLoader *loader;
       guchar buffer[2048]; /* size must be dividable by BLK */
       FILE *f; 
       size_t items_read = sizeof(buffer) / BLK;
       size_t desired_max_area;

       f = fopen(local_file, "r");
       if (!f) return NULL;

       desired_max_area = (width * height * 4) - 1;
       loader = gdk_pixbuf_loader_new ();
       g_signal_connect(loader, "size-prepared", G_CALLBACK(size_prepared),
                        GINT_TO_POINTER(desired_max_area));

       while (items_read >= sizeof(buffer) / BLK)
       {
         long pos;
         int nbytes;

         /* read BLK bytes at a time as much as possible */
         if ((pos = ftell(f)) == -1)
         {
           gdk_pixbuf_loader_close(loader, NULL);
           goto cleanup;
         }
         items_read = fread(buffer, BLK, sizeof(buffer) / BLK, f);

         if (items_read < sizeof(buffer) / BLK)
         {
           /* read again one byte at a time */
           if (fseek(f, pos, SEEK_SET) == -1)
           {
             gdk_pixbuf_loader_close(loader, NULL);
             goto cleanup;
           }
           nbytes = fread(buffer, 1, sizeof(buffer), f);
         }
         else
           nbytes = items_read * BLK;

         if (!gdk_pixbuf_loader_write(loader, buffer, nbytes, error))
         { /* We have to call close before unreffing */
           gdk_pixbuf_loader_close(loader, NULL);
           goto cleanup;
         }
       }

       if (!gdk_pixbuf_loader_close(loader, error))
         goto cleanup;

       /* Loader owns reference to this pixbuf */
       pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);

       if (pixbuf)
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
 	   ULOG_ERR_F("can't create thumb: %s", error->message);
	   g_error_free (error);
       }
       return pixbuf;
    }

    return NULL;
}

static void
thumbnailer_oom_func (size_t cur, size_t max, void *data)
{
    ULOG_DEBUG_F("OOM: %u of %u!", cur, max);
    exit(1);
}

int main(int argc, char **argv)
{
    int result;

    setpriority(PRIO_PROCESS, getpid(), 10);
    g_thread_init(NULL);
    result = osso_mem_saw_enable(4 << 20, 64, thumbnailer_oom_func, NULL);
    if (result != 0)
      ULOG_ERR_F("osso_mem_saw_enable failed with error %d", result);
    else
      {
	result = hildon_thumber_main(&argc, &argv, create_thumb);
	osso_mem_saw_disable();
      }

    return result;
}
