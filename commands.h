#pragma once

#include <gdk/gdk.h>

typedef struct _Command Command;
typedef struct _CmdAction CmdAction;

typedef void (*CmdCallbackProc)(gpointer);

typedef enum {
    CMD_MODE_ANY = 0,
    CMD_MODE_NORMAL,
    CMD_MODE_FULLSCREEN
} CmdMode;

/* command_name mode::action or action for all */
Command *cmd_add(const gchar *action_name, const gchar *accelerator);
Command *cmd_find(CmdMode mode, guint keyval, GdkModifierType modifiers);
void cmd_run(Command *cmd);
void cmd_clear_list(void);

CmdAction *cmd_action_register(const gchar *action_name, CmdCallbackProc command_proc, gpointer user_data);
CmdAction *cmd_action_get(const gchar *action_name);
void cmd_action_cleanup(void);
