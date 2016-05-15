#include <stdio.h>
#include <string.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "video-output.h"
#include <dvbrecorder/dvbrecorder.h>
#include <dvbrecorder/channel-db.h>
#include <dvbrecorder/epg.h>

#include "ui-sidebar-dvb.h"
#include "favourites-dialog.h"

#include "config.h"
#include "ui-epg-list.h"

struct {
    GtkWidget *main_window;
    GtkWidget *drawing_area;
    GtkWidget *toolbox;
    GtkWidget *channel_list;
    GtkWidget *epg_dialog;
    GtkWidget *epg_list;

    struct {
        GtkWidget *record;
        gulong record_toggled_signal;
        GtkWidget *snapshot;
        GtkWidget *volume;
        gulong volume_changed_signal;
        GtkWidget *mute;
    } buttons;

    GtkAccelGroup *accelerator_group;

    guint32 fullscreen : 1;
    guint32 show_toolbox : 1;
    guint32 show_epg : 1;
} widgets;

struct {
    VideoOutput *video_output;
    DVBRecorder *recorder;
    guint32 is_recording : 1;
} appdata;

GtkWidget *main_create_context_menu(void);
void main_recorder_channel_selected_cb(UiSidebarChannels *sidebar, guint channel_id, gpointer userdata);

gchar *main_get_capture_dir(void)
{
    gchar *dir = NULL;

    if (config_get("main", "capture-dir", CFG_TYPE_STRING, &dir) != 0 || dir == NULL || dir[0] == '\0') {
        return g_strdup(g_get_home_dir());
    }
    
    return dir;
}

gchar *main_get_snapshot_dir(void)
{
    gchar *dir = NULL;

    if (config_get("main", "snapshot-dir", CFG_TYPE_STRING, &dir) != 0 || dir == NULL || dir[0] == '\0') {
        return g_strdup(g_get_home_dir());
    }
    
    return dir;
}

void main_init_channel_db(void)
{
    gchar *db = g_build_filename(
            g_get_user_config_dir(),
            "dvb-recorder",
            "channels.db",
            NULL);

    if (channel_db_init(db) != 0)
        fprintf(stderr, "Failed to init channel db\n");
    g_free(db);
}

static void main_quit(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

void main_toggle_fullscreen(void)
{
    if (!widgets.fullscreen) {
        widgets.fullscreen = TRUE;
        gtk_widget_hide(widgets.toolbox);
        gtk_window_fullscreen(GTK_WINDOW(widgets.main_window));
    }
    else {
        widgets.fullscreen = FALSE;
        gtk_widget_show(widgets.toolbox);
        gtk_window_unfullscreen(GTK_WINDOW(widgets.main_window));
    }
}

void main_action_snapshot(void)
{
    gchar fbuf[256];
    time_t t;
    struct tm *tmp;
    t = time(NULL);
    tmp = localtime(&t);

    gchar *snapshot_dir = main_get_snapshot_dir();
    strftime(fbuf, 256, "snapshot-%Y%m%d-%H%M%S.png", tmp);
    gchar *filename = g_build_filename(
            snapshot_dir,
            fbuf,
            NULL);
    g_free(snapshot_dir);

    video_output_snapshot(appdata.video_output, filename);

    g_free(filename);
}

void main_action_record(void)
{
    /* will get set by event */
    if (appdata.is_recording) {
        fprintf(stderr, "Stop recording.\n");
        dvb_recorder_record_stop(appdata.recorder);
    }
    else {
        gchar fbuf[256];
        time_t t;
        struct tm *tmp;
        t = time(NULL);
        tmp = localtime(&t);

        gchar *capture_dir = main_get_capture_dir();
        strftime(fbuf, 256, "capture-%Y%m%d-%H%M%S.ts", tmp);
        gchar *filename = g_build_filename(
                capture_dir,
                fbuf,
                NULL);
        g_free(capture_dir);

        fprintf(stderr, "Start recording.\n");
        dvb_recorder_record_start(appdata.recorder, filename);

        g_free(filename);
    }
}

void main_action_mute(void)
{
    gboolean muted = video_output_toggle_mute(appdata.video_output);

    g_signal_handler_block(widgets.buttons.volume, widgets.buttons.volume_changed_signal);

    if (muted)
        gtk_scale_button_set_value(GTK_SCALE_BUTTON(widgets.buttons.volume), 0.0);
    else
        gtk_scale_button_set_value(GTK_SCALE_BUTTON(widgets.buttons.volume),
                video_output_get_volume(appdata.video_output));

    g_signal_handler_unblock(widgets.buttons.volume, widgets.buttons.volume_changed_signal);
}

void main_action_set_volume(gdouble volume)
{
    video_output_set_volume(appdata.video_output, volume);

    g_signal_handler_block(widgets.buttons.volume, widgets.buttons.volume_changed_signal);
    gtk_scale_button_set_value(GTK_SCALE_BUTTON(widgets.buttons.volume), volume);
    g_signal_handler_unblock(widgets.buttons.volume, widgets.buttons.volume_changed_signal);
}

static void main_ui_volume_value_changed(GtkScaleButton *button, gdouble value, gpointer data)
{
    video_output_set_volume(appdata.video_output, value);
}

static gboolean main_key_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    fprintf(stderr, "key event\n");
    switch (event->keyval) {
        case GDK_KEY_f:
            if (event->type == GDK_KEY_RELEASE)
                main_toggle_fullscreen();
            break;
        case GDK_KEY_r:
            if (event->type == GDK_KEY_RELEASE)
                main_action_record();
            break;
        case GDK_KEY_space:
            if (event->type == GDK_KEY_RELEASE)
                main_action_snapshot();
            break;
        case GDK_KEY_m:
            if (event->type == GDK_KEY_RELEASE)
                main_action_mute();
            break;
/*        case GDK_KEY_t:
            if (event->type == GDK_KEY_RELEASE)
                dvb_recorder_set_channel(appdata.recorder, 0);
            break;*/
        default:
            return FALSE;
    }

    return TRUE;
}

