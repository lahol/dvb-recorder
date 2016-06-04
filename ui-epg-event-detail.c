#include "ui-epg-event-detail.h"
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include "utils.h"

struct _UiEpgEventDetailPrivate {
    /* private data */
    GtkWidget *title_label;
    GtkWidget *description;

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
            g_value_set_pointer(value, self->priv->event);
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

    self->priv->description = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(self->priv->description), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(self->priv->description), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_justification(GTK_TEXT_VIEW(self->priv->description), GTK_JUSTIFY_FILL);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(self->priv->description), 5);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(self->priv->description), 13);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(self->priv->description), 2);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(self->priv->description), 2);

    gtk_box_pack_start(GTK_BOX(vbox), self->priv->title_label, FALSE, FALSE, 0);


    /* FIXME: text view should resize (horizontal) when widget is resized */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll), GTK_SHADOW_ETCHED_IN);

    gtk_box_pack_start(GTK_BOX(desc_box), self->priv->description, TRUE, TRUE, 0);
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

/* extra short event to hand over control of which short event is to be printed to a higher level */
gchar *_ui_epg_event_detail_format_header(EPGEvent *event, EPGShortEvent *sev)
{
    if (event == NULL)
        return g_strdup("<i>no description</i>");

    GString *content = g_string_new(NULL);
    gchar buf[256];
    gchar *buffer = NULL;
    if (sev) {
        buffer = util_translate_control_codes_to_markup(sev->description);
    }

    if (buffer) {
        g_string_append_printf(content, "<b>%s</b>\n", buffer);
        g_free(buffer);
    }
    else {
        g_string_append(content, "<i>no description</i>\n");
    }

    util_time_to_string(buf, 256, event->starttime, TRUE);
    g_string_append(content, buf);
    
    util_duration_to_string(buf, 256, event->duration);
    g_string_append_printf(content, " – %s", buf);

    return g_string_free(content, FALSE);
}

gchar *_ui_epg_event_detail_format_description(EPGShortEvent *sh_event, EPGExtendedEvent *ext_event)
{
    if (ext_event == NULL && sh_event == NULL)
        return g_strdup("<i>no description available</i>");;

    GString *content = g_string_new(NULL);

    gchar *buffer;
    if (sh_event) {
        buffer = util_translate_control_codes_to_markup(sh_event->text);
        if (buffer) {
            g_string_append(content, buffer);
            g_free(buffer);
            g_string_append(content, "\n\n");
        }
    }

    if (ext_event == NULL)
        return g_string_free(content, FALSE);

    buffer = util_translate_control_codes_to_markup(ext_event->text);
    if (buffer) {
        g_string_append(content, buffer);
        g_free(buffer);
    }

    GList *tmp;
    EPGExtendedEventItem *desc;
    for (tmp = ext_event->description_items; tmp; tmp = g_list_next(tmp)) {
        desc = (EPGExtendedEventItem *)tmp->data;
        g_string_append_c(content, '\n');

        buffer = util_translate_control_codes_to_markup(desc->description);
        if (buffer) {
            g_string_append(content, buffer);
            g_free(buffer);
        }

        g_string_append_c(content, '\t');

        buffer = util_translate_control_codes_to_markup(desc->content);
        if (buffer) {
            g_string_append(content, buffer);
            g_free(buffer);
        }
    }

    return g_string_free(content, FALSE);
}

void ui_epg_event_detail_set_event(UiEpgEventDetail *self, EPGEvent *event)
{
    g_return_if_fail(IS_UI_EPG_EVENT_DETAIL(self));

    EPGShortEvent *sev = NULL;
    EPGExtendedEvent *eev = NULL;
    GtkTextBuffer *buffer;
    gchar *text;

    if (event) {
        sev = event->short_descriptions ? event->short_descriptions->data : NULL;
        eev = event->extended_descriptions ? event->extended_descriptions->data : NULL;

        text = _ui_epg_event_detail_format_header(event, sev);
        gtk_label_set_markup(GTK_LABEL(self->priv->title_label), text);
        g_free(text);

        buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(self->priv->description));

        text = _ui_epg_event_detail_format_description(sev, eev);
        gtk_text_buffer_set_text(buffer, text, -1);
        g_free(text);
     }

    if (self->priv->event != event)
        epg_event_free(self->priv->event);
    self->priv->event = event;
}
