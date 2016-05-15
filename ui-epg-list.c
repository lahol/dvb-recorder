#include "ui-epg-list.h"
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include "ui-epg-event-detail.h"

struct _UiEpgListPrivate {
    /* private data */
    GtkWidget *events_list;

    GtkWidget *stack;
    GtkWidget *overview_page;
    GtkWidget *details_page;
    GtkWidget *details_widget;

    DVBRecorder *recorder;
};

G_DEFINE_TYPE(UiEpgList, ui_epg_list, GTK_TYPE_BIN);

enum {
    PROP_0,
    N_PROPERTIES
};

enum {
    EPG_ROW_ID,
    EPG_ROW_TITLE,
    EPG_ROW_STARTTIME,
    EPG_ROW_DURATION,
    EPG_ROW_SHORT,
    EPG_ROW_EXTENDED,
    EPG_ROW_STARTTIME_STRING,
    EPG_ROW_DURATION_STRING,
    EPG_N_ROWS
};

static void ui_epg_list_dispose(GObject *gobject)
{
    /*UiEpgList *self = UI_EPG_LIST(gobject);*/

    G_OBJECT_CLASS(ui_epg_list_parent_class)->dispose(gobject);
}

static void ui_epg_list_finalize(GObject *gobject)
{
    /*UiEpgList *self = UI_EPG_LIST(gobject);*/

    G_OBJECT_CLASS(ui_epg_list_parent_class)->finalize(gobject);
}

static void ui_epg_list_set_property(GObject *object, guint prop_id, 
        const GValue *value, GParamSpec *spec)
{
    UiEpgList *self = UI_EPG_LIST(object);

    switch (prop_id) {
       /* case PROP_RECORDER:
    	break;*/
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void ui_epg_list_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *spec)
{
    UiEpgList *self = UI_EPG_LIST(object);

    switch (prop_id) {
    	/* case PROP_*:
		break;*/
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void ui_epg_list_class_init(UiEpgListClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    /* override GObject methods */
    gobject_class->dispose = ui_epg_list_dispose;
    gobject_class->finalize = ui_epg_list_finalize;
    gobject_class->set_property = ui_epg_list_set_property;
    gobject_class->get_property = ui_epg_list_get_property;

/*    g_object_class_install_property(gobject_class,
            PROP_MODEL,
            g_param_spec_object("model",
                "TreeView Model",
                "The model for the widget",
                GTK_TYPE_TREE_MODEL,
                G_PARAM_READWRITE));*/

    g_type_class_add_private(G_OBJECT_CLASS(klass), sizeof(UiEpgListPrivate));
}

static gint ui_epg_list_box_sort_func(GtkListBoxRow *row1, GtkListBoxRow *row2, gpointer userdata)
{
    /* get event time from row items -> get child (gtk_bin_get_child(row1)) */
    return 1;
}

static void ui_epg_list_row_activated(UiEpgList *self, GtkTreePath *path, GtkTreeViewColumn *column, GtkTreeView *tv)
{
    g_return_if_fail(IS_UI_EPG_LIST(self));

    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_view_get_model(tv);
    if (model == NULL || !gtk_tree_model_get_iter(model, &iter, path))
        return;

    guint16 event_id;

    gtk_tree_model_get(model, &iter, EPG_ROW_ID, &event_id, -1);

    fprintf(stderr, "Show information for event %u\n", event_id);

    EPGEvent *event = epg_event_dup(dvb_recorder_get_epg_event(self->priv->recorder, event_id));
    ui_epg_event_detail_set_event(UI_EPG_EVENT_DETAIL(self->priv->details_widget), event);

    gtk_stack_set_visible_child(GTK_STACK(self->priv->stack), self->priv->details_page);
}

static void ui_epg_list_details_button_back_clicked(UiEpgList *self)
{
    g_return_if_fail(IS_UI_EPG_LIST(self));

    gtk_stack_set_visible_child(GTK_STACK(self->priv->stack), self->priv->overview_page);
}

static void populate_widget(UiEpgList *self)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);

    self->priv->events_list = gtk_tree_view_new();

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkListStore *store;

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Start time"),
            renderer, "text", EPG_ROW_STARTTIME_STRING, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(self->priv->events_list), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Short Description"),
            renderer, "text", EPG_ROW_TITLE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(self->priv->events_list), column);

    store = gtk_list_store_new(EPG_N_ROWS,
                               G_TYPE_UINT,     /* id */
                               G_TYPE_STRING,   /* title */
                               G_TYPE_UINT64,   /* starttime */
                               G_TYPE_UINT,     /* duration */
                               G_TYPE_POINTER,  /* reserved: short desc */
                               G_TYPE_POINTER,  /* reserved: extendet desc */
                               G_TYPE_STRING,   /* starttime as string; alternatively as cell_data_func */
                               G_TYPE_STRING);  /* duration as string */

    gtk_tree_view_set_model(GTK_TREE_VIEW(self->priv->events_list), GTK_TREE_MODEL(store));
    g_object_unref(store);

    g_signal_connect_swapped(G_OBJECT(self->priv->events_list), "row-activated",
            G_CALLBACK(ui_epg_list_row_activated), self);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_ETCHED_IN);
    gtk_container_add(GTK_CONTAINER(scroll), self->priv->events_list);

    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    self->priv->stack = gtk_stack_new();
    fprintf(stderr, "transition type: %d\n", gtk_stack_get_transition_type(GTK_STACK(self->priv->stack)));
    gtk_stack_set_transition_type(GTK_STACK(self->priv->stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    self->priv->overview_page = vbox;

    gtk_container_add(GTK_CONTAINER(self->priv->stack), self->priv->overview_page);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);

    GtkWidget *button = gtk_button_new_with_label(_("Back"));
    g_signal_connect_swapped(G_OBJECT(button), "clicked",
            G_CALLBACK(ui_epg_list_details_button_back_clicked), self);
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

    self->priv->details_widget = ui_epg_event_detail_new();
    gtk_box_pack_start(GTK_BOX(vbox), self->priv->details_widget, TRUE, TRUE, 0);

    self->priv->details_page = vbox;
    gtk_container_add(GTK_CONTAINER(self->priv->stack), self->priv->details_page);

    gtk_stack_set_visible_child(GTK_STACK(self->priv->stack), self->priv->overview_page);

    gtk_container_add(GTK_CONTAINER(self), self->priv->stack);
    gtk_widget_show_all(self->priv->stack);
}

