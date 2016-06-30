#include "commands.h"
#include "logging.h"
#include <gtk/gtk.h>

struct _CmdAction {
    GQuark action_quark;

    CmdCallbackProc callback;
    gpointer user_data;
};

struct _Command {
    guint keyval;
    GdkModifierType modifiers;
    /* take pointer to action and ref*/
    CmdAction *action;
    guint mode;

    /* cmd-specific userdata? */
};

/* use GTree instead? */
GList *cmd_actions = NULL;
GList *cmd_commands = NULL;

void cmd_free(Command *cmd)
{
    if (cmd) {
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
    if (a->mode != CMD_MODE_ANY) {
        if (a->mode < b->mode)
            return -1;
        if (a->mode > b->mode)
            return 1;
    }

    return 0;
}

Command *cmd_add(const gchar *action_name, const gchar *accelerator)
{
    guint keyval;
    GdkModifierType modifiers;
    gtk_accelerator_parse(accelerator, &keyval, &modifiers);
    if (keyval == 0 && modifiers == 0)
        return NULL;

    /* FIXME: action_name in the form mode::action or action */
    CmdAction *action = cmd_action_get(action_name);
    if (!action) {
        LOG("action %s not found. Not creating command %s\n", action_name, accelerator);
        return NULL;
    }

    Command *cmd = cmd_find(CMD_MODE_ANY, keyval, modifiers);
    if (cmd) {
        LOG("command for %s, overwriting it.\n", accelerator);
        cmd_commands = g_list_remove(cmd_commands, cmd);
    }
    else {
        cmd = g_malloc0(sizeof(Command));
    }

    cmd->keyval = keyval;
    cmd->modifiers = modifiers;
    cmd->mode = CMD_MODE_ANY;
    cmd->action = action;

    cmd_commands = g_list_insert_sorted(cmd_commands, cmd, (GCompareFunc)cmd_compare_keys);

    return cmd;
}

Command *cmd_find(CmdMode mode, guint keyval, GdkModifierType modifiers)
{
    Command compare;
    compare.keyval = keyval;
    compare.modifiers = modifiers;
    compare.mode = mode;

    GList *entry = g_list_find_custom(cmd_commands, &compare, (GCompareFunc)cmd_compare_keys);
    return (entry ? (Command *)entry->data : NULL);
}

void cmd_run(Command *cmd)
{
    if (cmd && cmd->action && cmd->action->callback)
        cmd->action->callback(cmd->action->user_data);
}

void cmd_clear_list(void)
{
    g_list_free_full(cmd_commands, (GDestroyNotify)cmd_free);
    cmd_commands = NULL;
}

CmdAction *cmd_action_register(const gchar *action_name, CmdCallbackProc command_proc, gpointer user_data)
{
    CmdAction *action = cmd_action_get(action_name);
    if (G_UNLIKELY(action)) {
        LOG("action %s already registered, overwriting.\n", action_name);
    }
    else {
        action = g_malloc0(sizeof(CmdAction));
        action->action_quark = g_quark_from_string(action_name);

        cmd_actions = g_list_prepend(cmd_actions, action);
    }

    action->callback = command_proc;
    action->user_data = user_data;

    return action;
}

gint cmd_action_compare_quark(const CmdAction *action, const GQuark quark)
{
    if (action && action->action_quark == quark)
        return 0;
    return 1;
}

CmdAction *cmd_action_get(const gchar *action_name)
{
    GQuark action_quark = g_quark_from_string(action_name);

    GList *entry = g_list_find_custom(cmd_actions, (gconstpointer)action_quark, (GCompareFunc)cmd_action_compare_quark);

    return (entry ? (CmdAction *)entry->data : NULL);
}

void cmd_action_cleanup(void)
{
    g_list_free_full(cmd_actions, (GDestroyNotify)g_free);
    cmd_actions = NULL;
}
