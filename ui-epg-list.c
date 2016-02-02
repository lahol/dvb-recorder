#include "ui-epg-list.h"
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include "epg-event-widget.h"

struct _UiEpgListPrivate {
    /* private data */
    GtkWidget *events_list;

    GList *children;
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
        /* case PROP_*:
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

static void populate_widget(UiEpgList *self)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);

    self->priv->events_list = gtk_list_box_new();
    gtk_list_box_set_sort_func(GTK_LIST_BOX(self->priv->events_list),
                               (GtkListBoxSortFunc)ui_epg_list_box_sort_func,
                               NULL,
                               NULL);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_ETCHED_IN);
    gtk_container_add(GTK_CONTAINER(scroll), self->priv->events_list);

    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

/*    GtkWidget *box;
    box = gtk_list_box_row_new();
    gtk_container_add(GTK_CONTAINER(box), epg_event_widget_new());
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(box), FALSE);
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(box), FALSE);
    gtk_container_add(GTK_CONTAINER(self->priv->events_list), box);

    box = gtk_list_box_row_new();
    gtk_container_add(GTK_CONTAINER(box), epg_event_widget_new());
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(box), FALSE);
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(box), FALSE);
    gtk_container_add(GTK_CONTAINER(self->priv->events_list), box);

    box = gtk_list_box_row_new();
    gtk_container_add(GTK_CONTAINER(box), epg_event_widget_new());
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(box), FALSE);
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(box), FALSE);
    gtk_container_add(GTK_CONTAINER(self->priv->events_list), box);

    box = gtk_list_box_row_new();
    gtk_container_add(GTK_CONTAINER(box), epg_event_widget_new());
    gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(box), FALSE);
    gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(box), FALSE);
    gtk_container_add(GTK_CONTAINER(self->priv->events_list), box);
*/

    gtk_container_add(GTK_CONTAINER(self), vbox);
    gtk_widget_show_all(vbox);
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

void ui_epg_list_clear_children(UiEpgList *self)
{
    g_list_free_full(self->priv->children, (GDestroyNotify)gtk_widget_destroy);
    self->priv->children = NULL;
}

void ui_epg_list_update_events(UiEpgList *list, GList *events)
{
    ui_epg_list_clear_children(list);
    GtkWidget *child;
    for ( ; events; events = g_list_next(events)) {
        child = gtk_list_box_row_new();
        gtk_container_add(GTK_CONTAINER(child), epg_event_widget_new((EPGEvent *)events->data));
        gtk_list_box_row_set_activatable(GTK_LIST_BOX_ROW(child), FALSE);
        gtk_list_box_row_set_selectable(GTK_LIST_BOX_ROW(child), FALSE);
        gtk_container_add(GTK_CONTAINER(list->priv->events_list), child);
        gtk_widget_show(child);

        list->priv->children = g_list_prepend(list->priv->children, child);
    }
}

