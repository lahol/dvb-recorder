#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "logging.h"
#include "config.h"
#include <time.h>

gchar log_filename[1024];
gboolean initalized = FALSE;

void log_reinit(void)
{
    gchar *fname = NULL;
    if (config_get("main", "logfile", CFG_TYPE_STRING, &fname) != 0) {
        fname = g_build_filename(
                g_get_user_data_dir(),
                "dvb-recorder.log",
                NULL);
    }

    strncpy(log_filename, fname, 1024);
    initalized = TRUE;

    g_free(fname);

}

void log_log(gchar *format, ...)
{
    if (!initalized)
        log_reinit();

    if (log_filename[0] == 0)
        return;

    FILE *f;

    if ((f = fopen(log_filename, "a")) == NULL)
        return;

    time_t t;
    static char buf[64];
    static struct tm bdt, *pdt = NULL;

    time(&t);
    localtime_r(&t, &bdt);
    pdt = &bdt;

    strftime(buf, 63, "[%Y%m%d-%H%M%S] ", pdt);
    fputs(buf, f);

    va_list args;
    va_start(args, format);
    vfprintf(f, format, args);
    va_end(args);

    fclose(f);
}

void log_logger_cb(gchar *string, gpointer userdata)
{
    log_log(string);
}
