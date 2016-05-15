#pragma once

#include <glib.h>

gboolean util_write_data_to_png(const gchar *filename, guint8 *buffer, guint width, guint height, guint bpp);

gchar *util_translate_control_codes_to_markup(gchar *string);
