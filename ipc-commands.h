#ifndef __COMMANDS_H__
#define __COMMANDS_H__

#include <glib.h>

typedef gint (*CmdHandler)(gint, gchar **);

void ipc_cmd_init(void);
void ipc_cmd_cleanup(void);

void ipc_cmd_handle_command_line(gchar *line, gpointer userdata);
gint ipc_cmd_run_command(gint argc, gchar **argv);

void ipc_cmd_add_command(gchar *command, CmdHandler handler);

#endif
