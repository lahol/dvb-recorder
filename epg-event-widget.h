#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>
#include <dvbrecorder/epg.h>

G_BEGIN_DECLS

#define EPG_EVENT_WIDGET_TYPE (epg_event_widget_get_type())
#define EPG_EVENT_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), EPG_EVENT_WIDGET_TYPE, EpgEventWidget))
#define IS_EPG_EVENT_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), EPG_EVENT_WIDGET_TYPE))
#define EPG_EVENT_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), EPG_EVENT_WIDGET_TYPE, EpgEventWidgetClass))
#define IS_EPG_EVENT_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), EPG_EVENT_WIDGET_TYPE))
#define EPG_EVENT_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), EPG_EVENT_WIDGET_TYPE, EpgEventWidgetClass))

typedef struct _EpgEventWidget EpgEventWidget;
typedef struct _EpgEventWidgetPrivate EpgEventWidgetPrivate;
typedef struct _EpgEventWidgetClass EpgEventWidgetClass;

struct _EpgEventWidget {
    GtkBin parent_instance;

    /*< private >*/
    EpgEventWidgetPrivate *priv;
};

struct _EpgEventWidgetClass {
    GtkBinClass parent_class;
};

GType epg_event_widget_get_type(void) G_GNUC_CONST;

GtkWidget *epg_event_widget_new(EPGEvent *event);

void epg_event_widget_set_event(EpgEventWidget *widget, EPGEvent *event);
EPGEvent *epg_event_widget_get_event(EpgEventWidget *widget);

G_END_DECLS
