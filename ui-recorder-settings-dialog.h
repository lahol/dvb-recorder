#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>
#include <dvbrecorder/dvbrecorder.h>

G_BEGIN_DECLS

#define UI_RECORDER_SETTINGS_DIALOG_TYPE (ui_recorder_settings_dialog_get_type())
#define UI_RECORDER_SETTINGS_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), UI_RECORDER_SETTINGS_DIALOG_TYPE, UiRecorderSettingsDialog))
#define IS_UI_RECORDER_SETTINGS_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), UI_RECORDER_SETTINGS_DIALOG_TYPE))
#define UI_RECORDER_SETTINGS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), UI_RECORDER_SETTINGS_DIALOG_TYPE, UiRecorderSettingsDialogClass))
#define IS_UI_RECORDER_SETTINGS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), UI_RECORDER_SETTINGS_DIALOG_TYPE))
#define UI_RECORDER_SETTINGS_DIALOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), UI_RECORDER_SETTINGS_DIALOG_TYPE, UiRecorderSettingsDialogClass))

typedef struct _UiRecorderSettingsDialog UiRecorderSettingsDialog;
typedef struct _UiRecorderSettingsDialogPrivate UiRecorderSettingsDialogPrivate;
typedef struct _UiRecorderSettingsDialogClass UiRecorderSettingsDialogClass;

struct _UiRecorderSettingsDialog {
    GtkDialog parent_instance;

    /*< private >*/
    UiRecorderSettingsDialogPrivate *priv;
};

struct _UiRecorderSettingsDialogClass {
    GtkDialogClass parent_class;
};

GType ui_recorder_settings_dialog_get_type(void) G_GNUC_CONST;

GtkWidget *ui_recorder_settings_dialog_new(GtkWindow *parent, DVBRecorder *recorder);

void ui_recorder_settings_dialog_set_parent(UiRecorderSettingsDialog *dialog, GtkWindow *parent);
void ui_recorder_settings_dialog_set_recorder(UiRecorderSettingsDialog *dialog, DVBRecorder *recorder);

void ui_recorder_settings_dialog_show(GtkWidget *parent, DVBRecorder *recorder);

G_END_DECLS