static gboolean main_button_event(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    if (event->type == GDK_BUTTON_PRESS) {
        if (event->button == GDK_BUTTON_SECONDARY) {
            GtkWidget *popup = main_create_context_menu();
            gtk_menu_popup(GTK_MENU(popup), NULL, NULL, NULL, NULL,
                    event->button, event->time);
            return TRUE;
        }
    }

    return FALSE;
}

static void main_drawing_area_size_allocate(GtkWidget *widget, GtkAllocation *alloc, gpointer data)
{
    fprintf(stderr, "drawing_area_size_allocate\n");
}

static void main_drawing_area_realize(GtkWidget *widget, gpointer data)
{
    fprintf(stderr, "drawing_area_realize\n");
}

void main_menu_show_toolbar(gpointer userdata)
{
    if (widgets.show_toolbox) {
        gtk_widget_hide(widgets.toolbox);
    }
    else {
        gtk_widget_show(widgets.toolbox);
    }

    widgets.show_toolbox = !widgets.show_toolbox;
}

void main_menu_quit(gpointer userdata)
{
    fprintf(stderr, "Menu > Quit\n");
    gtk_main_quit();
}

void main_menu_show_favourites_dialog(void)
{
    favourites_dialog_show(widgets.toolbox,
            (CHANNEL_FAVOURITES_DIALOG_UPDATE_NOTIFY)ui_sidebar_channels_update_favourites,
            widgets.channel_list);
}

void _main_add_accelerator(GtkWidget *item, const gchar *accel_signal, GtkAccelGroup *accel_group,
        guint accel_key, GdkModifierType accel_mods, GtkAccelFlags accel_flags,
        GCallback accel_cb, gpointer accel_data)
{
    gtk_widget_add_accelerator(item, accel_signal, accel_group, accel_key, accel_mods, accel_flags);
    gtk_accel_group_connect(accel_group, accel_key, accel_mods, 0,
            g_cclosure_new_swap(accel_cb, accel_data, NULL));
}

