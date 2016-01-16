#include "ui-dialog-scan.h"
#include "config.h"
#include <dvbrecorder/channels.h>
#include <dvbrecorder/channel-db.h>
#include <stdio.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <sys/types.h>
#include <signal.h>

struct _UiDialogScanPrivate {
    /* private data */
    GtkWindow *parent;

    GtkWidget *satellite_combo;
    GtkWidget *scan_button;
    GtkWidget *status_label;

    GtkWidget *spinner;

    GMutex scan_mutex;
    const gchar *satellite;

    GList *scanned_channels;
    GList *scanned_satellites;

    guint32 status_scanning : 1;

    guint32 channels_found;

    GPid child_pid;
};

G_DEFINE_TYPE(UiDialogScan, ui_dialog_scan, GTK_TYPE_DIALOG);

enum {
    PROP_0,
    PROP_PARENT,
    N_PROPERTIES
};

void ui_dialog_scan_set_parent(UiDialogScan *dialog, GtkWindow *parent);

static void ui_dialog_scan_child_watch_cb(GPid pid, gint status, UiDialogScan *self)
{
    fprintf(stderr, "child exited with status %d\n", status);
    if (GTK_IS_SPINNER(self->priv->spinner))
        gtk_spinner_stop(GTK_SPINNER(self->priv->spinner));
    g_spawn_close_pid(pid);
    self->priv->child_pid = 0;
}

static gboolean ui_dialog_scan_watch_out_cb(GIOChannel *channel, GIOCondition cond, UiDialogScan *self)
{
    gchar *string = NULL;
    gchar *label_text = NULL;
    gsize size;

    if (cond == G_IO_HUP) {
        g_io_channel_unref(channel);
        fprintf(stderr, "got hup\n");
        return FALSE;
    }

    g_io_channel_read_line(channel, &string, &size, NULL, NULL);

/*    fprintf(stderr, "read line: %s", string);*/

    g_mutex_lock(&self->priv->scan_mutex);
    ChannelData *data = channel_data_parse(string, self->priv->satellite);
    if (data != NULL) {
        self->priv->scanned_channels = g_list_prepend(self->priv->scanned_channels, data);
        label_text = g_strdup_printf("Found %d channels.\n", ++self->priv->channels_found);
        gtk_label_set_text(GTK_LABEL(self->priv->status_label), label_text);
    }
    g_mutex_unlock(&self->priv->scan_mutex);

    g_free(string);
    g_free(label_text);
    return TRUE;
}

static void ui_dialog_scan_start_scan(UiDialogScan *self)
{
    self->priv->channels_found = 0;
    const gchar *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(self->priv->satellite_combo));
    if (id == NULL)
        return;
    
    gchar *cmd_raw = NULL;
    if (config_get("dvb", "scan-command", CFG_TYPE_STRING, &cmd_raw) != 0)
        return;

    GRegex *cmd_regex = g_regex_new("\\${satellite}", G_REGEX_RAW, 0, NULL);
    gchar *cmd = g_regex_replace_literal(cmd_regex, cmd_raw, -1, 0, id, 0, NULL);

    fprintf(stderr, "Command: %s\n", cmd);

    gint argc = 0;
    gchar **argv = NULL;

    if (!g_shell_parse_argv(cmd, &argc, &argv, NULL))
        goto err;

    self->priv->satellite = id;
    self->priv->scanned_satellites = g_list_prepend(self->priv->scanned_satellites,
                                                    g_strdup(id));

    GIOChannel *out_ch;
    gint out;
    gboolean ret;

    ret = g_spawn_async_with_pipes(NULL, argv, NULL,
            G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL,
            NULL, &self->priv->child_pid, NULL, &out, NULL, NULL);

    if (!ret) {
        gtk_label_set_text(GTK_LABEL(self->priv->status_label), "Could not spawn child process.");
        goto err;
    }

    gtk_spinner_start(GTK_SPINNER(self->priv->spinner));

    g_child_watch_add(self->priv->child_pid, (GChildWatchFunc)ui_dialog_scan_child_watch_cb, self);

    out_ch = g_io_channel_unix_new(out);
    g_io_add_watch(out_ch, G_IO_IN | G_IO_HUP, (GIOFunc)ui_dialog_scan_watch_out_cb, self);

err:
    g_strfreev(argv);
    g_regex_unref(cmd_regex);
    g_free(cmd_raw);
    g_free(cmd);
}

GList *ui_dialog_scan_get_scanned_satellites(UiDialogScan *dialog)
{
    g_return_val_if_fail(IS_UI_DIALOG_SCAN(dialog), NULL);

    return dialog->priv->scanned_satellites;
}