static void ui_epg_list_init(UiEpgList *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
            UI_EPG_LIST_TYPE, UiEpgListPrivate);

    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);

    populate_widget(self);
}

GtkWidget *ui_epg_list_new(void)
{
    return g_object_new(UI_EPG_LIST_TYPE, NULL);
}

struct _ui_epg_list_update_data {
    UiEpgList *list;
    GList *events;
};

void _starttime_to_string(gchar *buffer, gsize buflen, time_t starttime)
{
    struct tm *tm = localtime(&starttime);

    strftime(buffer, buflen, "%a, %d.%m. %R", tm);
}

void _duration_to_string(gchar *buffer, gsize buflen, guint32 seconds)
{
/*    guint32 d, h, m, s, t;
    d = seconds / (24*3600);
    t = seconds % (24*3600);
    h = t / 3600;
    t = t % 3600;
    m = t / 60;
    s = t % 60;*/

    snprintf(buffer, buflen, "%us", seconds);
}

/* present/following event may be there twice: once in 0x4e and another time in 0x50..0x5f;
 * assume that the list is already sorted by time, i.e., the corresponding events must be
 * next to each other */
GList *_epg_list_remove_duplicates(GList *list)
{
    GList *tmp = list, *next;

    while (tmp) {
        next = tmp->next;

        if (tmp->prev) {
            if (((EPGEvent *)tmp->prev->data)->event_id == ((EPGEvent *)tmp->data)->event_id) {
                /* duplicate detected (event_id is unique in service definition) */
                /* drop this event */
                epg_event_free((EPGEvent *)tmp->data);
                list = g_list_delete_link(list, tmp);
            }
        }

        tmp = next;
    }

    return list;
}

static gboolean _ui_epg_list_update_events_idle(struct _ui_epg_list_update_data *data)
{
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(data->list->priv->events_list)));
    GtkTreeIter iter;
    EPGEvent *ev;
    EPGShortEvent *sev;
    gchar starttime_str[256];
    gchar duration_str[256];

    gtk_list_store_clear(GTK_LIST_STORE(store));

    data->events = _epg_list_remove_duplicates(data->events);

    for ( ; data->events; data->events = g_list_next(data->events)) {
        ev = (EPGEvent *)data->events->data;
        /* list head */
        sev = (EPGShortEvent *)(ev->short_descriptions ? ev->short_descriptions->data : NULL);
        _starttime_to_string(starttime_str, 256, ev->starttime);
        _duration_to_string(duration_str, 256, ev->duration);
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                EPG_ROW_TITLE, sev ? sev->description : "<i>no description</i>",
                EPG_ROW_ID, ev->event_id,
                EPG_ROW_STARTTIME, ev->starttime,
                EPG_ROW_DURATION, ev->duration,
                EPG_ROW_STARTTIME_STRING, starttime_str,
                EPG_ROW_DURATION_STRING, duration_str,
                -1);
    }

    /* FIXME: clear events data (or keep until next update?) */
    g_list_free_full(data->events, (GDestroyNotify)epg_event_free);
    g_free(data);

    return FALSE;
}

void ui_epg_list_update_events(UiEpgList *list, GList *events)
{
    struct _ui_epg_list_update_data *data = g_malloc(sizeof(struct _ui_epg_list_update_data));
    data->list = list;
    data->events = epg_event_list_dup(events);

    gdk_threads_add_idle((GSourceFunc)_ui_epg_list_update_events_idle, data);
}

void ui_epg_list_set_recorder_handle(UiEpgList *list, DVBRecorder *handle)
{
    g_return_if_fail(IS_UI_EPG_LIST(list));

    list->priv->recorder = handle;
}

