#include "ui-sidebar-dvb.h"
#include <glib.h>
#include <glib/gi18n.h>

#include "channel-list.h"
#include "favourites-dialog.h"
#include "config.h"

#include "logging.h"

#include <dvbrecorder/channel-db.h>

typedef struct _UiSidebarChannelsPrivate {
    GtkWidget *favourites_list_widget;
    GtkWidget *channel_list;

    GtkListStore *channel_list_store;
    GtkListStore *favourites_list_store;

    guint32 selected_favourites_list_id;
    guint32 selected_channel_id;

    gulong cursor_changed_signal;
} UiSidebarChannelsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(UiSidebarChannels, ui_sidebar_channels, GTK_TYPE_BIN);

enum {
    PROP_0,
    N_PROPERTIES
};

enum {
    SIGNAL_CHANNEL_SELECTED = 0,
    SIGNAL_FAVOURITES_LIST_CHANGED,
    SIGNAL_SIGNAL_SOURCE_CHANGED,
    SIGNAL_CHANNEL_CONTEXT_ACTION,
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

    ui_sidebar_signals[SIGNAL_FAVOURITES_LIST_CHANGED] =
        g_signal_new("favourites-list-changed",
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
    ui_sidebar_signals[SIGNAL_SIGNAL_SOURCE_CHANGED] =
        g_signal_new("signal-source-changed",
                G_TYPE_FROM_CLASS(gobject_class),
                G_SIGNAL_RUN_LAST,
                0,
                NULL,
                NULL,
                NULL,
                G_TYPE_NONE,
                1,
                G_TYPE_STRING,
                NULL);
    ui_sidebar_signals[SIGNAL_CHANNEL_CONTEXT_ACTION] =
        g_signal_new("channel-context-action",
                G_TYPE_FROM_CLASS(gobject_class),
                G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
                0,
                NULL,
                NULL,
                NULL,
                G_TYPE_NONE,
                2,
                G_TYPE_UINT,
                G_TYPE_UINT,
                NULL);
}

static void _ui_sidebar_channels_channel_channel_selected(UiSidebarChannels *sidebar, guint channel_id, gpointer userdata)
{
    /* Just promote the signal. */
    LOG("channel-selected: %u\n", channel_id);
    g_signal_emit(sidebar, ui_sidebar_signals[SIGNAL_CHANNEL_SELECTED], 0, channel_id);
}

struct _UiSidebarContextData {
    UiSidebarChannels *sidebar;
    guint32 channel_id;
};

static void _ui_sidebar_channels_context_details_activate(struct _UiSidebarContextData *data)
{
    if (data) {
        g_signal_emit(data->sidebar, ui_sidebar_signals[SIGNAL_CHANNEL_CONTEXT_ACTION],
                g_quark_from_string("show-details"), g_quark_from_string("show-details"),
                data->channel_id);
        g_free(data);
    }
}

static void _ui_sidebar_channels_context_popup_menu(UiSidebarChannels *sidebar, GdkEventButton *event,
                                                    guint32 channel_id)
{
    GtkWidget *menu;
    GtkWidget *item;

    struct _UiSidebarContextData *context_data = g_malloc0(sizeof(struct _UiSidebarContextData));
    context_data->sidebar = sidebar;
    context_data->channel_id = channel_id;

    menu = gtk_menu_new();
    item = gtk_menu_item_new_with_label(_("Details"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(_ui_sidebar_channels_context_details_activate), context_data);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    gtk_widget_show_all(menu);
#if GTK_CHECK_VERSION(3,22,0)
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
#else
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, event->button, event->time);
#endif
}

static gboolean _ui_sidebar_channels_channel_button_press(UiSidebarChannels *sidebar, GdkEventButton *event,
        GtkTreeView *tree_view)
{
    UiSidebarChannelsPrivate *priv = ui_sidebar_channels_get_instance_private(sidebar);
    if (event->button == 3) {
        GtkTreePath *path = channel_list_get_path_at_pos(CHANNEL_LIST(priv->channel_list), (gint)event->x, (gint)event->y);
        if (path != NULL) {
            guint32 channel_id = channel_list_get_channel_from_path(CHANNEL_LIST(priv->channel_list), path);
            gtk_tree_path_free(path);

            _ui_sidebar_channels_context_popup_menu(sidebar, event, channel_id);
        }
        return TRUE;
    }

    return FALSE;
}

static void _ui_sidebar_channels_channel_cursor_changed(UiSidebarChannels *sidebar, GtkTreeView *tree_view)
{
    LOG("cursor-changed\n");
    fprintf(stderr, "sidebar channels channel cursor changed\n");
    gboolean cfg_channel_change_on_select = FALSE;
    if (config_get("main", "channel-change-on-select", CFG_TYPE_BOOLEAN, &cfg_channel_change_on_select) != 0)
        return;
    if (!cfg_channel_change_on_select)
        return;

    UiSidebarChannelsPrivate *priv = ui_sidebar_channels_get_instance_private(sidebar);
    g_return_if_fail(priv != NULL);

    guint32 selection_id = channel_list_get_channel_selection(CHANNEL_LIST(priv->channel_list));

    LOG("cursor-changed: selection: %u\n", selection_id);
    g_signal_emit(sidebar, ui_sidebar_signals[SIGNAL_CHANNEL_SELECTED], 0, selection_id);
}

static void _ui_sidebar_channels_signal_source_changed(UiSidebarChannels *sidebar, gchar *signal_source, ChannelList *channel_list)
{
    g_signal_emit(sidebar, ui_sidebar_signals[SIGNAL_SIGNAL_SOURCE_CHANGED], 0, signal_source);
}

GtkListStore *_ui_sidebar_channels_create_favourites_list_store(void)
{
    GtkListStore *list_store = gtk_list_store_new(FAV_N_ROWS, G_TYPE_UINT, G_TYPE_STRING);
    return list_store;
}

void _ui_sidebar_channels_reset_channels_list(UiSidebarChannels *sidebar, GtkListStore *store)
{
    UiSidebarChannelsPrivate *priv = ui_sidebar_channels_get_instance_private(sidebar);
    GtkTreeView *tv = channel_list_get_tree_view(CHANNEL_LIST(priv->channel_list));
    g_signal_handler_block(tv, priv->cursor_changed_signal);

    channel_list_clear(CHANNEL_LIST(priv->channel_list));

    guint32 id = ui_sidebar_channels_get_current_list(sidebar);

    fprintf(stderr, "reset channels list %u\n", id);
    channel_db_foreach(id, (CHANNEL_DB_FOREACH_CALLBACK)channel_list_fill_cb, priv->channel_list);
    g_signal_handler_unblock(tv, priv->cursor_changed_signal);
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
    UiSidebarChannelsPrivate *priv = ui_sidebar_channels_get_instance_private(sidebar);

    fprintf(stderr, "favourites selection changed\n");

    _ui_sidebar_channels_reset_channels_list(sidebar, priv->channel_list_store);

    g_signal_emit(sidebar, ui_sidebar_signals[SIGNAL_FAVOURITES_LIST_CHANGED], 0,
            priv->selected_favourites_list_id);
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
    UiSidebarChannelsPrivate *priv = ui_sidebar_channels_get_instance_private(self);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    priv->favourites_list_widget =
        _ui_sidebar_channels_create_favourites_list_widget(GTK_LIST_STORE(priv->favourites_list_store));

    priv->channel_list = channel_list_new(TRUE);
    GtkTreeView *tv = channel_list_get_tree_view(CHANNEL_LIST(priv->channel_list));
    gtk_tree_view_set_activate_on_single_click(tv, TRUE);
    g_signal_connect_swapped(G_OBJECT(priv->channel_list), "channel-selected",
            G_CALLBACK(_ui_sidebar_channels_channel_channel_selected), self);
    g_signal_connect_swapped(G_OBJECT(tv), "button-press-event",
            G_CALLBACK(_ui_sidebar_channels_channel_button_press), self);
    priv->cursor_changed_signal = g_signal_connect_swapped(G_OBJECT(tv), "cursor-changed",
            G_CALLBACK(_ui_sidebar_channels_channel_cursor_changed), self);
    g_signal_connect_swapped(G_OBJECT(priv->channel_list), "signal-source-changed",
            G_CALLBACK(_ui_sidebar_channels_signal_source_changed), self);

    gtk_box_pack_start(GTK_BOX(vbox), priv->channel_list, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), priv->favourites_list_widget, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(self), vbox);
    gtk_widget_show_all(vbox);

}

