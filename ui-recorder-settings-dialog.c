#include "ui-recorder-settings-dialog.h"
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <dvbrecorder/filter.h>
#include "config.h"

enum {
    FILTER_CHECK_VIDEO = 0,
    FILTER_CHECK_AUDIO,
    FILTER_CHECK_TELETEXT,
    FILTER_CHECK_SUBTITLES,
    FILTER_CHECK_PAT,
    FILTER_CHECK_PMT,
    FILTER_CHECK_EIT,
    FILTER_CHECK_SDT,
    FILTER_CHECK_RST,
    FILTER_CHECK_OTHER,
    N_FILTER_CHECK
};

struct _FilterCheckEntry {
    guint id;
    DVBFilterType flag;
    const gchar *title;
} filter_check_entries[] = {
    { FILTER_CHECK_VIDEO, DVB_FILTER_VIDEO, "Video" },
    { FILTER_CHECK_AUDIO, DVB_FILTER_AUDIO, "Audio" },
    { FILTER_CHECK_TELETEXT, DVB_FILTER_TELETEXT, "Teletext" },
    { FILTER_CHECK_SUBTITLES, DVB_FILTER_SUBTITLES, "Subtitles" },
    { FILTER_CHECK_PAT, DVB_FILTER_PAT, "PAT" },
    { FILTER_CHECK_PMT, DVB_FILTER_PMT, "PMT" },
    { FILTER_CHECK_EIT, DVB_FILTER_EIT, "EIT" },
    { FILTER_CHECK_SDT, DVB_FILTER_SDT, "SDT" },
    { FILTER_CHECK_RST, DVB_FILTER_RST, "RST" },
    { FILTER_CHECK_OTHER, DVB_FILTER_OTHER, "Other" }
};

struct _UiRecorderSettingsDialogPrivate {
    GtkWindow *parent;

    DVBRecorder *recorder;

    DVBFilterType filter;

    GtkWidget *checkbuttons[N_FILTER_CHECK];
};

G_DEFINE_TYPE(UiRecorderSettingsDialog, ui_recorder_settings_dialog, GTK_TYPE_DIALOG);

enum {
    PROP_0,
    PROP_PARENT,
    PROP_RECORDER,
    N_PROPERTIES
};

void ui_recorder_settings_dialog_check_filter(UiRecorderSettingsDialog *dialog);
void ui_recorder_settings_dialog_set_recorder_filter(UiRecorderSettingsDialog *dialog);


static void ui_recorder_settings_dialog_dispose(GObject *gobject)
{
    /*UiRecorderSettingsDialog *self = UI_RECORDER_SETTINGS_DIALOG(gobject);*/

    G_OBJECT_CLASS(ui_recorder_settings_dialog_parent_class)->dispose(gobject);
}

static void ui_recorder_settings_dialog_finalize(GObject *gobject)
{
    /*UiRecorderSettingsDialog *self = UI_RECORDER_SETTINGS_DIALOG(gobject);*/

    G_OBJECT_CLASS(ui_recorder_settings_dialog_parent_class)->finalize(gobject);
}

