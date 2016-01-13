#pragma once

#include <glib.h>

typedef enum {
    CFG_TYPE_STRING,
    CFG_TYPE_INT
} CfgType;

gint config_read_cmdline(int *pargc, char ***pargv);
gint config_load(gchar *conffile);
void config_free(void);
gint config_get(gchar *group, gchar *key, CfgType type, gpointer value);
