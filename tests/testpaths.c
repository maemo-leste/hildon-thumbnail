#include <stdio.h>

#include <hildon-albumart-factory.h>

#include <unistd.h>

int main ()
{
	/* don't use like this in software, you must free the result */

	g_print ("%s\n", hildon_albumart_get_path ("a", "b", "album"));
	g_print ("%s\n", hildon_albumart_get_path ("a", NULL, "album"));
	g_print ("%s\n", hildon_albumart_get_path (NULL, "b", "album"));
	g_print ("%s\n", hildon_albumart_get_path ("a", "b", NULL));

	g_print ("%s\n", hildon_albumart_get_path ("abc", "b", NULL));
	g_print ("%s\n", hildon_albumart_get_path ("a", "abc", NULL));
	g_print ("%s\n", hildon_albumart_get_path ("abc", "abc", NULL));

	g_print ("%s\n", hildon_albumart_get_path ("abc", "b", "album"));
	g_print ("%s\n", hildon_albumart_get_path ("a", "abc", "album"));
	g_print ("%s\n", hildon_albumart_get_path ("abc", "abc", "album"));

}

