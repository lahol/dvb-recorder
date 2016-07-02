#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VIDEO_SETTINGS_DIALOG_TYPE (video_settings_dialog_get_type())
#define VIDEO_SETTINGS_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), VIDEO_SETTINGS_DIALOG_TYPE, VideoSettingsDialog))
#define IS_VIDEO_SETTINGS_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), VIDEO_SETTINGS_DIALOG_TYPE))
#define VIDEO_SETTINGS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), VIDEO_SETTINGS_DIALOG_TYPE, VideoSettingsDialogClass))
#define IS_VIDEO_SETTINGS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), VIDEO_SETTINGS_DIALOG_TYPE))
#define VIDEO_SETTINGS_DIALOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), VIDEO_SETTINGS_DIALOG_TYPE, VideoSettingsDialogClass))

typedef struct _VideoSettingsDialog VideoSettingsDialog;
typedef struct _VideoSettingsDialogPrivate VideoSettingsDialogPrivate;
typedef struct _VideoSettingsDialogClass VideoSettingsDialogClass;

struct _VideoSettingsDialog {
    GtkDialog parent_instance;

    /*< private >*/
    VideoSettingsDialogPrivate *priv;
};

struct _VideoSettingsDialogClass {
    GtkDialogClass parent_class;
};

GType video_settings_dialog_get_type(void) G_GNUC_CONST;

GtkWidget *video_settings_dialog_new(GtkWindow *parent);

void video_settings_dialog_set_parent(VideoSettingsDialog *dialog, GtkWindow *parent);

G_END_DECLS
