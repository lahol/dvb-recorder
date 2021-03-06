#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define UI_SIDEBAR_CHANNELS_TYPE (ui_sidebar_channels_get_type())
#define UI_SIDEBAR_CHANNELS(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), UI_SIDEBAR_CHANNELS_TYPE, UiSidebarChannels))
#define IS_UI_SIDEBAR_CHANNELS(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), UI_SIDEBAR_CHANNELS_TYPE))
#define UI_SIDEBAR_CHANNELS_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), UI_SIDEBAR_CHANNELS_TYPE, UiSidebarChannelsClass))
#define IS_UI_SIDEBAR_CHANNELS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), UI_SIDEBAR_CHANNELS_TYPE))
#define UI_SIDEBAR_CHANNELS_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), UI_SIDEBAR_CHANNELS_TYPE, UiSidebarChannelsClass))

typedef struct _UiSidebarChannels UiSidebarChannels;
typedef struct _UiSidebarChannelsClass UiSidebarChannelsClass;

struct _UiSidebarChannels {
    GtkBin parent_instance;
};

struct _UiSidebarChannelsClass {
    GtkBinClass parent_class;
};

GType ui_sidebar_channels_get_type(void) G_GNUC_CONST;

GtkWidget *ui_sidebar_channels_new(void);

void ui_sidebar_channels_update_favourites(UiSidebarChannels *sidebar);

guint32 ui_sidebar_channels_get_current_list(UiSidebarChannels *sidebar);
void ui_sidebar_channels_set_current_list(UiSidebarChannels *sidebar, guint32 id);
guint32 ui_sidebar_channels_get_current_channel(UiSidebarChannels *sidebar);
void ui_sidebar_channels_set_current_channel(UiSidebarChannels *sidebar, guint32 id, gboolean activate);
const gchar *ui_sidebar_channels_get_current_signal_source(UiSidebarChannels *sidebar);
void ui_sidebar_channels_set_current_signal_source(UiSidebarChannels *sidebar, const gchar *signal_source);

G_END_DECLS
