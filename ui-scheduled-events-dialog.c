#include "ui-scheduled-events-dialog.h"
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <dvbrecorder/dvbrecorder.h>
#include <dvbrecorder/scheduled.h>
#include <dvbrecorder/channel-db.h>
#include "ui-add-scheduled-event-dialog.h"
#include "ui-dialogs-run.h"
#include <time.h>

struct _UiScheduledEventsDialogPrivate {
    /* private data */
    GtkWindow *parent;
    DVBRecorder *recorder;

    GtkListStore *store;
    GtkWidget *tree_view;
};

G_DEFINE_TYPE(UiScheduledEventsDialog, ui_scheduled_events_dialog, GTK_TYPE_DIALOG);

enum {
    PROP_0,
    PROP_PARENT,
    PROP_RECORDER,
    N_PROPERTIES
};

enum {
    SE_ROW_EVENT_ID = 0,
    SE_ROW_CHNL_ID,
    SE_ROW_CHNL_TEXT,
    SE_ROW_START,
    SE_ROW_START_TEXT,
    SE_ROW_END,
    SE_ROW_END_TEXT,
    SE_N_ROWS
};

void ui_scheduled_events_dialog_set_parent(UiScheduledEventsDialog *dialog, GtkWindow *parent);
void ui_scheduled_events_dialog_update_list(UiScheduledEventsDialog *self);

static void ui_scheduled_events_dialog_dispose(GObject *gobject)
{
    /*UiScheduledEventsDialog *self = UI_SCHEDULED_EVENTS_DIALOG(gobject);*/

    G_OBJECT_CLASS(ui_scheduled_events_dialog_parent_class)->dispose(gobject);
}

static void ui_scheduled_events_dialog_finalize(GObject *gobject)
{
    /*UiScheduledEventsDialog *self = UI_SCHEDULED_EVENTS_DIALOG(gobject);*/

    G_OBJECT_CLASS(ui_scheduled_events_dialog_parent_class)->finalize(gobject);
}

