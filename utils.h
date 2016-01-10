#pragma once

#include <glib.h>

gboolean util_write_data_to_png(const gchar *filename, guint8 *buffer, guint width, guint height, guint bpp);

