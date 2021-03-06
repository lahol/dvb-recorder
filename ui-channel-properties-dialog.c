#include "ui-channel-properties-dialog.h"
#include <glib/gi18n.h>
#include <dvbrecorder/channels.h>
#include <dvbrecorder/channel-db.h>

typedef struct _UiChannelPropertiesDialogPrivate {
    /* private data */
    GtkWindow *parent;
    ChannelData *channel_data;
    GtkListStore *properties;
    GtkWidget *tree_view;
} UiChannelPropertiesDialogPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(UiChannelPropertiesDialog, ui_channel_properties_dialog, GTK_TYPE_DIALOG);

enum {
    PROP_0,
    PROP_PARENT,
    PROP_CHANNEL_ID,
    N_PROPERTIES
};

enum {
    CHNL_PROP_KEY = 0,
    CHNL_PROP_VALUE,
    CHNL_PROP_N
};

void ui_channel_properties_dialog_set_parent(UiChannelPropertiesDialog *dialog, GtkWindow *parent);

static void ui_channel_properties_dialog_dispose(GObject *gobject)
{
    UiChannelPropertiesDialog *self = UI_CHANNEL_PROPERTIES_DIALOG(gobject);
    UiChannelPropertiesDialogPrivate *priv = ui_channel_properties_dialog_get_instance_private(self);

    channel_data_free(priv->channel_data);
    priv->channel_data = NULL;

    G_OBJECT_CLASS(ui_channel_properties_dialog_parent_class)->dispose(gobject);
}

static void ui_channel_properties_dialog_finalize(GObject *gobject)
{
    /*UiChannelPropertiesDialog *self = UI_CHANNEL_PROPERTIES_DIALOG(gobject);*/

    G_OBJECT_CLASS(ui_channel_properties_dialog_parent_class)->finalize(gobject);
}

static void ui_channel_properties_dialog_set_property(GObject *object, guint prop_id,
        const GValue *value, GParamSpec *spec)
{
    UiChannelPropertiesDialog *self = UI_CHANNEL_PROPERTIES_DIALOG(object);

    switch (prop_id) {
        case PROP_PARENT:
            ui_channel_properties_dialog_set_parent(self, GTK_WINDOW(g_value_get_object(value)));
            break;
        case PROP_CHANNEL_ID:
            ui_channel_properties_dialog_set_channel_id(self, g_value_get_uint(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void ui_channel_properties_dialog_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *spec)
{
    UiChannelPropertiesDialog *self = UI_CHANNEL_PROPERTIES_DIALOG(object);
    UiChannelPropertiesDialogPrivate *priv = ui_channel_properties_dialog_get_instance_private(self);

    switch (prop_id) {
        case PROP_PARENT:
            g_value_set_object(value, priv->parent);
            break;
        case PROP_CHANNEL_ID:
            g_value_set_uint(value, ui_channel_properties_dialog_get_channel_id(self));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void ui_channel_properties_dialog_class_init(UiChannelPropertiesDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    /* override GObject methods */
    gobject_class->dispose = ui_channel_properties_dialog_dispose;
    gobject_class->finalize = ui_channel_properties_dialog_finalize;
    gobject_class->set_property = ui_channel_properties_dialog_set_property;
    gobject_class->get_property = ui_channel_properties_dialog_get_property;

    g_object_class_install_property(gobject_class,
            PROP_PARENT,
            g_param_spec_object("parent",
                "Parent",
                "Parent",
                GTK_TYPE_WINDOW,
                G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
            PROP_CHANNEL_ID,
            g_param_spec_uint("channel-id",
                "Channel id",
                "Channel id",
                0,
                G_MAXUINT32,
                0,
                G_PARAM_READWRITE));
}

static void populate_widget(UiChannelPropertiesDialog *self)
{
    UiChannelPropertiesDialogPrivate *priv = ui_channel_properties_dialog_get_instance_private(self);
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(self));

    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    priv->tree_view = gtk_tree_view_new();

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Key"),
            renderer, "text", CHNL_PROP_KEY, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->tree_view), column);

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes(_("Value"),
            renderer, "text", CHNL_PROP_VALUE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(priv->tree_view), column);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_ETCHED_IN);
    gtk_container_add(GTK_CONTAINER(scroll), priv->tree_view);

    gtk_box_pack_start(GTK_BOX(content_area), scroll, TRUE, TRUE, 0);

    gtk_widget_show_all(content_area);

    gtk_dialog_add_buttons(GTK_DIALOG(self),
            _("_Close"), GTK_RESPONSE_CLOSE,
            NULL);
}

static void ui_channel_properties_dialog_init(UiChannelPropertiesDialog *self)
{
    UiChannelPropertiesDialogPrivate *priv = ui_channel_properties_dialog_get_instance_private(self);

/*    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);*/
    populate_widget(self);

    priv->properties = gtk_list_store_new(CHNL_PROP_N,
            G_TYPE_STRING,
            G_TYPE_STRING);

    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->tree_view),
            GTK_TREE_MODEL(priv->properties));
}