static void ui_sidebar_channels_init(UiSidebarChannels *self)
{
    UiSidebarChannelsPrivate *priv = ui_sidebar_channels_get_instance_private(self);

    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);

    priv->favourites_list_store = _ui_sidebar_channels_create_favourites_list_store();

    populate_widget(self);

    priv->channel_list_store = channel_list_get_list_store(CHANNEL_LIST(priv->channel_list));
    _ui_sidebar_channels_reset_favourites_list(priv->favourites_list_store);

    gtk_combo_box_set_active(GTK_COMBO_BOX(priv->favourites_list_widget), 0);
    g_signal_connect_swapped(G_OBJECT(priv->favourites_list_widget), "changed",
            G_CALLBACK(_ui_sidebar_channels_favourites_selection_changed), self);
    _ui_sidebar_channels_reset_channels_list(self, priv->channel_list_store);

}

GtkWidget *ui_sidebar_channels_new(void)
{
    return g_object_new(UI_SIDEBAR_CHANNELS_TYPE, NULL);
}

void ui_sidebar_channels_update_favourites(UiSidebarChannels *sidebar)
{
    g_return_if_fail(IS_UI_SIDEBAR_CHANNELS(sidebar));
    UiSidebarChannelsPrivate *priv = ui_sidebar_channels_get_instance_private(sidebar);
    gint last_id = gtk_combo_box_get_active(GTK_COMBO_BOX(priv->favourites_list_widget));
    _ui_sidebar_channels_reset_favourites_list(priv->favourites_list_store);
    gtk_combo_box_set_active(GTK_COMBO_BOX(priv->favourites_list_widget), last_id);
}

