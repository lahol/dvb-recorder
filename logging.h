#pragma once

#include <glib.h>

void log_log(gchar *format, ...);
void log_logger_cb(gchar *buffer, gpointer userdata);

#define LOG(fmt, ...) log_log("[dvb-recorder] " __FILE__ ":%d " fmt, __LINE__, ##__VA_ARGS__)
