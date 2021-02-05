#include "epg-event-widget.h"
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <time.h>

typedef struct _EpgEventWidgetPrivate {
    /* private data */
    GtkWidget *expander;
    GtkWidget *short_desc;
    GtkWidget *extended_desc;
    GtkWidget *items_list;

    EPGEvent *event;
} EpgEventWidgetPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(EpgEventWidget, epg_event_widget, GTK_TYPE_BIN);

enum {
    PROP_0,
    PROP_EVENT,
    N_PROPERTIES
};

static void epg_event_widget_dispose(GObject *gobject)
{
    EpgEventWidget *self = EPG_EVENT_WIDGET(gobject);
    EpgEventWidgetPrivate *priv = epg_event_widget_get_instance_private(self);
    epg_event_free(priv->event);
    priv->event = NULL;

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
    EpgEventWidgetPrivate *priv = epg_event_widget_get_instance_private(self);

    switch (prop_id) {
    	case PROP_EVENT:
            g_value_set_object(value, priv->event);
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
}

void epg_event_widget_update_event(EpgEventWidget *self)
{
    EpgEventWidgetPrivate *priv = epg_event_widget_get_instance_private(self);
    if (priv->event == NULL)
        return;

    gchar *buf;
    gchar tbuf[64];
    struct tm *tm;
    /* -> lib: get descriptor for preffered language */
    EPGShortEvent *se = priv->event->short_descriptions ?
                        (EPGShortEvent *)priv->event->short_descriptions->data : NULL;
    EPGExtendedEvent *ee = priv->event->extended_descriptions ?
                           (EPGExtendedEvent *)priv->event->extended_descriptions->data : NULL;
    if (priv->expander) {
        tm = localtime(&priv->event->starttime);
        strftime(tbuf, 64, "%d.%m. %H:%M", tm);
        fprintf(stderr, "se->desc: %s\n", se ? se->description : NULL);
        buf = g_strdup_printf("%s <b>%s</b>", tbuf, se ? se->description : "<i>no title</i>");
        gtk_expander_set_label(GTK_EXPANDER(priv->expander), buf);
    }
    if (priv->short_desc) {
        gtk_label_set_markup(GTK_LABEL(priv->short_desc), se ? se->text : "<i>no description</i>");
    }
    if (priv->extended_desc) {
        gtk_label_set_markup(GTK_LABEL(priv->extended_desc), ee ? ee->text : "<i>no description</i>");
    }
}

static void populate_widget(EpgEventWidget *self)
{
    EpgEventWidgetPrivate *priv = epg_event_widget_get_instance_private(self);
    priv->expander = gtk_expander_new("HH:MM Event title");
    gtk_expander_set_use_markup(GTK_EXPANDER(priv->expander), TRUE);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);

    priv->short_desc = gtk_label_new("Short Description");
    gtk_widget_set_halign(priv->short_desc, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), priv->short_desc, FALSE, FALSE, 2);

    priv->extended_desc = gtk_label_new("Some longer description");
    gtk_widget_set_halign(priv->extended_desc, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), priv->extended_desc, FALSE, FALSE, 2);

    gtk_container_add(GTK_CONTAINER(priv->expander), vbox);

    gtk_container_add(GTK_CONTAINER(self), priv->expander);

    epg_event_widget_update_event(self);
}

static void epg_event_widget_init(EpgEventWidget *self)
{
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
    EpgEventWidgetPrivate *priv = epg_event_widget_get_instance_private(widget);

    if (priv->event != event)
        epg_event_free(priv->event);
    priv->event = epg_event_dup(event);

    epg_event_widget_update_event(widget);
}

EPGEvent *epg_event_widget_get_event(EpgEventWidget *widget)
{
    g_return_val_if_fail(IS_EPG_EVENT_WIDGET(widget), NULL);
    EpgEventWidgetPrivate *priv = epg_event_widget_get_instance_private(widget);

    return priv->event;
}

