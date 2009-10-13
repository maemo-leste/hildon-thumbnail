#include <gtk/gtk.h>
#include <hildon-albumart-factory.h>
#include <hildon-thumbnail-factory.h>

GtkWidget *window;
GtkWidget *box;
GtkWidget *hbox;
GtkWidget *image, *imaget;
GtkWidget *atext;
GtkWidget *btext;
GtkWidget *button;


#ifdef OLDAPI
static void
on_art_back (HildonAlbumartFactoryHandle handle, gpointer user_data, GdkPixbuf *albumart, GError *error)
{
	if (albumart) {

		gtk_image_set_from_pixbuf (user_data, albumart);
	}
}
#else
static void 
on_art_back (HildonAlbumartFactory *self, GdkPixbuf *albumart, GError *error, gpointer user_data)
{
	if (albumart) {
		GdkPixbuf *b = hildon_thumbnail_orientate ("file:///home/user/.cache/media-art/album-325dca7d85d2b6a2f09f2486c125e5fc-681aeee31930af7af8d1ee209ba0195f.jpeg", 
		                                       "2", albumart);
		gtk_image_set_from_pixbuf (user_data, b);
		g_object_unref (b);
	}
}
#endif

void _thumbnail_created_cb2 (HildonThumbnailFactoryHandle handle,
    gpointer user_data, GdkPixbuf *thumbnail, GError *error)
{
    if(thumbnail)                                                                                      
    {                                         
	gtk_image_set_from_pixbuf (user_data, thumbnail) ;  
    }

}

static void 
_thumbnail_created_cb (HildonThumbnailFactory *self,
            GdkPixbuf *thumbnail, GError *error, gpointer user_data) {                             
    printf("thumbnail_created_cb\n");                                                                  
    if(error)
        printf("Error: %s\n",error->message);                                                          
    if(thumbnail)                                                                                      
    {                                         
	gtk_image_set_from_pixbuf (user_data, thumbnail) ;  
    }
    else printf("thumbnail: NULL\n");                                                                  
} 

static void
on_button_clicked (GtkButton *button, gpointer user_data)
{
/*
	gchar *album, *artist;

	album = gtk_entry_get_text (btext);
	artist = gtk_entry_get_text (atext);

#ifdef OLDAPI
	hildon_albumart_factory_load(artist, album, "album", on_art_back, image);
#else
	HildonAlbumartFactory *f = hildon_albumart_factory_get_instance ();
	HildonAlbumartRequest *r1, *r2;

	r1 = hildon_albumart_factory_queue (f, artist, album, "album",
		on_art_back, image, NULL);

	r2 = hildon_albumart_factory_queue_thumbnail (f, artist, album, "album",
		256, 256, TRUE,
		on_art_back, imaget, NULL);


	g_print ("Requesting Nelly!\n");

printf ("LOC: %s\n",hildon_thumbnail_get_uri("file:///home/user/.cache/media-art/album-325dca7d85d2b6a2f09f2486c125e5fc-681aeee31930af7af8d1ee209ba0195f.jpeg", 100, 100, FALSE));


	hildon_albumart_factory_queue_thumbnail(
                 f,
                 "Nelly Furtado",
                 "2008 Grammy Nominees",
                 "album",
                 1, 1, TRUE,
                 on_art_back, imaget, NULL);



g_print ("%s\n", hildon_albumart_get_path("Nelly Furtado",
                 "2008 Grammy Nominees", "album"));
*/

/*
 hildon_thumbnail_factory_request_pixbuf (hildon_thumbnail_factory_get_instance (),
            "file:///Does'nexist",
            256, 256,
            FALSE,
            NULL,
            _thumbnail_created_cb,
            user_data,
            NULL);
*/

hildon_thumbnail_factory_load(
            "file:///tmp/20090104_002_anticlock.jpg", "image/jpeg",
            256, 256, _thumbnail_created_cb2, user_data);

/*
	g_object_unref (f);
	g_object_unref (r1);
	g_object_unref (r2);
#endif
*/
}

int
main(int argc, char **argv) 
{

	gtk_init (&argc, &argv);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	box = gtk_vbox_new (FALSE, 5);

	hbox = gtk_hbox_new (FALSE, 5);

	atext = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (atext), FALSE, TRUE, 0);
	btext = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (btext), FALSE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (hbox), FALSE, TRUE, 0);

	image = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (image), TRUE, TRUE, 0);

	imaget = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (imaget), TRUE, TRUE, 0);

	button = gtk_button_new_with_label ("Get album art");
	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (button), FALSE, TRUE, 0);

	g_signal_connect (G_OBJECT (button), "clicked", 
					  G_CALLBACK (on_button_clicked), imaget);

	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (box));
	gtk_widget_show_all (GTK_WIDGET (window));
	gtk_main();
}