GtkWidget *main_create_context_menu(void)
{
    GtkWidget *popup = gtk_menu_new();

    GtkWidget *item;

    /* FIXME: check menu item */
    item = gtk_menu_item_new_with_label(_("Show toolbox"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_menu_show_toolbar), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    item = gtk_menu_item_new_with_label(_("Edit channel lists"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_menu_show_favourites_dialog), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    item = gtk_menu_item_new_with_label(_("Quit"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_menu_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    gtk_widget_show_all(popup);

    return popup;
}

GtkWidget *main_create_main_menu(void)
{
    GtkWidget *menu_bar = gtk_menu_bar_new();
    GtkWidget *menu = gtk_menu_new();

    GtkWidget *item;

    item = gtk_menu_item_new_with_label(_("Edit channel lists"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_menu_show_favourites_dialog), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label(_("Quit"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_menu_quit), NULL);
    _main_add_accelerator(item, "activate", widgets.accelerator_group, GDK_KEY_q,
            GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE,
            G_CALLBACK(main_menu_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label(_("File"));
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), item);

    return menu_bar;
}

void main_init_window(void)
{
    widgets.main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(widgets.main_window), "destroy",
            G_CALLBACK(main_quit), NULL);
    g_signal_connect(G_OBJECT(widgets.main_window), "key-release-event",
            G_CALLBACK(main_key_event), NULL);
    g_signal_connect(G_OBJECT(widgets.main_window), "key-press-event",
            G_CALLBACK(main_key_event), NULL);
    g_signal_connect(G_OBJECT(widgets.main_window), "button-press-event",
            G_CALLBACK(main_button_event), NULL);

    widgets.drawing_area = gtk_drawing_area_new();
    gtk_widget_add_events(widgets.drawing_area, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

    gtk_widget_set_size_request(widgets.drawing_area, 320, 240);
    g_signal_connect(G_OBJECT(widgets.drawing_area), "size-allocate",
            G_CALLBACK(main_drawing_area_size_allocate), NULL);
    g_signal_connect(G_OBJECT(widgets.drawing_area), "realize",
            G_CALLBACK(main_drawing_area_realize), NULL);

    gtk_container_add(GTK_CONTAINER(widgets.main_window), widgets.drawing_area);

    widgets.accelerator_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(widgets.main_window), widgets.accelerator_group);

    gtk_widget_show_all(widgets.main_window);
}

GtkWidget *main_init_toolbox_buttons(void)
{
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);

    widgets.buttons.record = gtk_toggle_button_new();
    gtk_button_set_image(GTK_BUTTON(widgets.buttons.record),
            gtk_image_new_from_icon_name("media-record", GTK_ICON_SIZE_LARGE_TOOLBAR));
    /* FIXME: signal handler */
    widgets.buttons.record_toggled_signal =
        g_signal_connect_swapped(G_OBJECT(widgets.buttons.record), "toggled",
            G_CALLBACK(main_action_record), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), widgets.buttons.record, FALSE, FALSE, 0);

    widgets.buttons.snapshot = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(widgets.buttons.snapshot),
            gtk_image_new_from_icon_name("document-save", GTK_ICON_SIZE_LARGE_TOOLBAR));
    g_signal_connect_swapped(G_OBJECT(widgets.buttons.snapshot), "clicked",
            G_CALLBACK(main_action_snapshot), NULL);
    gtk_box_pack_end(GTK_BOX(hbox), widgets.buttons.snapshot, FALSE, FALSE, 0);

    widgets.buttons.volume = gtk_volume_button_new();
    widgets.buttons.volume_changed_signal =
        g_signal_connect(G_OBJECT(widgets.buttons.volume), "value-changed",
                G_CALLBACK(main_ui_volume_value_changed), NULL);
    gtk_box_pack_end(GTK_BOX(hbox), widgets.buttons.volume, FALSE, FALSE, 0);

    return hbox;
}

void main_init_toolbox(void)
{
    widgets.toolbox = gtk_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(widgets.toolbox), GTK_WINDOW(widgets.main_window));
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(widgets.toolbox));

    GtkWidget *menu_bar = main_create_main_menu();
    gtk_box_pack_start(GTK_BOX(content), menu_bar, FALSE, FALSE, 0);

    GtkWidget *button_bar = main_init_toolbox_buttons();
    gtk_box_pack_start(GTK_BOX(content), button_bar, FALSE, FALSE, 0);

/*    GtkWidget *entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(content), entry, TRUE, TRUE, 0);*/
    widgets.channel_list = ui_sidebar_channels_new();
    g_signal_connect(G_OBJECT(widgets.channel_list), "channel-selected",
            G_CALLBACK(main_recorder_channel_selected_cb), NULL);
    gtk_widget_set_size_request(widgets.channel_list, 200, 400);
    gtk_box_pack_start(GTK_BOX(content), widgets.channel_list, TRUE, TRUE, 0);

    gtk_window_add_accel_group(GTK_WINDOW(widgets.toolbox), widgets.accelerator_group);
    
    gtk_widget_show_all(widgets.toolbox);
}

void main_init_epg_dialog(void)
{
    widgets.epg_dialog = gtk_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(widgets.epg_dialog), GTK_WINDOW(widgets.main_window));
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(widgets.epg_dialog));

    widgets.epg_list = ui_epg_list_new();
    gtk_widget_set_size_request(widgets.epg_list, 400, 200);
    gtk_box_pack_start(GTK_BOX(content), widgets.epg_list, TRUE, TRUE, 0);

    gtk_widget_show_all(widgets.epg_dialog);
}

void main_recorder_channel_selected_cb(UiSidebarChannels *sidebar, guint channel_id, gpointer userdata)
{
    fprintf(stderr, "channel-selected: (%p, %u, %p)\n", sidebar, channel_id, userdata);
    dvb_recorder_set_channel(appdata.recorder, channel_id);

    gtk_window_present(GTK_WINDOW(widgets.main_window));
}

void main_ui_update_button_status(void)
{
    DVBRecorderRecordStatus status;
    dvb_recorder_query_record_status(appdata.recorder, &status);
    g_signal_handler_block(widgets.buttons.record, widgets.buttons.record_toggled_signal);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.buttons.record),
            (gboolean)(status.status == DVB_RECORD_STATUS_RECORDING));
    g_signal_handler_unblock(widgets.buttons.record, widgets.buttons.record_toggled_signal);
}

