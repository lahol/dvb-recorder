#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UI_SCHEDULED_EVENTS_DIALOG_TYPE (ui_scheduled_events_dialog_get_type())
#define UI_SCHEDULED_EVENTS_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), UI_SCHEDULED_EVENTS_DIALOG_TYPE, UiScheduledEventsDialog))
#define IS_UI_SCHEDULED_EVENTS_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), UI_SCHEDULED_EVENTS_DIALOG_TYPE))
#define UI_SCHEDULED_EVENTS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), UI_SCHEDULED_EVENTS_DIALOG_TYPE, UiScheduledEventsDialogClass))
#define IS_UI_SCHEDULED_EVENTS_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), UI_SCHEDULED_EVENTS_DIALOG_TYPE))
#define UI_SCHEDULED_EVENTS_DIALOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), UI_SCHEDULED_EVENTS_DIALOG_TYPE, UiScheduledEventsDialogClass))

typedef struct _UiScheduledEventsDialog UiScheduledEventsDialog;
typedef struct _UiScheduledEventsDialogClass UiScheduledEventsDialogClass;

struct _UiScheduledEventsDialog {
    GtkDialog parent_instance;
};

struct _UiScheduledEventsDialogClass {
    GtkDialogClass parent_class;
};

GType ui_scheduled_events_dialog_get_type(void) G_GNUC_CONST;

GtkWidget *ui_scheduled_events_dialog_new(GtkWindow *parent);

G_END_DECLS
