#include "ui-add-scheduled-event-dialog.h"
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <dvbrecorder/scheduled.h>
#include "channel-list.h"
#include <dvbrecorder/channel-db.h>
#include "logging.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

struct _UiAddScheduledEventDialogPrivate {
    /* private data */
    GtkWindow *parent;

    GtkWidget *channel_list;
    GtkWidget *date_select;
    GtkWidget *time_edit;
    GtkWidget *duration_edit;

    ScheduledEvent event;
    guint validation_flags;
};

enum {
    VALID_FLAG_CHANNEL  = (1 << 0),
    VALID_FLAG_DATE     = (1 << 1),
    VALID_FLAG_TIME     = (1 << 2),
    VALID_FLAG_DURATION = (1 << 3),
    VALID_FLAG_ALL      = 0x0f
};

G_DEFINE_TYPE(UiAddScheduledEventDialog, ui_add_scheduled_event_dialog, GTK_TYPE_DIALOG);

enum {
    PROP_0,
    PROP_PARENT,
    PROP_SCHEDULED_EVENT,
    N_PROPERTIES
};

void ui_add_scheduled_event_dialog_set_parent(UiAddScheduledEventDialog *dialog, GtkWindow *parent);
void ui_add_scheduled_event_dialog_set_event(UiAddScheduledEventDialog *dialog, ScheduledEvent *event);
void ui_add_scheduled_event_dialog_update_event(UiAddScheduledEventDialog *dialog);
static gboolean ui_add_scheduled_events_dialog_validate(UiAddScheduledEventDialog *dialog);

static void ui_add_scheduled_event_dialog_dispose(GObject *gobject)
{
    /*UiAddScheduledEventDialog *self = UI_ADD_SCHEDULED_EVENT_DIALOG(gobject);*/

    G_OBJECT_CLASS(ui_add_scheduled_event_dialog_parent_class)->dispose(gobject);
}

static void ui_add_scheduled_event_dialog_finalize(GObject *gobject)
{
    /*UiAddScheduledEventDialog *self = UI_ADD_SCHEDULED_EVENT_DIALOG(gobject);*/

    G_OBJECT_CLASS(ui_add_scheduled_event_dialog_parent_class)->finalize(gobject);
}

