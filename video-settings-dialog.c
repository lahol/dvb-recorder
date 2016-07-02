#include "video-settings-dialog.h"
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include "video-output.h"
#include "config.h"

struct _VideoSettingsDialogPrivate {
    /* private data */
    GtkWindow *parent;

    VideoOutput *vo;

    GtkWidget *scale_brightness;
    GtkWidget *scale_contrast;
    GtkWidget *scale_hue;
    GtkWidget *scale_saturation;

    GtkAdjustment *adjust_brightness;
    GtkAdjustment *adjust_contrast;
    GtkAdjustment *adjust_hue;
    GtkAdjustment *adjust_saturation;

    gint brightness;
    gint contrast;
    gint hue;
    gint saturation;
};

G_DEFINE_TYPE(VideoSettingsDialog, video_settings_dialog, GTK_TYPE_DIALOG);

enum {
    PROP_0,
    PROP_PARENT,
    PROP_VIDEO_OUTPUT,
    PROP_BRIGHTNESS,
    PROP_CONTRAST,
    PROP_HUE,
    PROP_SATURATION,
    N_PROPERTIES
};

static void video_settings_dialog_dispose(GObject *gobject)
{
    /*VideoSettingsDialog *self = VIDEO_SETTINGS_DIALOG(gobject);*/

    G_OBJECT_CLASS(video_settings_dialog_parent_class)->dispose(gobject);
}

static void video_settings_dialog_finalize(GObject *gobject)
{
    /*VideoSettingsDialog *self = VIDEO_SETTINGS_DIALOG(gobject);*/

    G_OBJECT_CLASS(video_settings_dialog_parent_class)->finalize(gobject);
}

