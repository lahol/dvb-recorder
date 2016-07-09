#pragma once

#include <glib.h>

gboolean util_write_data_to_png(const gchar *filename, guint8 *buffer, guint width, guint height, guint bpp);

gchar *util_translate_control_codes_to_markup(gchar *string);

void util_time_to_string(gchar *buffer, gsize buflen, time_t starttime, gboolean short_format);
void util_duration_to_string(gchar *buffer, gsize buflen, guint32 seconds);
void util_duration_to_string_iso(gchar *buffer, gsize buflen, guint32 seconds);

