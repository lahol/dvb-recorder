#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>
#include <dvbrecorder/dvbrecorder.h>
#include "video-output.h"

#include "ui-dialog-scan.h"
#include "favourites-dialog.h"
#include "ui-recorder-settings-dialog.h"
#include "video-settings-dialog.h"
#include "ui-channel-properties-dialog.h"

GtkResponseType ui_dialog_scan_show(GtkWidget *parent, DVBRecorder *recorder);
void favourites_dialog_show(GtkWidget *parent, DVBRecorder *recorder, guint32 list_id,
                            CHANNEL_FAVOURITES_DIALOG_UPDATE_NOTIFY notify_cb, gpointer userdata);
void ui_recorder_settings_dialog_show(GtkWidget *parent, DVBRecorder *recorder);
void video_settings_dialog_show(GtkWidget *parent, VideoOutput *vo);

