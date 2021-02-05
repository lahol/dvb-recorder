#include "channel-list.h"
#include <glib/gi18n.h>
#include <glib/gprintf.h>

typedef struct _ChannelListPrivate {
    GtkTreeModel *model;

    GtkWidget *channels_widget;
    GtkWidget *channel_tree_view;
    GtkWidget *filter_entry;

    GtkWidget *signal_source_widget;
    GtkListStore *signal_source_store;

    GtkListStore *channels_store;
    GtkTreeModel *channels_filter_model;

    GtkTreeSelection *selection;

    gboolean filterable;

    gchar *filter_text;
    gchar *filter_source;

    GList *active_sources;
    GQuark current_source;
    guint combo_box_changed_signal;
} ChannelListPrivate;

enum {
    SIGNAL_SOURCE_ID,
    SIGNAL_SOURCE_TITLE,
    SIGNAL_SOURCE_N
};

G_DEFINE_TYPE_WITH_PRIVATE(ChannelList, channel_list, GTK_TYPE_BIN);

enum {
    PROP_0,
    PROP_MODEL,
    PROP_SELECTION,
    PROP_FILTERABLE,
    N_PROPERTIES
};

enum {
    SIGNAL_SIGNAL_SOURCE_CHANGED = 0,
    N_SIGNALS
};

guint channel_list_signals[N_SIGNALS];

void channel_list_set_filterable(ChannelList *channel_list, gboolean filterable);

static void channel_list_dispose(GObject *gobject)
{
    ChannelList *self = CHANNEL_LIST(gobject);
    ChannelListPrivate *priv = channel_list_get_instance_private(self);

    g_clear_object(&priv->model);

    g_free(priv->filter_text);
    priv->filter_text = NULL;

    G_OBJECT_CLASS(channel_list_parent_class)->dispose(gobject);
}

static void channel_list_finalize(GObject *gobject)
{
    /*ChannelList *self = CHANNEL_LIST(gobject);*/

    G_OBJECT_CLASS(channel_list_parent_class)->finalize(gobject);
}