static void video_settings_dialog_set_property(GObject *object, guint prop_id, 
        const GValue *value, GParamSpec *spec)
{
    VideoSettingsDialog *self = VIDEO_SETTINGS_DIALOG(object);

    switch (prop_id) {
        case PROP_PARENT:
            video_settings_dialog_set_parent(self, GTK_WINDOW(g_value_get_object(value)));
            break;
        case PROP_VIDEO_OUTPUT:
            self->priv->vo = g_value_get_pointer(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void video_settings_dialog_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *spec)
{
    VideoSettingsDialog *self = VIDEO_SETTINGS_DIALOG(object);

    switch (prop_id) {
        case PROP_PARENT:
            g_value_set_object(value, self->priv->parent);
            break;
        case PROP_BRIGHTNESS:
            g_value_set_int(value, self->priv->brightness);
            break;
        case PROP_CONTRAST:
            g_value_set_int(value, self->priv->contrast);
            break;
        case PROP_HUE:
            g_value_set_int(value, self->priv->hue);
            break;
        case PROP_SATURATION:
            g_value_set_int(value, self->priv->saturation);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void video_settings_dialog_class_init(VideoSettingsDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    /* override GObject methods */
    gobject_class->dispose = video_settings_dialog_dispose;
    gobject_class->finalize = video_settings_dialog_finalize;
    gobject_class->set_property = video_settings_dialog_set_property;
    gobject_class->get_property = video_settings_dialog_get_property;

    g_object_class_install_property(gobject_class,
            PROP_PARENT,
            g_param_spec_object("parent",
                "Parent",
                "Parent",
                GTK_TYPE_WINDOW,
                G_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
            PROP_VIDEO_OUTPUT,
            g_param_spec_pointer("video-output",
                "VideoOutput",
                "The video output to control",
                G_PARAM_WRITABLE));

    g_object_class_install_property(gobject_class,
            PROP_BRIGHTNESS,
            g_param_spec_int("brightness",
                "Brightness",
                "Video brightness",
                -1000,
                1000,
                0,
                G_PARAM_READABLE));

    g_object_class_install_property(gobject_class,
            PROP_CONTRAST,
            g_param_spec_int("contrast",
                "Contrast",
                "Video contrast",
                -1000,
                1000,
                0,
                G_PARAM_READABLE));

    g_object_class_install_property(gobject_class,
            PROP_HUE,
            g_param_spec_int("hue",
                "Hue",
                "Video hue",
                -1000,
                1000,
                0,
                G_PARAM_READABLE));

    g_object_class_install_property(gobject_class,
            PROP_SATURATION,
            g_param_spec_int("saturation",
                "Saturation",
                "Video saturation",
                -1000,
                1000,
                0,
                G_PARAM_READABLE));

    g_type_class_add_private(G_OBJECT_CLASS(klass), sizeof(VideoSettingsDialogPrivate));
}

static void video_settings_dialog_value_changed_brightness(VideoSettingsDialog *dialog, GtkSpinButton *scale)
{
    dialog->priv->brightness = gtk_spin_button_get_value_as_int(scale);
    if (dialog->priv->vo)
        video_output_set_brightness(dialog->priv->vo, dialog->priv->brightness);
}

static void video_settings_dialog_value_changed_contrast(VideoSettingsDialog *dialog, GtkSpinButton *scale)
{
    dialog->priv->contrast = gtk_spin_button_get_value_as_int(scale);
    if (dialog->priv->vo)
        video_output_set_contrast(dialog->priv->vo, dialog->priv->contrast);
}

static void video_settings_dialog_value_changed_hue(VideoSettingsDialog *dialog, GtkSpinButton *scale)
{
    dialog->priv->hue = gtk_spin_button_get_value_as_int(scale);
    if (dialog->priv->vo)
        video_output_set_hue(dialog->priv->vo, dialog->priv->hue);
}

static void video_settings_dialog_value_changed_saturation(VideoSettingsDialog *dialog, GtkSpinButton *scale)
{
    dialog->priv->saturation = gtk_spin_button_get_value_as_int(scale);
    if (dialog->priv->vo)
        video_output_set_saturation(dialog->priv->vo, dialog->priv->saturation);
}

static void populate_widget(VideoSettingsDialog *self)
{
    gtk_dialog_add_buttons(GTK_DIALOG(self),
            _("_Apply"), GTK_RESPONSE_APPLY,
            _("_Close"), GTK_RESPONSE_CLOSE,
            NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(self));

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), FALSE);

    GtkWidget *label = NULL;
    GtkAdjustment *adjust = NULL;
    
    label = gtk_label_new("Brightness:");
    adjust = gtk_adjustment_new((gdouble)self->priv->brightness, -1000, 1000, 1.0, 20, 0);
    self->priv->scale_brightness = gtk_spin_button_new(adjust, 10, 0);
    g_signal_connect_swapped(G_OBJECT(self->priv->scale_brightness), "value-changed",
            G_CALLBACK(video_settings_dialog_value_changed_brightness), self);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), self->priv->scale_brightness, 1, 0, 1, 1);

    label = gtk_label_new("Contrast:");
    adjust = gtk_adjustment_new((gdouble)self->priv->contrast, -1000, 1000, 1.0, 20, 0);
    self->priv->scale_contrast = gtk_spin_button_new(adjust, 10, 0);
    g_signal_connect_swapped(G_OBJECT(self->priv->scale_contrast), "value-changed",
            G_CALLBACK(video_settings_dialog_value_changed_contrast), self);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), self->priv->scale_contrast, 1, 1, 1, 1);

    label = gtk_label_new("Hue:");
    adjust = gtk_adjustment_new((gdouble)self->priv->hue, -1000, 1000, 1.0, 20, 0);
    self->priv->scale_hue = gtk_spin_button_new(adjust, 10, 0);
    g_signal_connect_swapped(G_OBJECT(self->priv->scale_hue), "value-changed",
            G_CALLBACK(video_settings_dialog_value_changed_hue), self);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), self->priv->scale_hue, 1, 2, 1, 1);

    label = gtk_label_new("Saturation:");
    adjust = gtk_adjustment_new((gdouble)self->priv->saturation, -1000, 1000, 1.0, 20, 0);
    self->priv->scale_saturation = gtk_spin_button_new(adjust, 10, 0);
    g_signal_connect_swapped(G_OBJECT(self->priv->scale_saturation), "value-changed",
            G_CALLBACK(video_settings_dialog_value_changed_saturation), self);
    gtk_grid_attach(GTK_GRID(grid), label, 0, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), self->priv->scale_saturation, 1, 3, 1, 1);

    gtk_widget_show_all(grid);

    gtk_box_pack_start(GTK_BOX(content_area), grid, TRUE, TRUE, 3);
}

static void video_settings_dialog_init(VideoSettingsDialog *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
            VIDEO_SETTINGS_DIALOG_TYPE, VideoSettingsDialogPrivate);

/*    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);*/
    if (config_get("video", "brightness", CFG_TYPE_INT, &self->priv->brightness) != 0)
        self->priv->brightness = 0;
    if (config_get("video", "contrast", CFG_TYPE_INT, &self->priv->contrast) != 0)
        self->priv->contrast = 0;
    if (config_get("video", "hue", CFG_TYPE_INT, &self->priv->hue) != 0)
        self->priv->hue = 0;
    if (config_get("video", "saturation", CFG_TYPE_INT, &self->priv->saturation) != 0)
        self->priv->saturation = 0;

    populate_widget(self);
}

GtkWidget *video_settings_dialog_new(GtkWindow *parent)
{
    return g_object_new(VIDEO_SETTINGS_DIALOG_TYPE, "parent", parent, NULL);
}

void video_settings_dialog_set_parent(VideoSettingsDialog *dialog, GtkWindow *parent)
{
    dialog->priv->parent = parent;
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
}
