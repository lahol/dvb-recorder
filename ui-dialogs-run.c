#include "ui-dialogs-run.h"
#include "config.h"
#include <dvbrecorder/dvb-scanner.h>

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