static void channel_list_set_property(GObject *object, guint prop_id,
        const GValue *value, GParamSpec *spec)
{
    ChannelList *channel_list = CHANNEL_LIST(object);

    switch (prop_id) {
        case PROP_MODEL:
            channel_list_set_model(channel_list, g_value_get_object(value));
            break;
        case PROP_FILTERABLE:
            fprintf(stderr, "set filterable\n");
            channel_list_set_filterable(channel_list, g_value_get_boolean(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void channel_list_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *spec)
{
    ChannelList *channel_list = CHANNEL_LIST(object);
    ChannelListPrivate *priv = channel_list_get_instance_private(channel_list);

    switch (prop_id) {
        case PROP_MODEL:
            g_value_set_object(value, priv->model);
            break;
        case PROP_SELECTION:
            g_value_set_object(value, priv->selection);
            break;
        case PROP_FILTERABLE:
            g_value_set_boolean(value, priv->filterable);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void channel_list_class_init(ChannelListClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    /* override GObject methods */
    gobject_class->dispose = channel_list_dispose;
    gobject_class->finalize = channel_list_finalize;
    gobject_class->set_property = channel_list_set_property;
    gobject_class->get_property = channel_list_get_property;

    g_object_class_install_property(gobject_class,
            PROP_MODEL,
            g_param_spec_object("model",
                "TreeView Model",
                "The model for the channel list",
                GTK_TYPE_TREE_MODEL,
                G_PARAM_READWRITE));
    g_object_class_install_property(gobject_class,
            PROP_SELECTION,
            g_param_spec_object("selection",
                "TreeSelection",
                "The selection of the tree view",
                GTK_TYPE_TREE_SELECTION,
                G_PARAM_READABLE));
    g_object_class_install_property(gobject_class,
            PROP_FILTERABLE,
            g_param_spec_boolean("filterable",
                "Filterable",
                "Create filter or not",
                TRUE,
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    channel_list_signals[SIGNAL_SIGNAL_SOURCE_CHANGED] =
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
}

/* data = ChannelListPrivate */
static void _channel_list_filter_changed(GtkWidget *widget, gpointer data)
{
    fprintf(stderr, "filter_changed\n");
    ChannelListPrivate *priv = (ChannelListPrivate *)data;
    if (priv->filter_text)
        g_free(priv->filter_text);
    priv->filter_text = g_utf8_strdown(gtk_entry_get_text(GTK_ENTRY(widget)), -1);
    if (priv->channels_filter_model)
        gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(priv->channels_filter_model));
}

static void _channel_list_signal_source_changed(ChannelList *self, GtkComboBox *box)
{
    fprintf(stderr, "signal source changed\n");
    g_return_if_fail(IS_CHANNEL_LIST(self));
    ChannelListPrivate *priv = channel_list_get_instance_private(self);

    GQuark new_source;
    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter(box, &iter)) {
        gtk_tree_model_get(GTK_TREE_MODEL(priv->signal_source_store), &iter, SIGNAL_SOURCE_ID, &new_source, -1);
        priv->current_source = new_source;
        gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(priv->channels_filter_model));

        g_signal_emit(self, channel_list_signals[SIGNAL_SIGNAL_SOURCE_CHANGED], 0, g_quark_to_string(new_source));
    }
}

/* data = GtkWidget (entry filter) */
static gboolean _channel_list_visible_func(GtkTreeModel *model, GtkTreeIter *iter, ChannelList *self)
{
    ChannelListPrivate *priv = channel_list_get_instance_private(self);
    if ((priv->filter_text == NULL || priv->filter_text[0] == '\0')
            && priv->current_source == 0) {
        return TRUE;
    }

    gchar *channel;
    gchar *source;
    gtk_tree_model_get(model, iter, CHNL_ROW_TITLE, &channel, CHNL_ROW_SOURCE, &source, -1);
    if (!channel)
        return FALSE;
    gchar *lcchannel = g_utf8_strdown(channel, -1);
    g_free(channel);

    GQuark source_quark = g_quark_from_string(source);
    g_free(source);

    gboolean result = FALSE;
    if (priv->current_source == source_quark || priv->current_source == 0) {
        if ((priv->filter_text && priv->filter_text[0] != '\0' &&
                g_strstr_len(lcchannel, -1, priv->filter_text) != NULL) ||
                priv->filter_text == NULL || priv->filter_text[0] == '\0')
            result = TRUE;
    }

    g_free(lcchannel);

    return result;
}

static void populate_widget(ChannelList *self)
{
    ChannelListPrivate *priv = channel_list_get_instance_private(self);
    fprintf(stderr, "channel_list: populate_widget()\n");
    /* filter entry */
    priv->filter_entry = gtk_entry_new();
    if (!priv->filterable) {
        g_object_set(G_OBJECT(priv->filter_entry), "editable", FALSE, NULL);
    }
    g_signal_connect(G_OBJECT(priv->filter_entry), "changed",
            G_CALLBACK(_channel_list_filter_changed), priv);

    /* channel list */
    priv->channel_tree_view = gtk_tree_view_new();

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    priv->signal_source_widget = gtk_combo_box_new_with_model(
            GTK_TREE_MODEL(priv->signal_source_store));
    priv->combo_box_changed_signal =
        g_signal_connect_swapped(G_OBJECT(priv->signal_source_widget), "changed",
                G_CALLBACK(_channel_list_signal_source_changed), self);
    g_object_unref(priv->signal_source_store);

    renderer = gtk_cell_renderer_text_new();

    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->signal_source_widget), renderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(priv->signal_source_widget), renderer, "text",
            SIGNAL_SOURCE_TITLE, NULL);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Source"),
            renderer, "text", CHNL_ROW_SOURCE, "foreground", CHNL_ROW_FOREGROUND, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->channel_tree_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Title"),
            renderer, "text", CHNL_ROW_TITLE, "foreground", CHNL_ROW_FOREGROUND, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->channel_tree_view), column);

    priv->selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(priv->channel_tree_view));

    priv->channels_widget = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(priv->channels_widget),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(priv->channels_widget), GTK_SHADOW_ETCHED_IN);
    gtk_container_add(GTK_CONTAINER(priv->channels_widget), priv->channel_tree_view);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_box_pack_start(GTK_BOX(box), priv->filter_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), priv->signal_source_widget, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), priv->channels_widget, TRUE, TRUE, 0);

    gtk_widget_show_all(box);

    gtk_container_add(GTK_CONTAINER(self), box);
}