gboolean ui_sidebar_channels_find_list_by_id(UiSidebarChannels *sidebar, guint32 id, GtkTreeIter *match)
{
    g_return_val_if_fail(IS_UI_SIDEBAR_CHANNELS(sidebar), FALSE);
    UiSidebarChannelsPrivate *priv = ui_sidebar_channels_get_instance_private(sidebar);

    GtkTreeIter iter;
    gboolean valid;
    guint32 row_id;

    for (valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->favourites_list_store), &iter);
         valid;
         valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->favourites_list_store), &iter)) {
        gtk_tree_model_get(GTK_TREE_MODEL(priv->favourites_list_store), &iter, FAV_ROW_ID, &row_id, -1);
        if (row_id == id) {
            if (match)
                *match = iter;
            return TRUE;
        }
    }

    return FALSE;
}

guint32 ui_sidebar_channels_get_current_list(UiSidebarChannels *sidebar)
{
    g_return_val_if_fail(IS_UI_SIDEBAR_CHANNELS(sidebar), 0);
    UiSidebarChannelsPrivate *priv = ui_sidebar_channels_get_instance_private(sidebar);

    GtkTreeIter iter;
    GValue value = G_VALUE_INIT;

    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(priv->favourites_list_widget), &iter)) {
        gtk_tree_model_get_value(GTK_TREE_MODEL(priv->favourites_list_store),
                &iter, FAV_ROW_ID, &value);
        priv->selected_favourites_list_id = g_value_get_uint(&value);
        g_value_unset(&value);
    }

    return priv->selected_favourites_list_id;
}

