#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>
#include <dvbrecorder/dvbrecorder.h>

G_BEGIN_DECLS

#define UI_EPG_LIST_TYPE (ui_epg_list_get_type())
#define UI_EPG_LIST(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), UI_EPG_LIST_TYPE, UiEpgList))
#define IS_UI_EPG_LIST(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), UI_EPG_LIST_TYPE))
#define UI_EPG_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), UI_EPG_LIST_TYPE, UiEpgListClass))
#define IS_UI_EPG_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), UI_EPG_LIST_TYPE))
#define UI_EPG_LIST_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), UI_EPG_LIST_TYPE, UiEpgListClass))

typedef struct _UiEpgList UiEpgList;
typedef struct _UiEpgListClass UiEpgListClass;

struct _UiEpgList {
    GtkBin parent_instance;
};

struct _UiEpgListClass {
    GtkBinClass parent_class;
};

GType ui_epg_list_get_type(void) G_GNUC_CONST;

GtkWidget *ui_epg_list_new(void);

void ui_epg_list_update_events(UiEpgList *list, GList *events);
void ui_epg_list_reset_events(UiEpgList *list);
void ui_epg_list_set_recorder_handle(UiEpgList *list, DVBRecorder *handle);

G_END_DECLS