static void channel_list_init(ChannelList *self)
{
    fprintf(stderr, "channel_list_init()\n");
    ChannelListPrivate *priv = channel_list_get_instance_private(self);

    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);

    priv->signal_source_store = gtk_list_store_new(SIGNAL_SOURCE_N, G_TYPE_UINT, G_TYPE_STRING);

    populate_widget(self);

    GtkListStore *store = gtk_list_store_new(CHNL_N_ROWS,
            G_TYPE_INT,      /* id */
            G_TYPE_STRING,   /* title */
            G_TYPE_STRING,   /* signalsource */
            G_TYPE_STRING);  /* foreground */
    channel_list_set_model(self, GTK_TREE_MODEL(store));
    g_object_unref(G_OBJECT(store));
}

GtkWidget *channel_list_new(gboolean filterable)
{
    fprintf(stderr, "channel_list_new()\n");
    return g_object_new(CHANNEL_LIST_TYPE, "filterable", filterable, NULL);
}

void channel_list_set_model(ChannelList *channel_list, GtkTreeModel *model)
{
    g_return_if_fail(IS_CHANNEL_LIST(channel_list));
    ChannelListPrivate *priv = channel_list_get_instance_private(channel_list);

    if (priv->channels_filter_model)
        g_object_unref(priv->channels_filter_model);
    if (model != NULL) {
        priv->channels_store = GTK_LIST_STORE(model);

        if (priv->filterable) {
            priv->channels_filter_model = gtk_tree_model_filter_new(model, NULL);
            gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(priv->channels_filter_model),
                    (GtkTreeModelFilterVisibleFunc)_channel_list_visible_func, channel_list, NULL);

            gtk_tree_view_set_model(GTK_TREE_VIEW(priv->channel_tree_view),
                    priv->channels_filter_model);
        }
        else {
            priv->channels_filter_model = NULL;
            gtk_tree_view_set_model(GTK_TREE_VIEW(priv->channel_tree_view),
                    GTK_TREE_MODEL(priv->channels_store));
        }
    }
    else {
        priv->channels_store = NULL;
        priv->channels_filter_model = NULL;
        gtk_tree_view_set_model(GTK_TREE_VIEW(priv->channel_tree_view), NULL);
    }
}

void channel_list_set_filterable(ChannelList *channel_list, gboolean filterable)
{
    g_return_if_fail(IS_CHANNEL_LIST(channel_list));
    ChannelListPrivate *priv = channel_list_get_instance_private(channel_list);

    priv->filterable = filterable;

    if (priv->channels_store) {
        /* do not accidently drop model */
        g_object_ref(G_OBJECT(priv->channels_store));
        channel_list_set_model(channel_list, GTK_TREE_MODEL(priv->channels_store));
        g_object_unref(G_OBJECT(priv->channels_store));
    }

    g_object_set(G_OBJECT(priv->filter_entry), "editable", filterable, NULL);
    gtk_widget_set_sensitive(priv->filter_entry, filterable);
}

GtkTreeSelection *channel_list_get_selection(ChannelList *channel_list)
{
    g_return_val_if_fail(IS_CHANNEL_LIST(channel_list), NULL);
    ChannelListPrivate *priv = channel_list_get_instance_private(channel_list);
    return priv->selection;
}

GtkListStore *channel_list_get_list_store(ChannelList *channel_list)
{
    g_return_val_if_fail(IS_CHANNEL_LIST(channel_list), NULL);
    ChannelListPrivate *priv = channel_list_get_instance_private(channel_list);
    return priv->channels_store;
}

GtkTreeView *channel_list_get_tree_view(ChannelList *channel_list)
{
    g_return_val_if_fail(IS_CHANNEL_LIST(channel_list), NULL);
    ChannelListPrivate *priv = channel_list_get_instance_private(channel_list);
    return GTK_TREE_VIEW(priv->channel_tree_view);
}

