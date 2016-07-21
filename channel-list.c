#include "channel-list.h"
#include <glib/gi18n.h>
#include <glib/gprintf.h>

struct _ChannelListPrivate {
    GtkTreeModel *model;

    GtkWidget *channels_widget;
    GtkWidget *channel_tree_view;
    GtkWidget *filter_entry;

    GtkListStore *channels_store;
    GtkTreeModel *channels_filter_model;

    GtkTreeSelection *selection;

    gboolean filterable;

    gchar *filter_text;
    gchar *filter_source;

    GList *active_sources;
};

G_DEFINE_TYPE(ChannelList, channel_list, GTK_TYPE_BIN);

enum {
    PROP_0,
    PROP_MODEL,
    PROP_SELECTION,
    PROP_FILTERABLE,
    N_PROPERTIES
};

void channel_list_set_filterable(ChannelList *channel_list, gboolean filterable);

static void channel_list_dispose(GObject *gobject)
{
    ChannelList *self = CHANNEL_LIST(gobject);

    g_clear_object(&self->priv->model);

    g_free(self->priv->filter_text);
    self->priv->filter_text = NULL;

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

    switch (prop_id) {
        case PROP_MODEL:
            g_value_set_object(value, channel_list->priv->model);
            break;
        case PROP_SELECTION:
            g_value_set_object(value, channel_list->priv->selection);
            break;
        case PROP_FILTERABLE:
            g_value_set_boolean(value, channel_list->priv->filterable);
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

    g_type_class_add_private(G_OBJECT_CLASS(klass), sizeof(ChannelListPrivate));
}

/* data = ChannelListPrivate */
void _channel_list_filter_changed(GtkWidget *widget, gpointer data)
{
    ChannelListPrivate *priv = (ChannelListPrivate *)data;
    if (priv->filter_text)
        g_free(priv->filter_text);
    priv->filter_text = g_utf8_strdown(gtk_entry_get_text(GTK_ENTRY(widget)), -1);
    if (priv->channels_filter_model)
        gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(priv->channels_filter_model));
}

/* data = GtkWidget (entry filter) */
static gboolean _channel_list_visible_func(GtkTreeModel *model, GtkTreeIter *iter, ChannelList *self)
{
    fprintf(stderr, "filter_text: %s, filter_source: %s\n", self->priv->filter_text, self->priv->filter_source);
    if ((self->priv->filter_text == NULL || self->priv->filter_text[0] == '\0')
            && (self->priv->filter_source == NULL || self->priv->filter_source[0] == '\0')) {
        return TRUE;
    }

    gchar *channel;
    gchar *source;
    gtk_tree_model_get(model, iter, CHNL_ROW_TITLE, &channel, CHNL_ROW_SOURCE, &source, -1);
    gchar *lcchannel = g_utf8_strdown(channel, -1);
    g_free(channel);

    gboolean result = FALSE;
    if (self->priv->filter_text && self->priv->filter_text[0] != '\0' && 
            g_strstr_len(lcchannel, -1, self->priv->filter_text) != NULL)
        result = TRUE;
    if (self->priv->filter_source && g_strcmp0(source, self->priv->filter_source) == 0)
        result = TRUE;

    g_free(lcchannel);
    g_free(source);

    return result;
}

static void populate_widget(ChannelList *self)
{
    fprintf(stderr, "channel_list: populate_widget()\n");
    /* filter entry */
    self->priv->filter_entry = gtk_entry_new();
    if (!self->priv->filterable) {
        g_object_set(G_OBJECT(self->priv->filter_entry), "editable", FALSE, NULL);
    }
    g_signal_connect(G_OBJECT(self->priv->filter_entry), "changed",
            G_CALLBACK(_channel_list_filter_changed), self->priv);

    /* channel list */
    self->priv->channel_tree_view = gtk_tree_view_new();

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Source"),
            renderer, "text", CHNL_ROW_SOURCE, "foreground", CHNL_ROW_FOREGROUND, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(self->priv->channel_tree_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Title"),
            renderer, "text", CHNL_ROW_TITLE, "foreground", CHNL_ROW_FOREGROUND, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(self->priv->channel_tree_view), column);

    self->priv->selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(self->priv->channel_tree_view));

    self->priv->channels_widget = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(self->priv->channels_widget),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(self->priv->channels_widget), GTK_SHADOW_ETCHED_IN);
    gtk_container_add(GTK_CONTAINER(self->priv->channels_widget), self->priv->channel_tree_view);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_box_pack_start(GTK_BOX(box), self->priv->filter_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), self->priv->channels_widget, TRUE, TRUE, 0);

    gtk_widget_show_all(box);

    gtk_container_add(GTK_CONTAINER(self), box);
}

