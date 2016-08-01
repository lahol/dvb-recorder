#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UI_CHANNEL_PROPERTIES_DIALOG_TYPE (ui_channel_properties_dialog_get_type())
#define UI_CHANNEL_PROPERTIES_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), UI_CHANNEL_PROPERTIES_DIALOG_TYPE, UiChannelPropertiesDialog))
#define IS_UI_CHANNEL_PROPERTIES_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), UI_CHANNEL_PROPERTIES_DIALOG_TYPE))
#define UI_CHANNEL_PROPERTIES_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), UI_CHANNEL_PROPERTIES_DIALOG_TYPE, UiChannelPropertiesDialogClass))
#define IS_UI_CHANNEL_PROPERTIES_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), UI_CHANNEL_PROPERTIES_DIALOG_TYPE))
#define UI_CHANNEL_PROPERTIES_DIALOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), UI_CHANNEL_PROPERTIES_DIALOG_TYPE, UiChannelPropertiesDialogClass))

typedef struct _UiChannelPropertiesDialog UiChannelPropertiesDialog;
typedef struct _UiChannelPropertiesDialogPrivate UiChannelPropertiesDialogPrivate;
typedef struct _UiChannelPropertiesDialogClass UiChannelPropertiesDialogClass;

struct _UiChannelPropertiesDialog {
    GtkDialog parent_instance;

    /*< private >*/
    UiChannelPropertiesDialogPrivate *priv;
};

struct _UiChannelPropertiesDialogClass {
    GtkDialogClass parent_class;
};

GType ui_channel_properties_dialog_get_type(void) G_GNUC_CONST;

GtkWidget *ui_channel_properties_dialog_new(GtkWindow *parent);

void ui_channel_properties_dialog_set_channel_id(UiChannelPropertiesDialog *dialog, guint32 channel_id);
guint32 ui_channel_properties_dialog_get_channel_id(UiChannelPropertiesDialog *dialog);

G_END_DECLS
