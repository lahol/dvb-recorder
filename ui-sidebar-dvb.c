#include "ui-sidebar-dvb.h"
#include <glib.h>
#include <glib/gi18n.h>

#include "channel-list.h"
#include "favourites-dialog.h"
#include "config.h"

#include <dvbrecorder/channel-db.h>

struct _UiSidebarChannelsPrivate {
    GtkWidget *favourites_list_widget;
    GtkWidget *channel_list;

    GtkListStore *channel_list_store;
    GtkListStore *favourites_list_store;
};

G_DEFINE_TYPE(UiSidebarChannels, ui_sidebar_channels, GTK_TYPE_BIN);

enum {
    PROP_0,
    N_PROPERTIES
};

enum {
    SIGNAL_CHANNEL_SELECTED = 0,
    N_SIGNALS
};

guint ui_sidebar_signals[N_SIGNALS];

static void ui_sidebar_channels_dispose(GObject *gobject)
{
    G_OBJECT_CLASS(ui_sidebar_channels_parent_class)->dispose(gobject);
}

static void ui_sidebar_channels_finalize(GObject *gobject)
{
    G_OBJECT_CLASS(ui_sidebar_channels_parent_class)->finalize(gobject);
}

static void ui_sidebar_channels_set_property(GObject *object, guint prop_id,
        const GValue *value, GParamSpec *spec)
{
    /*UiSidebarChannels *self = UI_SIDEBAR_CHANNELS(object);*/

    switch (prop_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void ui_sidebar_channels_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *spec)
{
    /*UiSidebarChannels *self = UI_SIDEBAR_CHANNELS(object);*/

    switch (prop_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void ui_sidebar_channels_class_init(UiSidebarChannelsClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->dispose = ui_sidebar_channels_dispose;
    gobject_class->finalize = ui_sidebar_channels_finalize;
    gobject_class->set_property = ui_sidebar_channels_set_property;
    gobject_class->get_property = ui_sidebar_channels_get_property;

    ui_sidebar_signals[SIGNAL_CHANNEL_SELECTED] =
        g_signal_new("channel-selected",
                G_TYPE_FROM_CLASS(gobject_class),
                G_SIGNAL_RUN_LAST,
                0,
                NULL,
                NULL,
                NULL,
                G_TYPE_NONE,
                1,
                G_TYPE_UINT,
                NULL);

    g_type_class_add_private(G_OBJECT_CLASS(klass), sizeof(UiSidebarChannelsPrivate));
}

static void _ui_sidebar_channels_channel_row_activated(UiSidebarChannels *sidebar, GtkTreePath *path, GtkTreeViewColumn *column,
        GtkTreeView *tree_view)
{
    printf("row activated\n");
    g_return_if_fail(IS_UI_SIDEBAR_CHANNELS(sidebar));
    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    if (model == NULL || !gtk_tree_model_get_iter(model, &iter, path)) {
        return;
    }

    guint32 selection_id;

    gtk_tree_model_get(model, &iter, CHNL_ROW_ID, &selection_id, -1);

    g_signal_emit(sidebar, ui_sidebar_signals[SIGNAL_CHANNEL_SELECTED], 0, selection_id);
}

static void _ui_sidebar_channels_channel_cursor_changed(UiSidebarChannels *sidebar, GtkTreeView *tree_view)
{
    gboolean cfg_channel_change_on_select = FALSE;
    if (config_get("main", "channel-change-on-select", CFG_TYPE_BOOLEAN, &cfg_channel_change_on_select) != 0)
        return;
    if (!cfg_channel_change_on_select)
        return;

    GtkTreePath *path = NULL;
    gtk_tree_view_get_cursor(tree_view, &path, NULL);

    if (!path)
        return;

    GtkTreeIter iter;
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    if (model == NULL || !gtk_tree_model_get_iter(model, &iter, path))
        return;

    guint32 selection_id;

    gtk_tree_model_get(model, &iter, CHNL_ROW_ID, &selection_id, -1);

    gtk_tree_path_free(path);

    g_signal_emit(sidebar, ui_sidebar_signals[SIGNAL_CHANNEL_SELECTED], 0, selection_id);
}

GtkListStore *_ui_sidebar_channels_create_favourites_list_store(void)
{
    GtkListStore *list_store = gtk_list_store_new(FAV_N_ROWS, G_TYPE_UINT, G_TYPE_STRING);
    return list_store;
}

void _ui_sidebar_channels_reset_channels_list(UiSidebarChannels *sidebar, GtkListStore *store)
{
    gtk_list_store_clear(store);

    GtkTreeIter iter;
    GValue value = G_VALUE_INIT;
    guint32 id = 0;

    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(sidebar->priv->favourites_list_widget), &iter)) {
        gtk_tree_model_get_value(GTK_TREE_MODEL(sidebar->priv->favourites_list_store),
                &iter, FAV_ROW_ID, &value);
        id = g_value_get_uint(&value);
        g_value_unset(&value);
    }
    
    fprintf(stderr, "reset channels list %u\n", id);
    channel_db_foreach(id, (CHANNEL_DB_FOREACH_CALLBACK)channel_list_fill_cb, store);
}

void _ui_sidebar_channels_fill_favourites_cb(ChannelDBList *data, GtkListStore *store)
{
    GtkTreeIter iter;

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
            FAV_ROW_ID, data->id,
            FAV_ROW_TITLE, data->title,
            -1);
}

