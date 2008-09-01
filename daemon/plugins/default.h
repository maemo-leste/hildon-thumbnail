#ifndef __DEFAULT_H__
#define __DEFAULT_H__

typedef struct ImagePng ImagePng;
typedef struct ImagePngClass ImagePngClass;

struct ImagePng {
	GObject parent;
};

struct ImagePngClass {
	GObjectClass parent;
};

void image_png_create (ImagePng *object, GStrv urls, DBusGMethodInvocation *context);
void image_png_move (ImagePng *object, GStrv from_urls, GStrv to_urls, DBusGMethodInvocation *context);
void image_png_delete (ImagePng *object, GStrv urls, DBusGMethodInvocation *context);

#endif