static void channel_list_init(ChannelList *self)
{
    fprintf(stderr, "channel_list_init()\n");
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
            CHANNEL_LIST_TYPE, ChannelListPrivate);

    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);

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

    if (channel_list->priv->channels_filter_model)
        g_object_unref(channel_list->priv->channels_filter_model);
    if (model != NULL) {
        channel_list->priv->channels_store = GTK_LIST_STORE(model);

        if (channel_list->priv->filterable) {
            channel_list->priv->channels_filter_model = gtk_tree_model_filter_new(model, NULL);
            gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(channel_list->priv->channels_filter_model),
                    (GtkTreeModelFilterVisibleFunc)_channel_list_visible_func, channel_list, NULL);

            gtk_tree_view_set_model(GTK_TREE_VIEW(channel_list->priv->channel_tree_view),
                    channel_list->priv->channels_filter_model);
        }
        else {
            channel_list->priv->channels_filter_model = NULL;
            gtk_tree_view_set_model(GTK_TREE_VIEW(channel_list->priv->channel_tree_view),
                    GTK_TREE_MODEL(channel_list->priv->channels_store));
        }
    }
    else {
        channel_list->priv->channels_store = NULL;
        channel_list->priv->channels_filter_model = NULL;
        gtk_tree_view_set_model(GTK_TREE_VIEW(channel_list->priv->channel_tree_view), NULL);
    }
}

void channel_list_set_filterable(ChannelList *channel_list, gboolean filterable)
{
    g_return_if_fail(IS_CHANNEL_LIST(channel_list));

    channel_list->priv->filterable = filterable;

    if (channel_list->priv->channels_store) {
        /* do not accidently drop model */
        g_object_ref(G_OBJECT(channel_list->priv->channels_store));
        channel_list_set_model(channel_list, GTK_TREE_MODEL(channel_list->priv->channels_store));
        g_object_unref(G_OBJECT(channel_list->priv->channels_store));
    }

    g_object_set(G_OBJECT(channel_list->priv->filter_entry), "editable", filterable, NULL);
    gtk_widget_set_sensitive(channel_list->priv->filter_entry, filterable);
}

GtkTreeSelection *channel_list_get_selection(ChannelList *channel_list)
{
    g_return_val_if_fail(IS_CHANNEL_LIST(channel_list), NULL);
    return channel_list->priv->selection;
}

GtkListStore *channel_list_get_list_store(ChannelList *channel_list)
{
    g_return_val_if_fail(IS_CHANNEL_LIST(channel_list), NULL);
    return channel_list->priv->channels_store;
}

GtkTreeView *channel_list_get_tree_view(ChannelList *channel_list)
{
    g_return_val_if_fail(IS_CHANNEL_LIST(channel_list), NULL);
    return GTK_TREE_VIEW(channel_list->priv->channel_tree_view);
}

void channel_list_fill_cb(ChannelData *data, ChannelList *channel_list)
{
    g_return_if_fail(IS_CHANNEL_LIST(channel_list));
    g_return_if_fail(data != NULL);

    GtkTreeIter iter;

    gtk_list_store_append(channel_list->priv->channels_store, &iter);
    gtk_list_store_set(channel_list->priv->channels_store, &iter,
           CHNL_ROW_ID, data->id,
           CHNL_ROW_TITLE, data->name,
           CHNL_ROW_SOURCE, data->signalsource,
           CHNL_ROW_FOREGROUND, data->flags & CHNL_FLAG_DIRTY ? "gray" : NULL,
           -1);

/*    GQuark source_quark = g_quark_from_string(data->signalsource);
    if (!g_list_find(*/
}

void channel_list_clear(ChannelList *channel_list)
{
    g_return_if_fail(IS_CHANNEL_LIST(channel_list));

    if (channel_list->priv->channels_store)
        gtk_list_store_clear(channel_list->priv->channels_store);

    g_list_free(channel_list->priv->active_sources);
    channel_list->priv->active_sources = NULL;
}

