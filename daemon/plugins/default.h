#ifndef __DEFAULT_H__
#define __DEFAULT_H__

void image_png_create (GObject *object, GStrv urls, DBusGMethodInvocation *context);
void image_png_move (GObject *object, GStrv from_urls, GStrv to_urls, DBusGMethodInvocation *context);
void image_png_delete (GObject *object, GStrv urls, DBusGMethodInvocation *context);

#endif
