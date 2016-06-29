#include "commands.h"
#include "logging.h"
#include <gtk/gtk.h>

struct _Command {
    guint keyval;
    GdkModifierType modifiers;
    CmdCallbackProc action;
    gpointer user_data;
    gchar *command_name;
    guint mode;
};

GList *cmd_commands = NULL;

void cmd_free(Command *cmd)
{
    if (cmd) {
        g_free(cmd->command_name);
        g_free(cmd);
    }
}

gint cmd_compare_keys(Command *a, Command *b)
{
    if (a->keyval < b->keyval)
        return -1;
    if (a->keyval > b->keyval)
        return 1;
    if (a->modifiers < b->modifiers)
        return -1;
    if (a->modifiers > b->modifiers)
        return 1;
    return 0;
}

Command *cmd_add(const gchar *command_name, const gchar *accelerator,
                 CmdCallbackProc command_proc, gpointer user_data)
{
    guint keyval;
    GdkModifierType modifiers;
    gtk_accelerator_parse(accelerator, &keyval, &modifiers);
    if (keyval == 0 && modifiers == 0)
        return NULL;

    Command *cmd = cmd_find(keyval, modifiers);
    if (cmd) {
        LOG("command for %s already exists: %s. Overwriting with %s.\n",
                accelerator, cmd->command_name, command_name);
        cmd_commands = g_list_remove(cmd_commands, cmd);
        g_free(cmd->command_name);
    }
    else {
        cmd = g_malloc0(sizeof(Command));
    }

    cmd->keyval = keyval;
    cmd->modifiers = modifiers;
    cmd->action = command_proc;
    cmd->user_data = user_data;
    cmd->command_name = g_strdup(command_name);

    cmd_commands = g_list_insert_sorted(cmd_commands, cmd, (GCompareFunc)cmd_compare_keys);

    return cmd;
}

Command *cmd_find(guint keyval, GdkModifierType modifiers)
{
    Command compare;
    compare.keyval = keyval;
    compare.modifiers = modifiers;

    GList *entry = g_list_find_custom(cmd_commands, &compare, (GCompareFunc)cmd_compare_keys);
    return (entry ? (Command *)entry->data : NULL);
}

void cmd_run(Command *cmd)
{
    if (cmd && cmd->action)
        cmd->action(cmd->user_data);
}

void cmd_clear_list(void)
{
    g_list_free_full(cmd_commands, (GDestroyNotify)cmd_free);
    cmd_commands = NULL;
}