static void ui_scheduled_events_dialog_set_property(GObject *object, guint prop_id, 
        const GValue *value, GParamSpec *spec)
{
    UiScheduledEventsDialog *self = UI_SCHEDULED_EVENTS_DIALOG(object);

    switch (prop_id) {
        case PROP_PARENT:
            ui_scheduled_events_dialog_set_parent(self, GTK_WINDOW(g_value_get_object(value)));
            break;
        case PROP_RECORDER:
            self->priv->recorder = g_value_get_pointer(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void ui_scheduled_events_dialog_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *spec)
{
    UiScheduledEventsDialog *self = UI_SCHEDULED_EVENTS_DIALOG(object);

    switch (prop_id) {
    	case PROP_PARENT:
            g_value_set_object(value, self->priv->parent);
            break;
        case PROP_RECORDER:
            g_value_set_pointer(value, self->priv->recorder);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void ui_scheduled_events_dialog_class_init(UiScheduledEventsDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    /* override GObject methods */
    gobject_class->dispose = ui_scheduled_events_dialog_dispose;
    gobject_class->finalize = ui_scheduled_events_dialog_finalize;
    gobject_class->set_property = ui_scheduled_events_dialog_set_property;
    gobject_class->get_property = ui_scheduled_events_dialog_get_property;

    g_object_class_install_property(gobject_class,
            PROP_PARENT,
            g_param_spec_object("parent",
                "Parent",
                "Parent",
                GTK_TYPE_WINDOW,
                G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
            PROP_RECORDER,
            g_param_spec_pointer("recorder",
                "Recorder",
                "Recorder",
                G_PARAM_READWRITE));

    g_type_class_add_private(G_OBJECT_CLASS(klass), sizeof(UiScheduledEventsDialogPrivate));
}

struct _UiScheduledEventsDialogContextData {
    UiScheduledEventsDialog *dialog;
    guint64 event_id;
};

static void _ui_scheduled_events_dialog_menu_add(struct _UiScheduledEventsDialogContextData *data)
{
    if (ui_add_scheduled_event_dialog_show(GTK_WIDGET(data->dialog), data->dialog->priv->recorder, 0))
        ui_scheduled_events_dialog_update_list(data->dialog);
}

static void _ui_scheduled_events_dialog_menu_edit(struct _UiScheduledEventsDialogContextData *data)
{
    if (ui_add_scheduled_event_dialog_show(GTK_WIDGET(data->dialog), data->dialog->priv->recorder, data->event_id))
        ui_scheduled_events_dialog_update_list(data->dialog);
}

static void _ui_scheduled_events_dialog_menu_remove(struct _UiScheduledEventsDialogContextData *data)
{
    fprintf(stderr, "remove %" G_GUINT64_FORMAT "\n", data->event_id);

    scheduled_event_remove(data->dialog->priv->recorder, data->event_id);
    ui_scheduled_events_dialog_update_list(data->dialog);
}

static void _ui_scheduled_events_dialog_menu_destroy(GtkWidget *menu, gpointer userdata)
{
    g_free(userdata);
}

static void _ui_scheduled_events_dialog_context_popup_menu(UiScheduledEventsDialog *self,
        GdkEventButton *event, guint64 event_id)
{
    GtkWidget *menu;
    GtkWidget *item;
    struct _UiScheduledEventsDialogContextData *data = g_malloc(sizeof(struct _UiScheduledEventsDialogContextData));

    data->dialog = self;
    data->event_id = event_id;

    menu = gtk_menu_new();
    g_signal_connect(G_OBJECT(menu), "selection-done",
            G_CALLBACK(_ui_scheduled_events_dialog_menu_destroy), data);

    item = gtk_menu_item_new_with_label(_("Add"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(_ui_scheduled_events_dialog_menu_add), data);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label(_("Edit"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(_ui_scheduled_events_dialog_menu_edit), data);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label(_("Remove"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(_ui_scheduled_events_dialog_menu_remove), data);
    if (!event_id)
        gtk_widget_set_sensitive(item, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
}

static gboolean _ui_scheduled_events_dialog_list_button_press(UiScheduledEventsDialog *self,
        GdkEventButton *event, GtkTreeView *tree_view)
{
    if (event->button == 3) {
        GtkTreePath *path = NULL;
        guint64 event_id = 0;
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(self->priv->tree_view),
                    (gint)event->x, (gint)event->y,
                    &path, NULL, NULL, NULL)) {
            GtkTreeIter iter;
            if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(self->priv->store), &iter, path)) {
                gtk_tree_path_free(path);
                return TRUE;
            }

            gtk_tree_model_get(GTK_TREE_MODEL(self->priv->store), &iter, SE_ROW_EVENT_ID, &event_id, -1);
            gtk_tree_path_free(path);
        }

        _ui_scheduled_events_dialog_context_popup_menu(self, event, event_id);
        return TRUE;
    }
    return FALSE;
}

static void populate_widget(UiScheduledEventsDialog *self)
{
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(self));

    self->priv->tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(self->priv->store));
    g_signal_connect_swapped(G_OBJECT(self->priv->tree_view), "button-press-event",
            G_CALLBACK(_ui_scheduled_events_dialog_list_button_press), self);

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Channel"),
            renderer, "text", SE_ROW_CHNL_TEXT, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(self->priv->tree_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Start"),
            renderer, "text", SE_ROW_START_TEXT, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(self->priv->tree_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("End"),
            renderer, "text", SE_ROW_END_TEXT, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(self->priv->tree_view), column);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_ETCHED_IN);
    gtk_container_add(GTK_CONTAINER(scroll), self->priv->tree_view);

    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);

    gtk_container_add(GTK_CONTAINER(content_area), scroll);

    gtk_widget_show_all(content_area);

    gtk_dialog_add_buttons(GTK_DIALOG(self),
            _("_Close"), GTK_RESPONSE_CLOSE,
            NULL);
}

static void ui_scheduled_events_dialog_init(UiScheduledEventsDialog *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
            UI_SCHEDULED_EVENTS_DIALOG_TYPE, UiScheduledEventsDialogPrivate);

/*    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);*/

    self->priv->store = gtk_list_store_new(SE_N_ROWS, G_TYPE_UINT64, G_TYPE_UINT64, G_TYPE_STRING,
            G_TYPE_UINT64, G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_STRING);

    populate_widget(self);

    ui_scheduled_events_dialog_update_list(self);

    gtk_window_set_default_size(GTK_WINDOW(self), 400, 300);
}

void _ui_scheduled_events_dialog_fill_list_cb(ScheduledEvent *event, UiScheduledEventsDialog *self)
{
    GtkTreeIter iter;

    gtk_list_store_prepend(self->priv->store, &iter);
    gtk_list_store_set(self->priv->store, &iter,
            SE_ROW_EVENT_ID, event->id,
            SE_ROW_CHNL_ID, event->channel_id,
            SE_ROW_CHNL_TEXT, " ",
            SE_ROW_START, event->time_start,
            SE_ROW_START_TEXT, " ",
            SE_ROW_END, event->time_end,
            SE_ROW_END_TEXT, " ",
            -1);

    gchar buffer[256];
    struct tm *tm;

    tm = localtime((time_t *)&event->time_start);
    strftime(buffer, 256, "%a, %F %H:%M", tm);
    gtk_list_store_set(self->priv->store, &iter, SE_ROW_START_TEXT, buffer, -1);

    tm = localtime((time_t *)&event->time_end);
    strftime(buffer, 256, "%a, %F %H:%M", tm);
    gtk_list_store_set(self->priv->store, &iter, SE_ROW_END_TEXT, buffer, -1);

    ChannelData *chnl = channel_db_get_channel((guint32)event->channel_id);
    if (chnl) {
        gtk_list_store_set(self->priv->store, &iter, SE_ROW_CHNL_TEXT, chnl->name, -1);
        channel_data_free(chnl);
    }
}

void ui_scheduled_events_dialog_update_list(UiScheduledEventsDialog *self)
{
    gtk_list_store_clear(self->priv->store);

    scheduled_event_enum((ScheduledEventEnumProc)_ui_scheduled_events_dialog_fill_list_cb, self);
}

GtkWidget *ui_scheduled_events_dialog_new(GtkWindow *parent)
{
    return g_object_new(UI_SCHEDULED_EVENTS_DIALOG_TYPE, "parent", parent, NULL);
}

void ui_scheduled_events_dialog_set_parent(UiScheduledEventsDialog *dialog, GtkWindow *parent)
{
    dialog->priv->parent = parent;
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_modal(GTK_WINDOW(dialog), FALSE);
}

