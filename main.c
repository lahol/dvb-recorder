#include <stdio.h>
#include <string.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include "video-output.h"
#include <dvbrecorder.h>

struct {
    GtkWidget *main_window;
    GtkWidget *drawing_area;
    GtkWidget *toolbox;

    gboolean fullscreen;
} widgets;

struct {
    VideoOutput *video_output;
    DVBRecorder *recorder;
} appdata;

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

static gboolean main_key_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    fprintf(stderr, "key event\n");
    switch (event->keyval) {
        case GDK_KEY_f:
            if (event->type == GDK_KEY_RELEASE)
                main_toggle_fullscreen();
            break;
        case GDK_KEY_q:
            gtk_main_quit();
            break;
        case GDK_KEY_r:
            break;
        case GDK_KEY_space:
            break;
        default:
            return TRUE;
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

void main_init_window(void)
{
    widgets.main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(widgets.main_window), "destroy",
            G_CALLBACK(main_quit), NULL);
    g_signal_connect(G_OBJECT(widgets.main_window), "key-release-event",
            G_CALLBACK(main_key_event), NULL);
    g_signal_connect(G_OBJECT(widgets.main_window), "key-press-event",
            G_CALLBACK(main_key_event), NULL);

    widgets.drawing_area = gtk_drawing_area_new();
    gtk_widget_add_events(widgets.drawing_area, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);

    gtk_widget_set_size_request(widgets.drawing_area, 320, 240);
    g_signal_connect(G_OBJECT(widgets.drawing_area), "size-allocate",
            G_CALLBACK(main_drawing_area_size_allocate), NULL);
    g_signal_connect(G_OBJECT(widgets.drawing_area), "realize",
            G_CALLBACK(main_drawing_area_realize), NULL);

    gtk_container_add(GTK_CONTAINER(widgets.main_window), widgets.drawing_area);

    gtk_widget_show_all(widgets.main_window);
}

void main_init_toolbox(void)
{
    widgets.toolbox = gtk_dialog_new();
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(widgets.toolbox));

    GtkWidget *entry = gtk_entry_new();
    gtk_container_add(GTK_CONTAINER(content), entry);
    gtk_widget_show_all(widgets.toolbox);
}

int main(int argc, char **argv)
{
    if (!XInitThreads()) {
        fprintf(stderr, "XInitThreads() failed.\n");
        return 1;
    }

    gtk_init(&argc, &argv);

    /* setup librecorder */

    main_init_toolbox();
    main_init_window();
    
    appdata.recorder = dvb_recorder_new(NULL, NULL);
    appdata.video_output = video_output_new(widgets.drawing_area);

    int fd = dvb_recorder_enable_video_source(appdata.recorder, TRUE);
    fprintf(stderr, "recorder video source: %d\n", fd);

    dvb_recorder_set_channel(appdata.recorder, 0);
    video_output_set_infile(appdata.video_output, fd);

    sleep(1);
    dvb_recorder_start(appdata.recorder);

    gtk_main();

    fprintf(stderr, "destroying video\n");
    video_output_destroy(appdata.video_output);
    fprintf(stderr, "destroying recorder\n");
    dvb_recorder_destroy(appdata.recorder);

    fprintf(stderr, "done\n");
    return 0;
}
