#ifndef __IPC_H__
#define __IPC_H__

#include <glib.h>

typedef void (*IPCCommandFunc)(gchar *, gpointer);

gboolean ipc_init(gchar *fifoname, IPCCommandFunc callback, gpointer userdata);
void ipc_set_command_cb(IPCCommandFunc callback, gpointer userdata);
void ipc_cleanup(void);

#endif