GList *ui_dialog_scan_get_channel_results(UiDialogScan *dialog)
{
    g_return_val_if_fail(IS_UI_DIALOG_SCAN(dialog), NULL);

    return dialog->priv->scanned_channels;
}

static void ui_dialog_scan_dispose(GObject *gobject)
{
    UiDialogScan *self = UI_DIALOG_SCAN(gobject);

    g_list_free_full(self->priv->scanned_channels,
            (GDestroyNotify)channel_data_free);
    self->priv->scanned_channels = NULL;

    g_list_free_full(self->priv->scanned_satellites,
            (GDestroyNotify)g_free);
    self->priv->scanned_satellites = NULL;

    G_OBJECT_CLASS(ui_dialog_scan_parent_class)->dispose(gobject);
}

static void ui_dialog_scan_finalize(GObject *gobject)
{
    /*UiDialogScan *self = UI_DIALOG_SCAN(gobject);*/

    G_OBJECT_CLASS(ui_dialog_scan_parent_class)->finalize(gobject);
}

static void ui_dialog_scan_set_property(GObject *object, guint prop_id, 
        const GValue *value, GParamSpec *spec)
{
    UiDialogScan *self = UI_DIALOG_SCAN(object);

    switch (prop_id) {
        case PROP_PARENT:
            ui_dialog_scan_set_parent(self, GTK_WINDOW(g_value_get_object(value)));
	    break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void ui_dialog_scan_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *spec)
{
    UiDialogScan *self = UI_DIALOG_SCAN(object);

    switch (prop_id) {
    	case PROP_PARENT:
            g_value_set_object(value, self->priv->parent);
	    break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void ui_dialog_scan_class_init(UiDialogScanClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    /* override GObject methods */
    gobject_class->dispose = ui_dialog_scan_dispose;
    gobject_class->finalize = ui_dialog_scan_finalize;
    gobject_class->set_property = ui_dialog_scan_set_property;
    gobject_class->get_property = ui_dialog_scan_get_property;

    g_object_class_install_property(gobject_class,
            PROP_PARENT,
            g_param_spec_object("parent",
                "Parent",
                "Parent",
                GTK_TYPE_WINDOW,
                G_PARAM_READWRITE));

    g_type_class_add_private(G_OBJECT_CLASS(klass), sizeof(UiDialogScanPrivate));
}

static void populate_widget(UiDialogScan *self)
{
    gtk_dialog_add_buttons(GTK_DIALOG(self),
            _("_Ok"), GTK_RESPONSE_OK,
            _("_Close"), GTK_RESPONSE_CLOSE,
            NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(self));

    GtkWidget *hbox;
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);

    self->priv->satellite_combo = gtk_combo_box_text_new();

    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(self->priv->satellite_combo), "S19E2", "Astra 19.2");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(self->priv->satellite_combo), "S23E5", "Astra S23.5E");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(self->priv->satellite_combo), "S13E0", "Hotbird S13.0E");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(self->priv->satellite_combo), "S28E2", "Astra S28.2E");

    gtk_box_pack_start(GTK_BOX(hbox), self->priv->satellite_combo, TRUE, TRUE, 3);

    self->priv->scan_button = gtk_button_new_with_label("Start Scan");
    g_signal_connect_swapped(G_OBJECT(self->priv->scan_button), "clicked",
            G_CALLBACK(ui_dialog_scan_start_scan), self);
    gtk_box_pack_start(GTK_BOX(hbox), self->priv->scan_button, FALSE, FALSE, 3);

    gtk_widget_show_all(hbox);

    gtk_box_pack_start(GTK_BOX(content_area), hbox, TRUE, TRUE, 3);

    self->priv->spinner = gtk_spinner_new();
    gtk_widget_show(self->priv->spinner);
    gtk_box_pack_start(GTK_BOX(content_area), self->priv->spinner, TRUE, TRUE, 3);

    self->priv->status_label = gtk_label_new(NULL);
    gtk_widget_show(self->priv->status_label);
    gtk_box_pack_start(GTK_BOX(content_area), self->priv->status_label, TRUE, TRUE, 3);
}

static void ui_dialog_scan_init(UiDialogScan *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
            UI_DIALOG_SCAN_TYPE, UiDialogScanPrivate);

    g_mutex_init(&self->priv->scan_mutex);

    populate_widget(self);
}

GtkWidget *ui_dialog_scan_new(GtkWindow *parent)
{
    return g_object_new(UI_DIALOG_SCAN_TYPE, "parent", parent, NULL);
}