void ui_sidebar_channels_set_current_list(UiSidebarChannels *sidebar, guint32 id)
{
    g_return_if_fail(IS_UI_SIDEBAR_CHANNELS(sidebar));
    UiSidebarChannelsPrivate *priv = ui_sidebar_channels_get_instance_private(sidebar);

    GtkTreeIter iter;

    if (!ui_sidebar_channels_find_list_by_id(sidebar, id, &iter))
        return;

    gtk_combo_box_set_active_iter(GTK_COMBO_BOX(priv->favourites_list_widget), &iter);

/*    _ui_sidebar_channels_reset_favourites_list(sidebar, priv->channel_list_store);*/
}

gboolean ui_sidebar_channels_find_channel_by_id(UiSidebarChannels *sidebar, guint32 id, GtkTreeIter *match)
{
    g_return_val_if_fail(IS_UI_SIDEBAR_CHANNELS(sidebar), FALSE);
    UiSidebarChannelsPrivate *priv = ui_sidebar_channels_get_instance_private(sidebar);

    GtkTreeIter iter;
    gboolean valid;
    gint row_id;

    for (valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->channel_list_store), &iter);
         valid;
         valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->channel_list_store), &iter)) {
        gtk_tree_model_get(GTK_TREE_MODEL(priv->channel_list_store), &iter, CHNL_ROW_ID, &row_id, -1);
        if ((guint32)row_id == id) {
            if (match)
                *match = iter;
            return TRUE;
        }
    }

    return FALSE;
}

guint32 ui_sidebar_channels_get_current_channel(UiSidebarChannels *sidebar)
{
    g_return_val_if_fail(IS_UI_SIDEBAR_CHANNELS(sidebar), 0);
    UiSidebarChannelsPrivate *priv = ui_sidebar_channels_get_instance_private(sidebar);

    return channel_list_get_channel_selection(CHANNEL_LIST(priv->channel_list));
}

void ui_sidebar_channels_set_current_channel(UiSidebarChannels *sidebar, guint32 id, gboolean activate)
{
    g_return_if_fail(IS_UI_SIDEBAR_CHANNELS(sidebar));
    UiSidebarChannelsPrivate *priv = ui_sidebar_channels_get_instance_private(sidebar);

    GtkTreeIter iter;

    if (!ui_sidebar_channels_find_channel_by_id(sidebar, id, &iter)) {
        GtkTreePath *invalid_path = gtk_tree_path_new();
        gtk_tree_view_set_cursor(channel_list_get_tree_view(CHANNEL_LIST(priv->channel_list)),
                invalid_path, NULL, FALSE);
        gtk_tree_path_free(invalid_path);
        return;
    }

    GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->channel_list_store), &iter);

    if (path) {
        GtkTreeView *tv = channel_list_get_tree_view(CHANNEL_LIST(priv->channel_list));
        if (!activate)
            g_signal_handler_block(tv, priv->cursor_changed_signal);
        gtk_tree_view_set_cursor(tv, path, NULL, FALSE);
        if (!activate)
            g_signal_handler_unblock(tv, priv->cursor_changed_signal);
        gtk_tree_path_free(path);
    }
}

const gchar *ui_sidebar_channels_get_current_signal_source(UiSidebarChannels *sidebar)
{
    g_return_val_if_fail(IS_UI_SIDEBAR_CHANNELS(sidebar), NULL);
    UiSidebarChannelsPrivate *priv = ui_sidebar_channels_get_instance_private(sidebar);

    return channel_list_get_active_signal_source(CHANNEL_LIST(priv->channel_list));
}

void ui_sidebar_channels_set_current_signal_source(UiSidebarChannels *sidebar, const gchar *signal_source)
{
    g_return_if_fail(IS_UI_SIDEBAR_CHANNELS(sidebar));
    UiSidebarChannelsPrivate *priv = ui_sidebar_channels_get_instance_private(sidebar);

    channel_list_set_active_signal_source(CHANNEL_LIST(priv->channel_list), signal_source);
}
