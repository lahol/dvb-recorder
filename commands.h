#pragma once

#include <gdk/gdk.h>

typedef struct _Command Command;

typedef void (*CmdCallbackProc)(gpointer);

/* command_name mode::action or action for all */
Command *cmd_add(const gchar *command_name, const gchar *accelerator,
                 CmdCallbackProc command_proc, gpointer user_data);
Command *cmd_find(guint keyval, GdkModifierType modifiers);
void cmd_run(Command *cmd);
void cmd_clear_list(void);