void _dump_event(EPGEvent *event)
{
    size_t sz = sizeof(EPGEvent);
    gchar *ptr = (gchar *)event;
    gchar *end = ptr + sz;
    fprintf(stderr, "[main] dump event %p [%zd]\n", event, sz);
    while (ptr < end) {
        fprintf(stderr, "%02x ", *ptr & 0xff);
        ++ptr;
    }
    fprintf(stderr, "\n");
}

void main_recorder_event_callback(DVBRecorderEvent *event, gpointer userdata)
{
    switch (event->type) {
        case DVB_RECORDER_EVENT_RECORD_STATUS_CHANGED:
            fprintf(stderr, "record status changed: %d\n", ((DVBRecorderEventRecordStatusChanged *)event)->status);
            switch (((DVBRecorderEventRecordStatusChanged *)event)->status) {
                case DVB_RECORD_STATUS_RECORDING:
                    appdata.is_recording = 1;
                    break;
                default:
                    if (appdata.is_recording) {
                        appdata.is_recording = 0;
                        DVBRecorderRecordStatus status;
                        dvb_recorder_query_record_status(appdata.recorder, &status);
                        fprintf(stderr, "%zd bytes (%.2f MiB), time: %f.0 seconds\n",
                                status.filesize, ((double)status.filesize)/(1024*1024), status.elapsed_time);
                    }
                    break;
            }
            main_ui_update_button_status();
            break;
        case DVB_RECORDER_EVENT_STREAM_STATUS_CHANGED:
            fprintf(stderr, "stream status changed: %d\n", ((DVBRecorderEventStreamStatusChanged *)event)->status);
            switch (((DVBRecorderEventStreamStatusChanged *)event)->status) {
                case DVB_STREAM_STATUS_TUNED:
                    fprintf(stderr, "dvb-recorder: tuned in\n");
                    video_output_set_infile(appdata.video_output,
                            dvb_recorder_enable_video_source(appdata.recorder, TRUE));
                    break;
                case DVB_STREAM_STATUS_TUNE_FAILED:
                    fprintf(stderr, "dvb-recorder: tune failed\n");
                    video_output_set_infile(appdata.video_output, -1);
                    break;
                case DVB_STREAM_STATUS_STOPPED:
                    fprintf(stderr, "dvb-recorder: stopped\n");
                    video_output_set_infile(appdata.video_output, -1);
                    break;
                case DVB_STREAM_STATUS_RUNNING:
                    fprintf(stderr, "dvb-recorder: running\n");
                    break;
                default:
                    break;
            }
            break;
        case DVB_RECORDER_EVENT_EIT_CHANGED:
            {
                fprintf(stderr, "EIT changed\n");

                GList *events = dvb_recorder_get_epg(appdata.recorder);
                ui_epg_list_update_events(UI_EPG_LIST(widgets.epg_list), events); 

                fprintf(stderr, "Updated events\n");

                g_list_free(events);
            }
            break;
        default:
            break;
    }
}

int main(int argc, char **argv)
{
    if (!XInitThreads()) {
        fprintf(stderr, "XInitThreads() failed.\n");
        return 1;
    }

    gtk_init(&argc, &argv);

    gchar *config = g_build_filename(
            g_get_user_config_dir(),
            "dvb-recorder",
            "config",
            NULL);

    if (config_load(config) != 0)
        fprintf(stderr, "Failed to load config.\n");
    g_free(config);

    main_init_channel_db();

    /* setup librecorder */
    appdata.recorder = dvb_recorder_new(main_recorder_event_callback, NULL);

    main_init_window();
    main_init_toolbox();
    main_init_epg_dialog();
    ui_epg_list_set_recorder_handle(UI_EPG_LIST(widgets.epg_list), appdata.recorder);
    widgets.show_toolbox = 1;
    widgets.show_epg = 1;
    gtk_window_present(GTK_WINDOW(widgets.main_window));
    
/*    if (!appdata.recorder)*/
        
    appdata.video_output = video_output_new(widgets.drawing_area);

    int fd = dvb_recorder_enable_video_source(appdata.recorder, TRUE);
    fprintf(stderr, "recorder video source: %d\n", fd);
    video_output_set_infile(appdata.video_output, fd);

    main_action_set_volume(1.0);

/*    dvb_recorder_set_channel(appdata.recorder, 0);*/

    gtk_main();

    fprintf(stderr, "destroying video\n");
    video_output_destroy(appdata.video_output);
    fprintf(stderr, "destroying recorder\n");
    dvb_recorder_destroy(appdata.recorder);

    config_free();

    fprintf(stderr, "done\n");
    return 0;
}