void channel_list_fill_cb(ChannelData *data, ChannelList *channel_list)
{
    g_return_if_fail(IS_CHANNEL_LIST(channel_list));
    g_return_if_fail(data != NULL);
    ChannelListPrivate *priv = channel_list_get_instance_private(channel_list);

    GtkTreeIter iter;

    gtk_list_store_append(priv->channels_store, &iter);
    gtk_list_store_set(priv->channels_store, &iter,
           CHNL_ROW_ID, data->id,
           CHNL_ROW_TITLE, data->name,
           CHNL_ROW_SOURCE, data->signalsource,
           CHNL_ROW_FOREGROUND, data->flags & CHNL_FLAG_DIRTY ? "gray" : NULL,
           -1);

    GQuark source_quark = g_quark_from_string(data->signalsource);
    if (source_quark && !g_list_find(priv->active_sources, GUINT_TO_POINTER(source_quark))) {
        priv->active_sources = g_list_prepend(priv->active_sources, GUINT_TO_POINTER(source_quark));
        /* append to combobox */
        g_signal_handler_block(priv->signal_source_widget, priv->combo_box_changed_signal);
        gtk_list_store_append(priv->signal_source_store, &iter);
        gtk_list_store_set(priv->signal_source_store, &iter,
                SIGNAL_SOURCE_ID, source_quark,
                SIGNAL_SOURCE_TITLE, data->signalsource,
                -1);
        if (source_quark == priv->current_source) {
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(priv->signal_source_widget), &iter);
        }
        g_signal_handler_unblock(priv->signal_source_widget, priv->combo_box_changed_signal);
    }
}

void channel_list_clear(ChannelList *channel_list)
{
    g_return_if_fail(IS_CHANNEL_LIST(channel_list));
    ChannelListPrivate *priv = channel_list_get_instance_private(channel_list);

    if (priv->channels_store)
        gtk_list_store_clear(priv->channels_store);

    g_list_free(priv->active_sources);
    priv->active_sources = NULL;

    if (priv->signal_source_store) {
        g_signal_handler_block(priv->signal_source_widget, priv->combo_box_changed_signal);
        gtk_list_store_clear(priv->signal_source_store);
        GtkTreeIter iter;

        gtk_list_store_append(priv->signal_source_store, &iter);
        gtk_list_store_set(priv->signal_source_store, &iter,
                SIGNAL_SOURCE_ID, 0,
                SIGNAL_SOURCE_TITLE, "All satellites",
                -1);
        g_signal_handler_unblock(priv->signal_source_widget, priv->combo_box_changed_signal);
    }
}

void channel_list_set_active_signal_source(ChannelList *channel_list, const gchar *signal_source)
{
    g_return_if_fail(IS_CHANNEL_LIST(channel_list));
    ChannelListPrivate *priv = channel_list_get_instance_private(channel_list);

    gboolean valid;
    GQuark id;
    GtkTreeIter iter;
    GQuark source_quark = g_quark_from_string(signal_source);

    for (valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->signal_source_store), &iter);
         valid;
         valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->signal_source_store), &iter)) {
        gtk_tree_model_get(GTK_TREE_MODEL(priv->signal_source_store), &iter,
                                          SIGNAL_SOURCE_ID, &id, -1);
        if (source_quark == id) {
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(priv->signal_source_widget), &iter);
        }
    }
}

const gchar *channel_list_get_active_signal_source(ChannelList *channel_list)
{
    g_return_val_if_fail(IS_CHANNEL_LIST(channel_list), NULL);
    ChannelListPrivate *priv = channel_list_get_instance_private(channel_list);

    if (priv->current_source)
        return g_quark_to_string(priv->current_source);

    return NULL;
}

static gboolean channel_list_find_channel_by_id(ChannelList *self, guint32 id, GtkTreeIter *match)
{
    GtkTreeIter iter;
    gboolean valid;
    gint row_id;
    ChannelListPrivate *priv = channel_list_get_instance_private(self);

    for (valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(priv->channels_store), &iter);
         valid;
         valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(priv->channels_store), &iter)) {
        gtk_tree_model_get(GTK_TREE_MODEL(priv->channels_store), &iter, CHNL_ROW_ID, &row_id, -1);
        if ((guint32)row_id == id) {
            if (match)
                *match = iter;
            return TRUE;
        }
    }

    return FALSE;
}

void channel_list_set_channel_selection(ChannelList *channel_list, guint32 id)
{
    GtkTreeIter iter;
    if (!channel_list_find_channel_by_id(channel_list, id, &iter))
        return;
    ChannelListPrivate *priv = channel_list_get_instance_private(channel_list);

    GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(priv->channels_store), &iter);

    if (path) {
        gtk_tree_view_set_cursor(GTK_TREE_VIEW(priv->channel_tree_view), path, NULL, FALSE);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(priv->channel_tree_view), path, NULL, FALSE, 0, 0);
        gtk_tree_path_free(path);
    }
}

