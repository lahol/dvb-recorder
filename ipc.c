#include "ipc.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

gboolean ipc_io_incoming(GIOChannel *source, GIOCondition cond, gpointer data);

gchar *ipc_fifo_location = NULL;
GIOChannel *ipc_io_channel = NULL;
IPCCommandFunc ipc_command_cb = NULL;
gpointer ipc_command_cb_data = NULL;
guint ipc_watch_source_id = 0;

gboolean ipc_init(gchar *fifoname, IPCCommandFunc callback, gpointer userdata)
{
    if (fifoname == NULL || fifoname[0] == 0)
        return FALSE;

    ipc_set_command_cb(callback, userdata);

    ipc_fifo_location = g_strdup(fifoname);
    if (mkfifo(ipc_fifo_location, 00666) != 0) {
        ipc_cleanup();
        return FALSE;
    }

    int fd = open(ipc_fifo_location, O_NONBLOCK | O_RDWR);

    if (fd < 0 || (ipc_io_channel = g_io_channel_unix_new(fd)) == NULL) {
        ipc_cleanup();
        return FALSE;
    }

    g_io_add_watch(ipc_io_channel, G_IO_IN, ipc_io_incoming, NULL);

    return TRUE;
}

void ipc_set_command_cb(IPCCommandFunc callback, gpointer userdata)
{
    ipc_command_cb = callback;
    ipc_command_cb_data = userdata;
}

void ipc_cleanup(void)
{
    if (ipc_watch_source_id > 0) {
        g_source_remove(ipc_watch_source_id);
        ipc_watch_source_id = 0;
    }
    if (ipc_io_channel) {
        g_io_channel_shutdown(ipc_io_channel, TRUE, NULL);
        g_io_channel_unref(ipc_io_channel);

        ipc_io_channel = NULL;
    }
    if (ipc_fifo_location) {
        unlink(ipc_fifo_location);
        g_free(ipc_fifo_location);
        ipc_fifo_location = NULL;
    }
}

gboolean ipc_io_incoming(GIOChannel *source, GIOCondition cond, gpointer data)
{
    if (cond != G_IO_IN) {
        fprintf(stderr, "error in incoming condition: %d\n", cond);
        return FALSE;
    }

    gchar *buffer = NULL;
    GIOStatus status = g_io_channel_read_line(source, &buffer, NULL, NULL, NULL);
    if (status != G_IO_STATUS_NORMAL) {
        fprintf(stderr, "error reading channel line: %d\n", status);
        return FALSE;
    }

    if (ipc_command_cb) {
        ipc_command_cb(buffer, ipc_command_cb_data);
    }

    g_free(buffer);

    return TRUE;
}