void ui_dialog_scan_set_parent(UiDialogScan *dialog, GtkWindow *parent)
{
    dialog->priv->parent = parent;
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
}

static void _channel_copy_to_list(ChannelData *data, GList **list)
{
    if (list)
        *list = g_list_prepend(*list, channel_data_dup(data));
}

static gboolean _ui_dialog_is_scanned_satellite(ChannelData *data, GList *scanned_satellites)
{
    if (data == NULL || scanned_satellites == NULL)
        return FALSE;
    if (data->signalsource == NULL || data->signalsource[0] == '\0')
        return FALSE;

    GList *tmp;
    static const gchar *translations[] = {
        "S19E2", "S19.2E",
        "S23E5", "S23.E5",
        "S13E0", "S13E",
        "S28E2", "S28.2E",
        NULL, NULL
    };

    guint i;
    for (i = 0; translations[i] != NULL; i += 2)
    {
        if (g_strcmp0(data->signalsource, translations[i+1]) == 0) {
           for (tmp = scanned_satellites; tmp != NULL; tmp = g_list_next(tmp)) {
               if (g_strcmp0((gchar *)tmp->data, translations[i]) == 0)
                   return TRUE;
           }
           break;
        }
    }

    return FALSE;
}

static gint _ui_dialog_compare_channel_nid_sid(ChannelData *a, ChannelData *b)
{
    if (a->nid == b->nid && a->sid == b->sid)
        return 0;
    if (a->nid > b->nid)
        return 1;
    else if (a->nid < b->nid)
        return -1;
    if (a->sid > b->sid)
        return 1;
    else if (a->sid < b->sid)
        return -1;
    return 1;
}

void ui_dialog_scan_update_channels_db(UiDialogScan *dialog)
{
    GList *old_channels = NULL;
    GList *scanned_satellites = ui_dialog_scan_get_scanned_satellites(dialog);
    if (scanned_satellites == NULL)
        return;
    GList *scanned_channels = ui_dialog_scan_get_channel_results(dialog);
    GList *tmp, *match;
    GList *new_channels = NULL;

    /* get all channels from db */
    channel_db_foreach(0, (CHANNEL_DB_FOREACH_CALLBACK)_channel_copy_to_list, &old_channels);

    /* if old channel comes from one of the scanned channels mark as dirty */
    for (tmp = old_channels; tmp != NULL; tmp = g_list_next(tmp)) {
        if (_ui_dialog_is_scanned_satellite((ChannelData *)tmp->data, scanned_satellites)) {
            ((ChannelData *)tmp->data)->flags |= CHNL_FLAG_DIRTY;
        }
    }
    /* for each scanned channel, look up in list if nid, sid match
     *  if found: update data, mark clean
     *  else: append to new_list */
    for (tmp = scanned_channels; tmp != NULL; tmp = g_list_next(tmp)) {
        match = g_list_find_custom(old_channels, tmp->data, (GCompareFunc)_ui_dialog_compare_channel_nid_sid);
        if (match != NULL) {
            channel_data_update_payload((ChannelData *)match->data, (ChannelData *)tmp->data);
            ((ChannelData *)match->data)->flags &= ~CHNL_FLAG_DIRTY;
        }
        else {
            new_channels = g_list_prepend(new_channels, channel_data_dup((ChannelData *)tmp->data));
        }
    }

    /* write lists back to db */
    channel_db_start_transaction();
    for (tmp = old_channels; tmp != NULL; tmp = g_list_next(tmp)) {
        channel_db_set_channel((ChannelData *)tmp->data);
    }
    for (tmp = new_channels; tmp != NULL; tmp = g_list_next(tmp)) {
        channel_db_set_channel((ChannelData *)tmp->data);
    }
    channel_db_commit_transaction();

    g_list_free_full(old_channels, (GDestroyNotify)channel_data_free);
    g_list_free_full(new_channels, (GDestroyNotify)channel_data_free);
}

GtkResponseType ui_dialog_scan_show(GtkWidget *parent)
{
    GtkWidget *dialog = ui_dialog_scan_new(GTK_WINDOW(parent));

    GtkResponseType result = gtk_dialog_run(GTK_DIALOG(dialog));
    if (result == GTK_RESPONSE_OK) {
        ui_dialog_scan_update_channels_db(UI_DIALOG_SCAN(dialog));
    }
    else if (result == GTK_RESPONSE_CLOSE) {
        if (UI_DIALOG_SCAN(dialog)->priv->child_pid > 0) {
            kill(UI_DIALOG_SCAN(dialog)->priv->child_pid, SIGKILL);
        }
    }

    gtk_widget_destroy(dialog);

    return result;
}

