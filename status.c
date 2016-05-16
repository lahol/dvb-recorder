#include "status.h"

void status_save_window_data(GKeyFile *kf, const gchar *group, GuiWindowStatus *win_status)
{
    if (!win_status->initialized)
        return;

    g_key_file_set_integer(kf, group, "x", win_status->x);
    g_key_file_set_integer(kf, group, "y", win_status->y);
    g_key_file_set_integer(kf, group, "width", win_status->width);
    g_key_file_set_integer(kf, group, "height", win_status->height);

    g_key_file_set_boolean(kf, group, "show", win_status->show_window);
    g_key_file_set_boolean(kf, group, "fullscreen", win_status->fullscreen);
}

void status_save_recorder_data(GKeyFile *kf, RecorderStatus *status)
{
    if (!status->initialized)
        return;

    g_key_file_set_uint64(kf, "Recorder", "channel-id", (guint64)status->channel_id);
    g_key_file_set_double(kf, "Recorder", "volume", status->volume);
    g_key_file_set_boolean(kf, "Recorder", "mute", status->mute);
    g_key_file_set_boolean(kf, "Recorder", "running", status->running);
}

void status_save(AppStatus *status, gchar *filename)
{
    if (!status || !filename)
        return;

    GKeyFile *kf = g_key_file_new();

    status_save_window_data(kf, "Window:main", &status->gui.main_window);
    status_save_window_data(kf, "Window:channels", &status->gui.channels_dialog);
    status_save_window_data(kf, "Window:epg", &status->gui.epg_dialog);

    status_save_recorder_data(kf, &status->recorder);

    g_key_file_save_to_file(kf, filename, NULL);

    g_key_file_free(kf);
}

void status_restore_window_data(GKeyFile *kf, const gchar *group, GuiWindowStatus *status)
{
    if (!g_key_file_has_group(kf, group)) {
        status->initialized = 0;
        return;
    }

    status->x = g_key_file_get_integer(kf, group, "x", NULL);
    status->y = g_key_file_get_integer(kf, group, "y", NULL);
    status->width = g_key_file_get_integer(kf, group, "width", NULL);
    status->height = g_key_file_get_integer(kf, group, "height", NULL);

    status->show_window = g_key_file_get_boolean(kf, group, "show", NULL);
    status->fullscreen = g_key_file_get_boolean(kf, group, "fullscreen", NULL);

    status->initialized = 1;
}

void status_restore_recorder_data(GKeyFile *kf, RecorderStatus *status)
{
    if (!g_key_file_has_group(kf, "Recorder")) {
        status->initialized = 0;
        return;
    }

    status->channel_id = (guint)g_key_file_get_uint64(kf, "Recorder", "channel-id", NULL);
    status->volume = g_key_file_get_double(kf, "Recorder", "volume", NULL);
    status->mute = g_key_file_get_boolean(kf, "Recorder", "mute", NULL);
    status->running = g_key_file_get_boolean(kf, "Recorder", "running", NULL);

    status->initialized = 1;
}

void status_restore(AppStatus *status, gchar *filename)
{
    if (!status || !filename)
        return;

    GKeyFile *kf = g_key_file_new();

    if (!g_key_file_load_from_file(kf, filename, G_KEY_FILE_NONE, NULL))
        goto done;

    status_restore_window_data(kf, "Window:main", &status->gui.main_window);
    status_restore_window_data(kf, "Window:channels", &status->gui.channels_dialog);
    status_restore_window_data(kf, "Window:epg", &status->gui.epg_dialog);

    status_restore_recorder_data(kf, &status->recorder);

done:
    g_key_file_free(kf);
}
