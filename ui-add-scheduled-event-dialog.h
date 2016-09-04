#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UI_ADD_SCHEDULED_EVENT_DIALOG_TYPE (ui_add_scheduled_event_dialog_get_type())
#define UI_ADD_SCHEDULED_EVENT_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), UI_ADD_SCHEDULED_EVENT_DIALOG_TYPE, UiAddScheduledEventDialog))
#define IS_UI_ADD_SCHEDULED_EVENT_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), UI_ADD_SCHEDULED_EVENT_DIALOG_TYPE))
#define UI_ADD_SCHEDULED_EVENT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), UI_ADD_SCHEDULED_EVENT_DIALOG_TYPE, UiAddScheduledEventDialogClass))
#define IS_UI_ADD_SCHEDULED_EVENT_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), UI_ADD_SCHEDULED_EVENT_DIALOG_TYPE))
#define UI_ADD_SCHEDULED_EVENT_DIALOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), UI_ADD_SCHEDULED_EVENT_DIALOG_TYPE, UiAddScheduledEventDialogClass))

typedef struct _UiAddScheduledEventDialog UiAddScheduledEventDialog;
typedef struct _UiAddScheduledEventDialogPrivate UiAddScheduledEventDialogPrivate;
typedef struct _UiAddScheduledEventDialogClass UiAddScheduledEventDialogClass;

struct _UiAddScheduledEventDialog {
    GtkDialog parent_instance;

    /*< private >*/
    UiAddScheduledEventDialogPrivate *priv;
};

struct _UiAddScheduledEventDialogClass {
    GtkDialogClass parent_class;
};

GType ui_add_scheduled_event_dialog_get_type(void) G_GNUC_CONST;

GtkWidget *ui_add_scheduled_event_dialog_new(GtkWindow *parent);

G_END_DECLS
