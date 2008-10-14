#include <gtk/gtk.h>
#include <hildon-albumart-factory.h>

GtkWindow *window;
GtkHBox *box;
GtkVBox *hbox;
GtkImage *image;
GtkEntry *atext;
GtkEntry *btext;
GtkButton *button;


static void
on_art_back (HildonAlbumartFactoryHandle handle, gpointer user_data, GdkPixbuf *albumart, GError *error)
{
	if (albumart) {
		gtk_image_set_from_pixbuf (image, albumart);
	}
}

static void
on_button_clicked (GtkButton *button, gpointer user_data)
{
	gchar *album, *artist;

	album = gtk_entry_get_text (btext);
	artist = gtk_entry_get_text (atext);

	hildon_albumart_factory_load(artist, album, "album", on_art_back, NULL);
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

	button = gtk_button_new_with_label ("Get album art");
	gtk_box_pack_start (GTK_BOX (box), GTK_WIDGET (button), FALSE, TRUE, 0);

	g_signal_connect (G_OBJECT (button), "clicked", 
					  G_CALLBACK (on_button_clicked), NULL);

	gtk_container_add (GTK_CONTAINER (window), GTK_WIDGET (box));
	gtk_widget_show_all (GTK_WIDGET (window));
	gtk_main();
}
