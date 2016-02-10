#include "epg-event-widget.h"
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <time.h>

struct _EpgEventWidgetPrivate {
    /* private data */
    GtkWidget *expander;
    GtkWidget *short_desc;
    GtkWidget *extended_desc;
    GtkWidget *items_list;

    EPGEvent *event;
};

G_DEFINE_TYPE(EpgEventWidget, epg_event_widget, GTK_TYPE_BIN);

enum {
    PROP_0,
    PROP_EVENT,
    N_PROPERTIES
};

static void epg_event_widget_dispose(GObject *gobject)
{
    EpgEventWidget *self = EPG_EVENT_WIDGET(gobject);
    epg_event_free(self->priv->event);
    self->priv->event = NULL;

    G_OBJECT_CLASS(epg_event_widget_parent_class)->dispose(gobject);
}

static void epg_event_widget_finalize(GObject *gobject)
{
    /*EpgEventWidget *self = EPG_EVENT_WIDGET(gobject);*/

    G_OBJECT_CLASS(epg_event_widget_parent_class)->finalize(gobject);
}

static void epg_event_widget_set_property(GObject *object, guint prop_id, 
        const GValue *value, GParamSpec *spec)
{
    EpgEventWidget *self = EPG_EVENT_WIDGET(object);

    switch (prop_id) {
        case PROP_EVENT:
            epg_event_widget_set_event(self, g_value_get_pointer(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void epg_event_widget_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *spec)
{
    EpgEventWidget *self = EPG_EVENT_WIDGET(object);

    switch (prop_id) {
    	case PROP_EVENT:
            g_value_set_object(value, self->priv->event);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void epg_event_widget_class_init(EpgEventWidgetClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    /* override GObject methods */
    gobject_class->dispose = epg_event_widget_dispose;
    gobject_class->finalize = epg_event_widget_finalize;
    gobject_class->set_property = epg_event_widget_set_property;
    gobject_class->get_property = epg_event_widget_get_property;

    g_object_class_install_property(gobject_class,
            PROP_EVENT,
            g_param_spec_pointer("event",
                "EPGEvent",
                "The event data",
                G_PARAM_READWRITE));

    g_type_class_add_private(G_OBJECT_CLASS(klass), sizeof(EpgEventWidgetPrivate));
}

void epg_event_widget_update_event(EpgEventWidget *self)
{
    if (self->priv->event == NULL)
        return;

    gchar *buf;
    gchar tbuf[64];
    struct tm *tm;
    /* -> lib: get descriptor for preffered language */
    EPGShortEvent *se = self->priv->event->short_descriptions ?
                        (EPGShortEvent *)self->priv->event->short_descriptions->data : NULL;
    EPGExtendedEvent *ee = self->priv->event->extended_descriptions ?
                           (EPGExtendedEvent *)self->priv->event->extended_descriptions->data : NULL;
    if (self->priv->expander) {
        tm = localtime(&self->priv->event->starttime);
        strftime(tbuf, 64, "%d.%m. %H:%M", tm);
        fprintf(stderr, "se->desc: %s\n", se ? se->description : NULL);
        buf = g_strdup_printf("%s <b>%s</b>", tbuf, se ? se->description : "<i>no title</i>");
        gtk_expander_set_label(GTK_EXPANDER(self->priv->expander), buf);
    }
    if (self->priv->short_desc) {
        gtk_label_set_markup(GTK_LABEL(self->priv->short_desc), se ? se->text : "<i>no description</i>");
    }
    if (self->priv->extended_desc) {
        gtk_label_set_markup(GTK_LABEL(self->priv->extended_desc), ee ? ee->text : "<i>no description</i>");
    }
}

static void populate_widget(EpgEventWidget *self)
{
    self->priv->expander = gtk_expander_new("HH:MM Event title");
    gtk_expander_set_use_markup(GTK_EXPANDER(self->priv->expander), TRUE);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);

    self->priv->short_desc = gtk_label_new("Short Description");
    gtk_widget_set_halign(self->priv->short_desc, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), self->priv->short_desc, FALSE, FALSE, 2);

    self->priv->extended_desc = gtk_label_new("Some longer description");
    gtk_widget_set_halign(self->priv->extended_desc, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), self->priv->extended_desc, FALSE, FALSE, 2);

    gtk_container_add(GTK_CONTAINER(self->priv->expander), vbox);

    gtk_container_add(GTK_CONTAINER(self), self->priv->expander);

    epg_event_widget_update_event(self);
}

static void epg_event_widget_init(EpgEventWidget *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
            EPG_EVENT_WIDGET_TYPE, EpgEventWidgetPrivate);

    gtk_widget_set_has_window(GTK_WIDGET(self), FALSE);

    populate_widget(self);
}

GtkWidget *epg_event_widget_new(EPGEvent *event)
{
    return g_object_new(EPG_EVENT_WIDGET_TYPE, "event", event, NULL);
}

void epg_event_widget_set_event(EpgEventWidget *widget, EPGEvent *event)
{
    g_return_if_fail(IS_EPG_EVENT_WIDGET(widget));

    if (widget->priv->event != event)
        epg_event_free(widget->priv->event);
    widget->priv->event = epg_event_dup(event);

    epg_event_widget_update_event(widget);
}

EPGEvent *epg_event_widget_get_event(EpgEventWidget *widget)
{
    g_return_val_if_fail(IS_EPG_EVENT_WIDGET(widget), NULL);

    return widget->priv->event;
}

