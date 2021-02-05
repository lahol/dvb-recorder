#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>
#include <dvbrecorder/epg.h>

G_BEGIN_DECLS

#define UI_EPG_EVENT_DETAIL_TYPE (ui_epg_event_detail_get_type())
#define UI_EPG_EVENT_DETAIL(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), UI_EPG_EVENT_DETAIL_TYPE, UiEpgEventDetail))
#define IS_UI_EPG_EVENT_DETAIL(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), UI_EPG_EVENT_DETAIL_TYPE))
#define UI_EPG_EVENT_DETAIL_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), UI_EPG_EVENT_DETAIL_TYPE, UiEpgEventDetailClass))
#define IS_UI_EPG_EVENT_DETAIL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), UI_EPG_EVENT_DETAIL_TYPE))
#define UI_EPG_EVENT_DETAIL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), UI_EPG_EVENT_DETAIL_TYPE, UiEpgEventDetailClass))

typedef struct _UiEpgEventDetail UiEpgEventDetail;
typedef struct _UiEpgEventDetailClass UiEpgEventDetailClass;

struct _UiEpgEventDetail {
    GtkBin parent_instance;
};

struct _UiEpgEventDetailClass {
    GtkBinClass parent_class;
};

GType ui_epg_event_detail_get_type(void) G_GNUC_CONST;

GtkWidget *ui_epg_event_detail_new(void);

void ui_epg_event_detail_set_event(UiEpgEventDetail *widget, EPGEvent *event);

G_END_DECLS
