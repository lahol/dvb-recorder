#include "config.h"
#include <stdio.h>
#include "commands.h"

GKeyFile *_config_keyfile;

/*static GOptionEntry _cmd_line_options[] = {
    { NULL }
};*/

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

gint config_save(gchar *conffile)
{
    return g_key_file_save_to_file(_config_keyfile, conffile, NULL) ? 0 : 1;
}

void config_free(void)
{
    g_key_file_free(_config_keyfile);
}

gint config_get(gchar *group, gchar *key, CfgType type, gpointer value)
{
    if (!g_key_file_has_key(_config_keyfile, group, key, NULL))
        return 1;
    GError *err = NULL;
    switch (type) {
        case CFG_TYPE_STRING:
            *((gchar **)value) = g_key_file_get_string(_config_keyfile, group, key, &err);
            break;
        case CFG_TYPE_INT:
            *((gint *)value) = g_key_file_get_integer(_config_keyfile, group, key, &err);
            break;
        case CFG_TYPE_BOOLEAN:
            *((gboolean *)value) = g_key_file_get_boolean(_config_keyfile, group, key, &err);
            if (err && err->code == G_KEY_FILE_ERROR_KEY_NOT_FOUND) {
                g_error_free(err);
                err = NULL;
            }
            break;
    }
    if (err != NULL) {
        fprintf(stderr, "config_get: %s\n", err->message);
        g_error_free(err);
        return 1;
    }
    return 0;
}

void config_set(gchar *group, gchar *key, CfgType type, gpointer value)
{
    switch (type) {
        case CFG_TYPE_STRING:
            g_key_file_set_string(_config_keyfile, group, key, (const gchar *)value);
            break;
        case CFG_TYPE_INT:
            g_key_file_set_integer(_config_keyfile, group, key, GPOINTER_TO_INT(value));
            break;
        case CFG_TYPE_BOOLEAN:
            g_key_file_set_boolean(_config_keyfile, group, key, (gboolean)GPOINTER_TO_INT(value));
            break;
    }
}

void config_enum_bindings(CfgEnumBindingProc binding_cb, gpointer userdata)
{
    if (binding_cb == NULL || _config_keyfile == NULL)
        return;
    gchar **groups = g_key_file_get_groups(_config_keyfile, NULL);
    gchar **keys = NULL;
    CmdMode mode;
    gchar *action;
    guint i, j;

    for (i = 0; groups[i]; ++i) {
        if (!g_str_has_prefix(groups[i], "binding:"))
            continue;
        mode = cmd_mode_from_string(&groups[i][8]);
        if (mode == CMD_MODE_INVALID)
            continue;

        keys = g_key_file_get_keys(_config_keyfile, groups[i], NULL, NULL);

        for (j = 0; keys[j]; ++j) {
            action = g_key_file_get_string(_config_keyfile, groups[i], keys[j], NULL);
            binding_cb(mode, action, keys[j], userdata);
            g_free(action);
        }

        g_strfreev(keys);
    }

    g_strfreev(groups);
}
