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
typedef struct _UiSidebarChannelsPrivate UiSidebarChannelsPrivate;
typedef struct _UiSidebarChannelsClass UiSidebarChannelsClass;

struct _UiSidebarChannels {
    GtkBin parent_instance;

    /*< private >*/
    UiSidebarChannelsPrivate *priv;
};

struct _UiSidebarChannelsClass {
    GtkBinClass parent_class;
};

GType ui_sidebar_channels_get_type(void) G_GNUC_CONST;

GtkWidget *ui_sidebar_channels_new(void);

void ui_sidebar_channels_update_favourites(UiSidebarChannels *sidebar);

G_END_DECLS
