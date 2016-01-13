#include "config.h"
#include <stdio.h>

GKeyFile *_config_keyfile;

static GOptionEntry _cmd_line_options[] = {
    { NULL }
};

gint config_read_cmdline(int *pargc, char ***pargv)
{
    return 0;
}

gint config_load(gchar *conffile)
{
    fprintf(stderr, "read conffile\n");
    _config_keyfile = g_key_file_new();

    if (!g_key_file_load_from_file(_config_keyfile, conffile,
                G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, NULL))
        return 1;
    return 0;
}

void config_free(void)
{
    g_key_file_free(_config_keyfile);
}

gint config_get(gchar *group, gchar *key, CfgType type, gpointer value)
{
    GError *err = NULL;
    switch (type) {
        case CFG_TYPE_STRING:
            *((gchar **)value) = g_key_file_get_string(_config_keyfile, group, key, &err);
            break;
        case CFG_TYPE_INT:
            *((gint *)value) = g_key_file_get_integer(_config_keyfile, group, key, &err);
            break;
    }
    if (err != NULL) {
        fprintf(stderr, "config_get: %s\n", err->message);
        g_error_free(err);
        return 1;
    }
    return 0;
}
