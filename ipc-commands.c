#include "ipc-commands.h"
#include <stdio.h>

GHashTable *commands = NULL;

void ipc_cmd_init(void)
{
    commands = g_hash_table_new(g_str_hash, g_str_equal);
}

void ipc_cmd_cleanup(void)
{
    if (commands) {
        g_hash_table_destroy(commands);
        commands = NULL;
    }
}

void ipc_cmd_add_command(gchar *command, CmdHandler handler)
{
    if (commands == NULL)
        return;

    g_hash_table_insert(commands, command, handler);
}

void ipc_cmd_handle_command_line(gchar *line, gpointer userdata)
{
    gint argc = 0;
    gchar **argv = NULL;

    if (!g_shell_parse_argv(line, &argc, &argv, NULL))
        return;

    if (argv == NULL || argv[0] == NULL)
        return;

    ipc_cmd_run_command(argc, argv);

    g_strfreev(argv);
}

gint ipc_cmd_run_command(gint argc, gchar **argv)
{
    if (commands == NULL || argv == NULL || argv[0] == NULL)
        return 1;
    CmdHandler handler = g_hash_table_lookup(commands, argv[0]);
    if (handler == NULL) {
        fprintf(stderr, "Command not found: %s\n", argv[0]);
        return 1;
    }
    return handler(argc, argv);
}
