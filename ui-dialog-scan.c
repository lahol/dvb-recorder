#include "ui-dialog-scan.h"
#include "config.h"
#include <dvbrecorder/channels.h>
#include <dvbrecorder/channel-db.h>
#include <dvbrecorder/dvb-scanner.h>
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

    guint32 channels_found;

    DVBScanner *scanner;
};

G_DEFINE_TYPE(UiDialogScan, ui_dialog_scan, GTK_TYPE_DIALOG);

enum {
    PROP_0,
    PROP_PARENT,
    N_PROPERTIES
};

void ui_dialog_scan_set_parent(UiDialogScan *dialog, GtkWindow *parent);

static void ui_dialog_scan_scan_started(UiDialogScan *dialog, DVBScanner *scanner)
{
    gtk_spinner_start(GTK_SPINNER(dialog->priv->spinner));
}

static void ui_dialog_scan_scan_finished(UiDialogScan *dialog, DVBScanner *scanner)
{
    if (GTK_IS_SPINNER(dialog->priv->spinner))
        gtk_spinner_stop(GTK_SPINNER(dialog->priv->spinner));
}

static void ui_dialog_scan_channel_found(UiDialogScan *dialog, ChannelData *channel, DVBScanner *scanner)
{
    gchar *label_text = NULL;

    label_text = g_strdup_printf("Found %d channels.\n", ++dialog->priv->channels_found);
    gtk_label_set_text(GTK_LABEL(dialog->priv->status_label), label_text);

    g_free(label_text);
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

    dvb_scanner_set_satellite(self->priv->scanner, id);
    dvb_scanner_set_scan_command(self->priv->scanner, cmd_raw);
    g_free(cmd_raw);

    dvb_scanner_start(self->priv->scanner);
}

static void ui_dialog_scan_dispose(GObject *gobject)
{
    UiDialogScan *self = UI_DIALOG_SCAN(gobject);

    g_clear_object(&self->priv->scanner);

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
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(self->priv->satellite_combo), "S9E0", "Eurobird 9");

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

    self->priv->scanner = dvb_scanner_new();
    g_signal_connect_swapped(G_OBJECT(self->priv->scanner), "scan-started",
            G_CALLBACK(ui_dialog_scan_scan_started), self);
    g_signal_connect_swapped(G_OBJECT(self->priv->scanner), "scan-finished",
            G_CALLBACK(ui_dialog_scan_scan_finished), self);
    g_signal_connect_swapped(G_OBJECT(self->priv->scanner), "channel-found",
            G_CALLBACK(ui_dialog_scan_channel_found), self);

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

GtkResponseType ui_dialog_scan_show(GtkWidget *parent)
{
    GtkWidget *dialog = ui_dialog_scan_new(GTK_WINDOW(parent));

    GtkResponseType result = gtk_dialog_run(GTK_DIALOG(dialog));
    dvb_scanner_stop(UI_DIALOG_SCAN(dialog)->priv->scanner);
    if (result == GTK_RESPONSE_OK) {
        dvb_scanner_update_channels_db(UI_DIALOG_SCAN(dialog)->priv->scanner);
    }

    gtk_widget_destroy(dialog);

    return result;
}
