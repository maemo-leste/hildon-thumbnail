/*  Compile with:
    gcc -Wall -g --std=gnu99 thumbnailer3.c -o thumbnailer3 $(pkg-config --cflags --libs glib-2.0 hildon-thumbnail)
 */

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <hildon-thumbnail-factory.h>

static GMainLoop *loop;
static HildonThumbnailFactory *thumbnail_factory;
static const gchar uri[] = "file:///home/divanov/MyDocs/woodcut.jpg";


static void
thumbnail_cb (HildonThumbnailFactory *thumbnail_factory,
              GdkPixbuf *pixbuf,
              GError *error,
              gpointer user_data)
{
        if (error) {
                g_debug (error->message);
        } else {
                g_debug ("Success");
        }

        g_main_loop_quit (loop);
}


gint
main (gint argc,
      gchar **argv)
{
        HildonThumbnailRequest *thumbnail_request;
        gchar *thumbnail;

        loop = g_main_loop_new (NULL, FALSE);

        thumbnail_factory = hildon_thumbnail_factory_get_instance ();

        g_debug ("hildon_thumbnail_get_uri");
        thumbnail = hildon_thumbnail_get_uri (uri, 124, 124, TRUE);
        g_free (thumbnail);

        g_debug ("hildon_thumbnail_factory_request_pixbuf");
        thumbnail_request = hildon_thumbnail_factory_request_pixbuf (
                                                 thumbnail_factory,
                                                 uri, 124, 124, TRUE, "",
                                                 thumbnail_cb, NULL, NULL);

        g_main_loop_run (loop);

        g_object_unref (thumbnail_factory);

        exit (0);
}
