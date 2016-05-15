#include "ui-epg-event-detail.h"
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include "utils.h"

struct _UiEpgEventDetailPrivate {
    /* private data */
    GtkWidget *title_label;
    GtkWidget *short_description;
    GtkWidget *extended_description;

    EPGEvent *event;
};

G_DEFINE_TYPE(UiEpgEventDetail, ui_epg_event_detail, GTK_TYPE_BIN);

enum {
    PROP_0,
    PROP_EVENT,
    N_PROPERTIES
};

static void ui_epg_event_detail_dispose(GObject *gobject)
{
    UiEpgEventDetail *self = UI_EPG_EVENT_DETAIL(gobject);
    epg_event_free(self->priv->event);
    self->priv->event = NULL;

    G_OBJECT_CLASS(ui_epg_event_detail_parent_class)->dispose(gobject);
}

static void ui_epg_event_detail_finalize(GObject *gobject)
{
    /*UiEpgEventDetail *self = UI_EPG_EVENT_DETAIL(gobject);*/

    G_OBJECT_CLASS(ui_epg_event_detail_parent_class)->finalize(gobject);
}

static void ui_epg_event_detail_set_property(GObject *object, guint prop_id, 
        const GValue *value, GParamSpec *spec)
{
    UiEpgEventDetail *self = UI_EPG_EVENT_DETAIL(object);

    switch (prop_id) {
        case PROP_EVENT:
            ui_epg_event_detail_set_event(self, g_value_get_pointer(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void ui_epg_event_detail_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *spec)
{
    UiEpgEventDetail *self = UI_EPG_EVENT_DETAIL(object);

    switch (prop_id) {
    	case PROP_EVENT:
            g_value_set_object(value, self->priv->event);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void ui_epg_event_detail_class_init(UiEpgEventDetailClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    /* override GObject methods */
    gobject_class->dispose = ui_epg_event_detail_dispose;
    gobject_class->finalize = ui_epg_event_detail_finalize;
    gobject_class->set_property = ui_epg_event_detail_set_property;
    gobject_class->get_property = ui_epg_event_detail_get_property;

    g_object_class_install_property(gobject_class,
            PROP_EVENT,
            g_param_spec_pointer("event",
                "EPGEvent",
                "The event data",
                G_PARAM_READWRITE));

    g_type_class_add_private(G_OBJECT_CLASS(klass), sizeof(UiEpgEventDetailPrivate));
}

static void populate_widget(UiEpgEventDetail *self)
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *desc_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);

    self->priv->title_label = gtk_label_new("Title");
    gtk_widget_set_halign(self->priv->title_label, GTK_ALIGN_START);

    self->priv->short_description = gtk_label_new("Short description");
    gtk_widget_set_halign(self->priv->short_description, GTK_ALIGN_START);

    self->priv->extended_description = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(self->priv->extended_description), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(self->priv->extended_description), GTK_WRAP_WORD_CHAR);

    gtk_box_pack_start(GTK_BOX(vbox), self->priv->title_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(desc_box), self->priv->short_description, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(desc_box), self->priv->extended_description, TRUE, TRUE, 0);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gtk_container_add(GTK_CONTAINER(scroll), desc_box);

    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    gtk_widget_show_all(vbox);
    gtk_container_add(GTK_CONTAINER(self), vbox);
}

static void ui_epg_event_detail_init(UiEpgEventDetail *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
            UI_EPG_EVENT_DETAIL_TYPE, UiEpgEventDetailPrivate);

/*    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);*/

    populate_widget(self);
}

GtkWidget *ui_epg_event_detail_new(void)
{
    return g_object_new(UI_EPG_EVENT_DETAIL_TYPE, NULL);
}

void ui_epg_event_detail_set_event(UiEpgEventDetail *self, EPGEvent *event)
{
    g_return_if_fail(IS_UI_EPG_EVENT_DETAIL(self));

    EPGShortEvent *sev = NULL;
    GtkTextBuffer *buffer;
    gchar *text;

    if (event) {
        sev = event->short_descriptions ? event->short_descriptions->data : NULL;
        if (sev) {
            text = util_translate_control_codes_to_markup(sev->description);
            gtk_label_set_text(GTK_LABEL(self->priv->title_label), text);
            g_free(text);

            text = util_translate_control_codes_to_markup(sev->text);
            gtk_label_set_text(GTK_LABEL(self->priv->short_description), text);
            g_free(text);
        }
        else {
            gtk_label_set_markup(GTK_LABEL(self->priv->title_label), "<i>no description</i>");
            gtk_label_set_markup(GTK_LABEL(self->priv->short_description), "<i>no short description</i>");
        }

        buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->priv->extended_description));
        if (event->extended_descriptions && event->extended_descriptions->data) {
            text = util_translate_control_codes_to_markup(((EPGExtendedEvent *)(event->extended_descriptions->data))->text);
            gtk_text_buffer_set_text(buffer, text, -1);
            g_free(text);
        }
        else {
            gtk_text_buffer_set_text(buffer, "<i>no extended description</i>", -1);
        }
    }

    if (self->priv->event != event)
        epg_event_free(self->priv->event);
    self->priv->event = event;
}