static void _ui_sidebar_channels_favourites_selection_changed(UiSidebarChannels *sidebar, GtkComboBox *box)
{
    g_return_if_fail(IS_UI_SIDEBAR_CHANNELS(sidebar));
    _ui_sidebar_channels_reset_channels_list(sidebar, sidebar->priv->channel_list_store);
}

void _ui_sidebar_channels_reset_favourites_list(GtkListStore *store)
{
    gtk_list_store_clear(store);

    channel_db_list_foreach((CHANNEL_DB_LIST_FOREACH_CALLBACK)_ui_sidebar_channels_fill_favourites_cb, store);
}

GtkWidget *_ui_sidebar_channels_create_favourites_list_widget(GtkListStore *store)
{
    GtkWidget *combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);

    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), renderer, "text", FAV_ROW_TITLE, NULL);

    return combo;
}

static void populate_widget(UiSidebarChannels *self)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    self->priv->favourites_list_widget =
        _ui_sidebar_channels_create_favourites_list_widget(GTK_LIST_STORE(self->priv->favourites_list_store));

    self->priv->channel_list = channel_list_new(TRUE);
    GtkTreeView *tv = channel_list_get_tree_view(CHANNEL_LIST(self->priv->channel_list));
    g_signal_connect_swapped(G_OBJECT(tv), "row-activated",
            G_CALLBACK(_ui_sidebar_channels_channel_row_activated), self);
    g_signal_connect_swapped(G_OBJECT(tv), "cursor-changed",
            G_CALLBACK(_ui_sidebar_channels_channel_cursor_changed), self);

    gtk_box_pack_start(GTK_BOX(vbox), self->priv->channel_list, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), self->priv->favourites_list_widget, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(self), vbox);
    gtk_widget_show_all(vbox);

}

static void ui_sidebar_channels_init(UiSidebarChannels *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
            UI_SIDEBAR_CHANNELS_TYPE, UiSidebarChannelsPrivate);

    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);

    self->priv->favourites_list_store = _ui_sidebar_channels_create_favourites_list_store();

    populate_widget(self);

    self->priv->channel_list_store = channel_list_get_list_store(CHANNEL_LIST(self->priv->channel_list));
    _ui_sidebar_channels_reset_favourites_list(self->priv->favourites_list_store);

    gtk_combo_box_set_active(GTK_COMBO_BOX(self->priv->favourites_list_widget), 0);
    g_signal_connect_swapped(G_OBJECT(self->priv->favourites_list_widget), "changed",
            G_CALLBACK(_ui_sidebar_channels_favourites_selection_changed), self);
    _ui_sidebar_channels_reset_channels_list(self, self->priv->channel_list_store);

}

GtkWidget *ui_sidebar_channels_new(void)
{
    return g_object_new(UI_SIDEBAR_CHANNELS_TYPE, NULL);
}

void ui_sidebar_channels_update_favourites(UiSidebarChannels *sidebar)
{
    g_return_if_fail(IS_UI_SIDEBAR_CHANNELS(sidebar));
    gint last_id = gtk_combo_box_get_active(GTK_COMBO_BOX(sidebar->priv->favourites_list_widget));
    _ui_sidebar_channels_reset_favourites_list(sidebar->priv->favourites_list_store);
    gtk_combo_box_set_active(GTK_COMBO_BOX(sidebar->priv->favourites_list_widget), last_id);
}
