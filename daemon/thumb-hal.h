#ifndef __THUMB_HAL_H__
#define __THUMB_HAL_H__

#include <glib-object.h>
#include <gio/gio.h>

#include <string.h>
#include <ctype.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <dbus/dbus-glib-bindings.h>

#include "thumbnailer.h"

G_BEGIN_DECLS

void thumb_hal_init (Thumbnailer *thumbnailer);
void thumb_hal_shutdown (void);

G_END_DECLS

#endif
