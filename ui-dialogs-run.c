#include "ui-dialogs-run.h"
#include "config.h"
#include <dvbrecorder/dvb-scanner.h>
#include <dvbrecorder/scheduled.h>

GtkResponseType ui_dialog_scan_show(GtkWidget *parent, DVBRecorder *recorder)
{
    GtkWidget *dialog = ui_dialog_scan_new(GTK_WINDOW(parent));
    g_object_set(G_OBJECT(dialog), "recorder", recorder, NULL);

    GtkResponseType result = gtk_dialog_run(GTK_DIALOG(dialog));
    DVBScanner *scanner = NULL;
    g_object_get(G_OBJECT(dialog), "scanner", &scanner, NULL);

    dvb_scanner_stop(scanner);
    if (result == GTK_RESPONSE_OK) {
        dvb_scanner_update_channels_db(scanner);
    }

    gtk_widget_destroy(dialog);

    return result;
}

void favourites_dialog_show(GtkWidget *parent, DVBRecorder *recorder, guint32 list_id,
                            CHANNEL_FAVOURITES_DIALOG_UPDATE_NOTIFY notify_cb, gpointer userdata)
{
    GtkWidget *dialog = channel_favourites_dialog_new(GTK_WINDOW(parent));
    g_object_set(G_OBJECT(dialog), "recorder", recorder, NULL);
    channel_favourites_dialog_set_update_notify(CHANNEL_FAVOURITES_DIALOG(dialog), notify_cb, userdata);

    fprintf(stderr, "set list id: %u\n", list_id);
    channel_favourites_dialog_set_current_list(CHANNEL_FAVOURITES_DIALOG(dialog), list_id);

    GtkResponseType result = gtk_dialog_run(GTK_DIALOG(dialog));

    if (result == GTK_RESPONSE_APPLY) {
        if (channel_favourites_dialog_write_favourite_lists(CHANNEL_FAVOURITES_DIALOG(dialog))
                && notify_cb) {
            notify_cb(userdata);
        }
    }

    if (GTK_IS_DIALOG(dialog))
        gtk_widget_destroy(dialog);
}

void ui_recorder_settings_dialog_show(GtkWidget *parent, DVBRecorder *recorder)
{
    GtkWidget *dialog = ui_recorder_settings_dialog_new(GTK_WINDOW(parent));
    g_object_set(G_OBJECT(dialog), "recorder", recorder, NULL);

    GtkResponseType result = gtk_dialog_run(GTK_DIALOG(dialog));

    if (result == GTK_RESPONSE_APPLY) {
        ui_recorder_settings_dialog_set_recorder_filter(UI_RECORDER_SETTINGS_DIALOG(dialog));
        config_set("dvb", "record-streams", CFG_TYPE_INT,
                GINT_TO_POINTER(dvb_recorder_get_record_filter(recorder)));
    }

    if (GTK_IS_DIALOG(dialog))
        gtk_widget_destroy(dialog);
}

void video_settings_dialog_show(GtkWidget *parent, VideoOutput *vo)
{
    GtkWidget *dialog = video_settings_dialog_new(GTK_WINDOW(parent));
    g_object_set(G_OBJECT(dialog), "video-output", vo, NULL);

    GtkResponseType result = gtk_dialog_run(GTK_DIALOG(dialog));

    gint brightness = 0, contrast = 0, hue = 0, saturation = 0;

    config_get("video", "brightness", CFG_TYPE_INT, &brightness);
    config_get("video", "contrast", CFG_TYPE_INT, &contrast);
    config_get("video", "hue", CFG_TYPE_INT, &hue);
    config_get("video", "saturation", CFG_TYPE_INT, &saturation);

    if (result == GTK_RESPONSE_APPLY) {
        g_object_get(G_OBJECT(dialog),
                     "brightness", &brightness,
                     "contrast", &contrast,
                     "hue", &hue,
                     "saturation", &saturation,
                     NULL);
        config_set("video", "brightness", CFG_TYPE_INT, GINT_TO_POINTER(brightness));
        config_set("video", "contrast", CFG_TYPE_INT, GINT_TO_POINTER(contrast));
        config_set("video", "hue", CFG_TYPE_INT, GINT_TO_POINTER(hue));
        config_set("video", "saturation", CFG_TYPE_INT, GINT_TO_POINTER(saturation));
    }
    else {
        /* reset values */
        if (vo) {
            video_output_set_brightness(vo, brightness);
            video_output_set_contrast(vo, contrast);
            video_output_set_hue(vo, hue);
            video_output_set_saturation(vo, saturation);
        }
    }

    if (GTK_IS_DIALOG(dialog))
        gtk_widget_destroy(dialog);
}

void ui_add_scheduled_event_dialog_show(GtkWidget *parent, DVBRecorder *recorder)
{
    GtkWidget *dialog = ui_add_scheduled_event_dialog_new(GTK_WINDOW(parent));

    GtkResponseType result = gtk_dialog_run(GTK_DIALOG(dialog));

    if (result == GTK_RESPONSE_ACCEPT) {
        /* get event */
        ScheduledEvent *event = NULL;
        g_object_get(G_OBJECT(dialog), "scheduled-event", &event, NULL);
        fprintf(stderr, "add new scheduled event\n");

        if (event) {
            scheduled_event_add(recorder, event->channel_id, event->time_start, event->time_end);
        }
    }

    if (GTK_IS_DIALOG(dialog))
        gtk_widget_destroy(dialog);
}

void ui_scheduled_events_dialog_show(GtkWidget *parent, DVBRecorder *recorder)
{
    GtkWidget *dialog = ui_scheduled_events_dialog_new(GTK_WINDOW(parent));
    g_object_set(G_OBJECT(dialog), "recorder", recorder, NULL);

    gtk_dialog_run(GTK_DIALOG(dialog));

    if (GTK_IS_DIALOG(dialog))
        gtk_widget_destroy(dialog);
}