static void ui_add_scheduled_event_dialog_set_property(GObject *object, guint prop_id, 
        const GValue *value, GParamSpec *spec)
{
    UiAddScheduledEventDialog *self = UI_ADD_SCHEDULED_EVENT_DIALOG(object);

    switch (prop_id) {
        case PROP_PARENT:
            ui_add_scheduled_event_dialog_set_parent(self, GTK_WINDOW(g_value_get_object(value)));
            break;
        case PROP_SCHEDULED_EVENT:
            ui_add_scheduled_event_dialog_set_event(self, g_value_get_pointer(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void ui_add_scheduled_event_dialog_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *spec)
{
    UiAddScheduledEventDialog *self = UI_ADD_SCHEDULED_EVENT_DIALOG(object);

    switch (prop_id) {
        case PROP_PARENT:
            g_value_set_object(value, self->priv->parent);
            break;
        case PROP_SCHEDULED_EVENT:
            ui_add_scheduled_event_dialog_update_event(self);
            g_value_set_pointer(value, &self->priv->event);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void ui_add_scheduled_event_dialog_class_init(UiAddScheduledEventDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    /* override GObject methods */
    gobject_class->dispose = ui_add_scheduled_event_dialog_dispose;
    gobject_class->finalize = ui_add_scheduled_event_dialog_finalize;
    gobject_class->set_property = ui_add_scheduled_event_dialog_set_property;
    gobject_class->get_property = ui_add_scheduled_event_dialog_get_property;

    g_object_class_install_property(gobject_class,
            PROP_PARENT,
            g_param_spec_object("parent",
                "Parent",
                "Parent",
                GTK_TYPE_WINDOW,
                G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
            PROP_SCHEDULED_EVENT,
            g_param_spec_pointer("scheduled-event",
                "ScheduledEvent",
                "Scheduled Event",
                G_PARAM_READWRITE));

    g_type_class_add_private(G_OBJECT_CLASS(klass), sizeof(UiAddScheduledEventDialogPrivate));
}

/*static void _ui_add_scheduled_event_dialog_cursor_changed(UiAddScheduledEventDialog *self, GtkTreeView *tree_view)*/
static void ui_add_scheduled_event_dialog_changed(UiAddScheduledEventDialog *self)
{
    gtk_dialog_set_response_sensitive(GTK_DIALOG(self),
                                      GTK_RESPONSE_ACCEPT,
                                      ui_add_scheduled_events_dialog_validate(self));
}

/*static void _ui_add_scheduled_event_dialog_editable_changed(UiAddScheduledEventDialog *self, GtkEditable *editable)
{
}*/

static void populate_widget(UiAddScheduledEventDialog *self)
{
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(self));

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), FALSE);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), FALSE);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 3);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 3);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 3);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);

    self->priv->channel_list = channel_list_new(TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), self->priv->channel_list, TRUE, TRUE, 0);

    GtkTreeView *tv = channel_list_get_tree_view(CHANNEL_LIST(self->priv->channel_list));
    gtk_tree_view_set_activate_on_single_click(tv, TRUE);
    g_signal_connect_swapped(G_OBJECT(tv), "cursor-changed",
            G_CALLBACK(ui_add_scheduled_event_dialog_changed), self);

    self->priv->date_select = gtk_calendar_new();
    gtk_grid_attach(GTK_GRID(grid), self->priv->date_select, 0, 0, 2, 1);

    GtkWidget *label = gtk_label_new("Start:");
    gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);

    self->priv->time_edit = gtk_entry_new();
    g_signal_connect_swapped(G_OBJECT(self->priv->time_edit), "changed",
            G_CALLBACK(ui_add_scheduled_event_dialog_changed), self);
    gtk_grid_attach(GTK_GRID(grid), self->priv->time_edit, 1, 1, 1, 1);

    label = gtk_label_new("Duration:");
    gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1);

    self->priv->duration_edit = gtk_entry_new();
    g_signal_connect_swapped(G_OBJECT(self->priv->duration_edit), "changed",
            G_CALLBACK(ui_add_scheduled_event_dialog_changed), self);
    gtk_grid_attach(GTK_GRID(grid), self->priv->duration_edit, 1, 2, 1, 1);

    gtk_box_pack_start(GTK_BOX(hbox), grid, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(content_area), hbox);

    gtk_widget_show_all(content_area);

    gtk_dialog_add_buttons(GTK_DIALOG(self),
            _("_Add"), GTK_RESPONSE_ACCEPT,
            _("_Close"), GTK_RESPONSE_CLOSE,
            NULL);

    gtk_dialog_set_response_sensitive(GTK_DIALOG(self), GTK_RESPONSE_ACCEPT, FALSE);
}

static void ui_add_scheduled_event_dialog_init(UiAddScheduledEventDialog *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
            UI_ADD_SCHEDULED_EVENT_DIALOG_TYPE, UiAddScheduledEventDialogPrivate);

    populate_widget(self);

    channel_db_foreach(0, (CHANNEL_DB_FOREACH_CALLBACK)channel_list_fill_cb, self->priv->channel_list);
}

GtkWidget *ui_add_scheduled_event_dialog_new(GtkWindow *parent)
{
    return g_object_new(UI_ADD_SCHEDULED_EVENT_DIALOG_TYPE, "parent", parent, NULL);
}

void ui_add_scheduled_event_dialog_set_parent(UiAddScheduledEventDialog *dialog, GtkWindow *parent)
{
    dialog->priv->parent = parent;
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_modal(GTK_WINDOW(dialog), FALSE);
}

