#ifndef __GDKPIXBUF_IO_LOADER_PLUGIN_H__
#define __GDKPIXBUF_IO_LOADER_PLUGIN_H__

GdkPixbuf *
my_gdk_pixbuf_new_from_stream_at_scale (GInputStream  *stream,
				     gint	    width,
				     gint 	    height,
				     gboolean       preserve_aspect_ratio,
				     GCancellable  *cancellable,
		  	    	     GError       **error);

GdkPixbuf *
my_gdk_pixbuf_new_from_stream (GInputStream  *stream,
			    GCancellable  *cancellable,
			    guint          max_pix,
			    guint max_w, guint max_h,
			    GError       **error);

#endif