GtkWidget *ui_channel_properties_dialog_new(GtkWindow *parent)
{
    return g_object_new(UI_CHANNEL_PROPERTIES_DIALOG_TYPE, "parent", parent, NULL);
}

void ui_channel_properties_dialog_set_channel_id(UiChannelPropertiesDialog *dialog, guint32 channel_id)
{
    g_return_if_fail(IS_UI_CHANNEL_PROPERTIES_DIALOG(dialog));
    UiChannelPropertiesDialogPrivate *priv = ui_channel_properties_dialog_get_instance_private(dialog);

    if (priv->channel_data) {
        if (priv->channel_data->id == channel_id)
            return;
        channel_data_free(priv->channel_data);
    }

    priv->channel_data = channel_db_get_channel(channel_id);

    gtk_list_store_clear(priv->properties);

    if (!priv->channel_data)
        return;

    GtkTreeIter iter;
    gchar numbuf[64];
    gchar *buf;
    ChannelData *data = priv->channel_data;

#define SET_VALUE(key, value) do {\
    gtk_list_store_append(priv->properties, &iter);\
    gtk_list_store_set(priv->properties, &iter,\
            CHNL_PROP_KEY, (key),\
            CHNL_PROP_VALUE, (value),\
            -1);\
    } while (0)
#define SET_VALUE_UINT(key, value) do {\
    g_snprintf(numbuf, 64, "%u", (value));\
    SET_VALUE((key), numbuf);\
    } while (0)
#define SET_VALUE_FORMAT(key, format, value) do {\
    buf = g_strdup_printf((format), (value));\
    SET_VALUE((key), buf);\
    } while (0)

    SET_VALUE_UINT("Id", data->id);
    SET_VALUE("Name", data->name);
    SET_VALUE("Provider", data->provider);
    SET_VALUE("Signal source", data->signalsource);
    SET_VALUE_FORMAT("Frequency", "%f GHz", data->frequency * 0.001f);
    SET_VALUE("Polarization", data->polarization == CHNL_POLARIZATION_HORIZONTAL ? "horizontal" : "vertical");
    SET_VALUE_UINT("Symbolrate", data->srate);
    SET_VALUE("Video-PID", data->vpid);
    SET_VALUE("Audio-PID", data->apid);
    SET_VALUE_UINT("Teletext-PID", data->tpid);
    SET_VALUE_UINT("Service ID", data->sid);
    SET_VALUE_UINT("Network ID", data->nid);
    SET_VALUE_UINT("Transports stream ID", data->tid);

#undef SET_VALUE_FORMAT
#undef SET_VALUE_UINT
#undef SET_VALUE
}

guint32 ui_channel_properties_dialog_get_channel_id(UiChannelPropertiesDialog *dialog)
{
    g_return_val_if_fail(IS_UI_CHANNEL_PROPERTIES_DIALOG(dialog), 0);
    UiChannelPropertiesDialogPrivate *priv = ui_channel_properties_dialog_get_instance_private(dialog);

    if (priv->channel_data)
        return priv->channel_data->id;
    return 0;
}

void ui_channel_properties_dialog_set_parent(UiChannelPropertiesDialog *dialog, GtkWindow *parent)
{
    UiChannelPropertiesDialogPrivate *priv = ui_channel_properties_dialog_get_instance_private(dialog);
    priv->parent = parent;
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_modal(GTK_WINDOW(dialog), FALSE);
}