void ui_add_scheduled_event_dialog_set_event(UiAddScheduledEventDialog *dialog, ScheduledEvent *event)
{
    if (!event)
        return;

    dialog->priv->event = *event;

    channel_list_set_channel_selection(CHANNEL_LIST(dialog->priv->channel_list), event->channel_id);

    struct tm *tm = localtime((time_t *)&event->time_start);
    g_object_set(G_OBJECT(dialog->priv->date_select),
            "day", tm->tm_mday,
            "month", tm->tm_mon,
            "year", tm->tm_year + 1900,
            NULL);

    gchar tbuf[32];

    sprintf(tbuf, "%02u:%02u", tm->tm_hour, tm->tm_min);
    gtk_entry_set_text(GTK_ENTRY(dialog->priv->time_edit), tbuf);

    sprintf(tbuf, "%u", (guint)((event->time_end - event->time_start)/60));
    gtk_entry_set_text(GTK_ENTRY(dialog->priv->duration_edit), tbuf);
}

void ui_add_scheduled_event_dialog_update_event(UiAddScheduledEventDialog *dialog)
{
    GtkTreeView *tv = channel_list_get_tree_view(CHANNEL_LIST(dialog->priv->channel_list));
    GtkTreePath *path = NULL;

    dialog->priv->validation_flags = 0;

    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_view_get_model(tv);
    gtk_tree_view_get_cursor(tv, &path, NULL);

    if (!path || model == NULL || !gtk_tree_model_get_iter(model, &iter, path)) {
        gtk_tree_path_free(path);
    }
    else {
        guint32 selection_id;

        gtk_tree_model_get(model, &iter, CHNL_ROW_ID, &selection_id, -1);

        gtk_tree_path_free(path);

        dialog->priv->event.channel_id = selection_id;

        dialog->priv->validation_flags |= VALID_FLAG_CHANNEL;
    }

    guint year, month, day;
    gtk_calendar_get_date(GTK_CALENDAR(dialog->priv->date_select), &year, &month, &day);
    dialog->priv->validation_flags |= VALID_FLAG_DATE;

    guint hour = 0, min = 0;
    const gchar *entry_text = gtk_entry_get_text(GTK_ENTRY(dialog->priv->time_edit));
    if (g_regex_match_simple("^([0-9]|0[0-9]|1[0-9]|2[0-3]):[0-5][0-9]$", entry_text, G_REGEX_RAW, 0)) {
        gchar **time_buf = g_strsplit(entry_text, ":", 0);
        if (time_buf[0]) {
            hour = strtoul(time_buf[0], NULL, 10);
            if (time_buf[1])
                min = strtoul(time_buf[1], NULL, 10);
        }

        g_strfreev(time_buf);
        dialog->priv->validation_flags |= VALID_FLAG_TIME;
    }

    entry_text = gtk_entry_get_text(GTK_ENTRY(dialog->priv->duration_edit));
    if (g_regex_match_simple("^[1-9][0-9]*$", entry_text, G_REGEX_RAW, 0)) {
        guint duration = strtoul(entry_text, NULL, 10);
        dialog->priv->validation_flags |= VALID_FLAG_DURATION;

        struct tm tm;
        tm.tm_sec = 0;
        tm.tm_min = min;
        tm.tm_hour = hour;
        tm.tm_mday = day;
        tm.tm_mon = month;
        tm.tm_year = year - 1900;
        tm.tm_wday = 0;
        tm.tm_yday = 0;
        tm.tm_isdst = -1;
        dialog->priv->event.time_start = (guint64)mktime(&tm);
        dialog->priv->event.time_end = dialog->priv->event.time_start + 60 * duration;
    }
}

static gboolean ui_add_scheduled_events_dialog_validate(UiAddScheduledEventDialog *dialog)
{
    ui_add_scheduled_event_dialog_update_event(dialog);

    guint conflicts = 0;
    if ((dialog->priv->validation_flags & VALID_FLAG_TIME) &&
        (dialog->priv->validation_flags & VALID_FLAG_DURATION)) {
        conflicts = scheduled_event_check_conflict(dialog->priv->event.time_start, dialog->priv->event.time_end);
    }
    fprintf(stderr, "number of conflicts: %u\n", conflicts);

    return (gboolean)(dialog->priv->validation_flags == VALID_FLAG_ALL);
}
