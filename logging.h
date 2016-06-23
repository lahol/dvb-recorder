#pragma once

#include <glib.h>

void log_log(gchar *format, ...);
void log_logger_cb(gchar *buffer, gpointer userdata);

#define LOG(fmt, ...) log_log(fmt, ##__VA_ARGS__)