static void ui_recorder_settings_dialog_set_property(GObject *object, guint prop_id, 
        const GValue *value, GParamSpec *spec)
{
    UiRecorderSettingsDialog *self = UI_RECORDER_SETTINGS_DIALOG(object);

    switch (prop_id) {
        case PROP_PARENT:
            ui_recorder_settings_dialog_set_parent(self, GTK_WINDOW(g_value_get_object(value)));
            break;
        case PROP_RECORDER:
            ui_recorder_settings_dialog_set_recorder(self, g_value_get_pointer(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void ui_recorder_settings_dialog_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *spec)
{
    UiRecorderSettingsDialog *self = UI_RECORDER_SETTINGS_DIALOG(object);

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

static void ui_recorder_settings_dialog_class_init(UiRecorderSettingsDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    /* override GObject methods */
    gobject_class->dispose = ui_recorder_settings_dialog_dispose;
    gobject_class->finalize = ui_recorder_settings_dialog_finalize;
    gobject_class->set_property = ui_recorder_settings_dialog_set_property;
    gobject_class->get_property = ui_recorder_settings_dialog_get_property;

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

    g_type_class_add_private(G_OBJECT_CLASS(klass), sizeof(UiRecorderSettingsDialogPrivate));
}

void ui_recorder_settings_dialog_response_cb(GtkDialog *dialog, gint response_id, gpointer data)
{
    fprintf(stderr, "response cb\n");
    if (response_id == GTK_RESPONSE_CLOSE || response_id == GTK_RESPONSE_DELETE_EVENT) {
        gtk_widget_destroy(GTK_WIDGET(dialog));
    }
}

static void populate_widget(UiRecorderSettingsDialog *self)
{
    fprintf(stderr, "populate widget\n");
    /* video, audio, teletext, subtitles, pat, pmt, eit, sdt, rst, other */
    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(self));

    guint i;
    for (i = 0; i < N_FILTER_CHECK; ++i) {
        self->priv->checkbuttons[i] = gtk_check_button_new_with_label(filter_check_entries[i].title);
        gtk_box_pack_start(GTK_BOX(content_area), self->priv->checkbuttons[i], FALSE, FALSE, 3);
        gtk_widget_show(self->priv->checkbuttons[i]);
    }

    gtk_dialog_add_buttons(GTK_DIALOG(self),
            _("_Apply"), GTK_RESPONSE_APPLY,
            _("_Close"), GTK_RESPONSE_CLOSE,
            NULL);

/*    gtk_widget_show_all(content_area);*/
}

static void ui_recorder_settings_dialog_init(UiRecorderSettingsDialog *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
            UI_RECORDER_SETTINGS_DIALOG_TYPE, UiRecorderSettingsDialogPrivate);

    gtk_window_set_title(GTK_WINDOW(self), _("Recorder settings"));

    populate_widget(self);
}

GtkWidget *ui_recorder_settings_dialog_new(GtkWindow *parent, DVBRecorder *recorder)
{
    return g_object_new(UI_RECORDER_SETTINGS_DIALOG_TYPE,
            "parent", parent,
            "recorder", recorder,
            NULL);
}

void ui_recorder_settings_dialog_set_parent(UiRecorderSettingsDialog *dialog, GtkWindow *parent)
{
    fprintf(stderr, "set parent\n");
    dialog->priv->parent = parent;
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
}

void ui_recorder_settings_dialog_set_recorder(UiRecorderSettingsDialog *dialog, DVBRecorder *recorder)
{
    fprintf(stderr, "ui_recorder_settings_dialog_set_recorder\n");
    dialog->priv->recorder = recorder;
    ui_recorder_settings_dialog_check_filter(dialog);
}

void ui_recorder_settings_dialog_check_filter(UiRecorderSettingsDialog *dialog)
{
    DVBFilterType filter = dvb_recorder_get_record_filter(dialog->priv->recorder);
    guint i;
    for (i = 0; i < N_FILTER_CHECK; ++i) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->priv->checkbuttons[i]),
                (filter & filter_check_entries[i].flag));
    }
}

void ui_recorder_settings_dialog_set_recorder_filter(UiRecorderSettingsDialog *dialog)
{
    DVBFilterType filter = DVB_FILTER_ALL;
    guint i;
    for (i = 0; i < N_FILTER_CHECK; ++i) {
        if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->priv->checkbuttons[i])))
            filter &= ~filter_check_entries[i].flag;
    }

    dvb_recorder_set_record_filter(dialog->priv->recorder, filter);
}

void ui_recorder_settings_dialog_show(GtkWidget *parent, DVBRecorder *recorder)
{
    GtkWidget *dialog = ui_recorder_settings_dialog_new(GTK_WINDOW(parent), recorder);

    GtkResponseType result = gtk_dialog_run(GTK_DIALOG(dialog));

    if (result == GTK_RESPONSE_APPLY) {
        ui_recorder_settings_dialog_set_recorder_filter(UI_RECORDER_SETTINGS_DIALOG(dialog));
        config_set("dvb", "record-streams", CFG_TYPE_INT,
                GINT_TO_POINTER(dvb_recorder_get_record_filter(recorder)));
    }

    fprintf(stderr, "destroying\n");
    if (GTK_IS_DIALOG(dialog))
        gtk_widget_destroy(dialog);
    fprintf(stderr, "destroyed\n");
}


