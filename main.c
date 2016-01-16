#include <stdio.h>
#include <string.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "video-output.h"
#include <dvbrecorder.h>

#include "config.h"

struct {
    GtkWidget *main_window;
    GtkWidget *drawing_area;
    GtkWidget *toolbox;

    GtkAccelGroup *accelerator_group;

    guint32 fullscreen : 1;
    guint32 show_toolbox : 1;
} widgets;

struct {
    VideoOutput *video_output;
    DVBRecorder *recorder;
    guint32 is_recording : 1;
} appdata;

GtkWidget *main_create_context_menu(void);

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

void main_init_toolbox(void)
{
    widgets.toolbox = gtk_dialog_new();
    gtk_window_set_transient_for(GTK_WINDOW(widgets.toolbox), GTK_WINDOW(widgets.main_window));
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(widgets.toolbox));

    GtkWidget *menu_bar = main_create_main_menu();
    gtk_box_pack_start(GTK_BOX(content), menu_bar, FALSE, FALSE, 0);

    GtkWidget *entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(content), entry, TRUE, TRUE, 0);

    gtk_window_add_accel_group(GTK_WINDOW(widgets.toolbox), widgets.accelerator_group);
    
    gtk_widget_show_all(widgets.toolbox);
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
            break;
        case DVB_RECORDER_EVENT_STREAM_STATUS_CHANGED:
            fprintf(stderr, "stream status changed: %d\n", ((DVBRecorderEventStreamStatusChanged *)event)->status);
            switch (((DVBRecorderEventStreamStatusChanged *)event)->status) {
                case DVB_STREAM_STATUS_TUNED:
                    fprintf(stderr, "dvb-recorder: tuned in\n");
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

    /* setup librecorder */

    main_init_window();
    main_init_toolbox();
    widgets.show_toolbox = 1;
    gtk_window_present(GTK_WINDOW(widgets.main_window));
    
    appdata.recorder = dvb_recorder_new(main_recorder_event_callback, NULL);
/*    if (!appdata.recorder)*/
        
    appdata.video_output = video_output_new(widgets.drawing_area);

    int fd = dvb_recorder_enable_video_source(appdata.recorder, TRUE);
    fprintf(stderr, "recorder video source: %d\n", fd);

    dvb_recorder_set_channel(appdata.recorder, 0);
    video_output_set_infile(appdata.video_output, fd);

    gtk_main();

    fprintf(stderr, "destroying video\n");
    video_output_destroy(appdata.video_output);
    fprintf(stderr, "destroying recorder\n");
    dvb_recorder_destroy(appdata.recorder);

    config_free();

    fprintf(stderr, "done\n");
    return 0;
}
