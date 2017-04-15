#pragma once

#include <gtk/gtk.h>

typedef struct {
    gint x;
    gint y;
    gint width;
    gint height;
    guint32 is_visible : 1;   /* is the window visible */
    guint32 show_window : 1;  /* user preference; window may be hidden due to fullscreen mode, etc. */
    guint32 fullscreen : 1;   /* just for main window */
    guint32 initialized : 1;
} GuiWindowStatus;

typedef struct {
    GuiWindowStatus main_window;
    GuiWindowStatus epg_dialog;
    GuiWindowStatus channels_dialog;
    GuiWindowStatus control_dialog;
    GuiWindowStatus channel_properties_dialog;
} GuiStatus;

typedef struct {
    guint channel_id;
    guint fav_list_id;
    gchar *signal_source;
    gdouble volume;
    guint32 mute : 1;
    guint32 running : 1;
    guint32 initialized : 1;
    guint32 show_clock : 1;
} RecorderStatus;

typedef struct {
    GuiStatus gui;
    RecorderStatus recorder;
} AppStatus;

void status_save(AppStatus *status, gchar *filename);
void status_restore(AppStatus *status, gchar *filename);
