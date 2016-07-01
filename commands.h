#pragma once

#include <gdk/gdk.h>

typedef struct _Command Command;
typedef struct _CmdAction CmdAction;

typedef void (*CmdCallbackProc)(gpointer);

typedef enum {
    CMD_MODE_INVALID = -1,
    CMD_MODE_ANY = 0,
    CMD_MODE_NORMAL,
    CMD_MODE_FULLSCREEN
} CmdMode;

Command *cmd_add(CmdMode mode, const gchar *action, const gchar *accelerator);
Command *cmd_find(CmdMode mode, guint keyval, GdkModifierType modifiers);
void cmd_run(Command *cmd);
void cmd_clear_list(void);

CmdAction *cmd_action_register(const gchar *action_name, CmdCallbackProc command_proc, gpointer user_data);
CmdAction *cmd_action_get(const gchar *action_name);
void cmd_action_cleanup(void);

CmdMode cmd_mode_from_string(const gchar *mode_str);
gchar *cmd_mode_to_string(CmdMode mode);
