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
#include "ui-recorder-settings-dialog.h"
#include "ui-dialogs-run.h"

#include "config.h"
#include "status.h"
#include "ui-epg-list.h"
#include "osd.h"

#include "commands.h"
#include "logging.h"
#include "utils.h"

struct {
    GtkWidget *main_window;
    GtkWidget *drawing_area;
    GtkWidget *channels_dialog;
    GtkWidget *channel_list;
    GtkWidget *epg_dialog;
    GtkWidget *epg_list;
    GtkWidget *control_dialog;
    GtkWidget *channel_properties_dialog;

    struct {
        GtkWidget *record;
        gulong record_toggled_signal;
        GtkWidget *snapshot;
        GtkWidget *volume;
        gulong volume_changed_signal;
        GtkWidget *mute;
        GtkWidget *refresh;
        GtkWidget *show_clock;
        gulong clock_toggled_signal;
    } buttons;

    GtkWidget *status_label;

    GtkAccelGroup *accelerator_group;
} widgets;

struct {
    VideoOutput *video_output;
    DVBRecorder *recorder;
    OSD *osd;
    guint32 is_recording : 1;
    guint32 is_video_ready : 1;
    guint32 is_sdt_ready : 1;
    guint32 notified_channel_change : 1;
    guint32 ignore_events : 1;
    guint32 epg_valid : 1;

    CmdMode command_mode;

    guint hide_cursor_source;
    GTimer *hide_cursor_timer;
    guint record_status_update_source;
    GdkCursor *blank_cursor;

    gchar *rec_status_format_normal;
    gchar *rec_status_format_recording;

    guint32 osd_channel_id;
    guint32 osd_snapshot_id;
} appdata;

AppStatus appstatus;

GtkWidget *main_create_context_menu(void);
void main_recorder_channel_selected_cb(UiSidebarChannels *sidebar, guint channel_id, gpointer userdata);
void main_recorder_favourites_list_changed_cb(UiSidebarChannels *sidebar, guint fav_list_id, gpointer userdata);
void main_recorder_sidebar_signal_source_changed_cb(UiSidebarChannels *sidebar, gchar *signal_source, gpointer userdata);
void main_recorder_sidebar_show_channel_details_cb(UiSidebarChannels *sidebar, GQuark action, guint32 channel_id, gpointer userdata);
gboolean main_dialog_delete_event(GtkWidget *widget, GdkEvent *event, gpointer userdata);
void main_dialog_response_event(GtkWidget *widget, gint response_id, GuiWindowStatus *winstatus);
gboolean main_update_record_status(gpointer userdata);

void main_window_status_set_visible(GtkWidget *window, GuiWindowStatus *status, gboolean is_visible)
{
    /* hide/show window, do not touch show_window */
    /* FIXME: only set visibility if the user does not want to hide this window */
    if (is_visible) {
        gtk_window_present(GTK_WINDOW(window));
        if (status->initialized) {
            gtk_window_set_default_size(GTK_WINDOW(window), status->width, status->height);
            gtk_window_move(GTK_WINDOW(window), status->x, status->y);
        }
        status->is_visible = 1;
    }
    else {
        gtk_widget_hide(window);
        status->is_visible = 0;
    }
}

void main_window_status_set_show(GtkWidget *window, GuiWindowStatus *status, gboolean do_show)
{
    /* set show/hide, keep track of show_window */
    if (do_show) {
        status->show_window = 1;
    }
    else {
        status->show_window = 0;
    }

    main_window_status_set_visible(window, status, do_show);
}

void main_window_status_toggle_show(GtkWidget *window, GuiWindowStatus *status)
{
    main_window_status_set_show(window, status, !status->show_window || !status->is_visible);
}

gboolean main_check_mouse_motion(gpointer data)
{
    if (!appdata.blank_cursor)
#if GTK_CHECK_VERSION(3, 16, 0)
        appdata.blank_cursor = gdk_cursor_new_from_name(gdk_display_get_default(), "none");
#else
        appdata.blank_cursor = gdk_cursor_new(GDK_BLANK_CURSOR);
#endif
    if (!appstatus.gui.main_window.fullscreen) {
        gdk_window_set_cursor(gtk_widget_get_window(widgets.main_window), NULL);
        if (appdata.hide_cursor_timer) {
            g_timer_stop(appdata.hide_cursor_timer);
            g_timer_destroy(appdata.hide_cursor_timer);
            appdata.hide_cursor_timer = NULL;
        }
        appdata.hide_cursor_source = 0;
        return FALSE;
    }
    gdouble elapsed = g_timer_elapsed(appdata.hide_cursor_timer, NULL);
    if (elapsed > 3) {
        g_timer_stop(appdata.hide_cursor_timer);
        g_timer_destroy(appdata.hide_cursor_timer);
        appdata.hide_cursor_timer = NULL;
        appdata.hide_cursor_source = 0;
        gdk_window_set_cursor(gtk_widget_get_window(widgets.main_window), appdata.blank_cursor);
        return FALSE;
    }
    return TRUE;
}

gboolean main_handle_motion_event(GtkWidget *widget, GdkEventMotion *event, gpointer data)
{
    if (!appstatus.gui.main_window.fullscreen)
        return FALSE;
    gdk_window_set_cursor(gtk_widget_get_window(widget), NULL);
    if (!appdata.hide_cursor_timer)
        appdata.hide_cursor_timer = g_timer_new();
    g_timer_start(appdata.hide_cursor_timer);
    if (!appdata.hide_cursor_source)
        appdata.hide_cursor_source = g_idle_add(main_check_mouse_motion, NULL);
    return FALSE;
}

void main_window_status_set_fullscreen(gboolean fullscreen)
{
    if (fullscreen) {
        main_window_status_set_visible(widgets.channels_dialog, &appstatus.gui.channels_dialog, FALSE);
        main_window_status_set_visible(widgets.epg_dialog, &appstatus.gui.epg_dialog, FALSE);
        main_window_status_set_visible(widgets.control_dialog, &appstatus.gui.control_dialog, FALSE);
        main_window_status_set_visible(widgets.channel_properties_dialog, &appstatus.gui.channel_properties_dialog, FALSE);
        gtk_window_fullscreen(GTK_WINDOW(widgets.main_window));
        appstatus.gui.main_window.fullscreen = 1;

        appdata.hide_cursor_timer = g_timer_new();
        g_timer_start(appdata.hide_cursor_timer);
        appdata.hide_cursor_source = g_idle_add(main_check_mouse_motion, NULL);

        appdata.command_mode = CMD_MODE_FULLSCREEN;
    }
    else {
        main_window_status_set_visible(widgets.channels_dialog, &appstatus.gui.channels_dialog, TRUE);
        main_window_status_set_visible(widgets.epg_dialog, &appstatus.gui.epg_dialog, TRUE);
        main_window_status_set_visible(widgets.control_dialog, &appstatus.gui.control_dialog, TRUE);
        main_window_status_set_visible(widgets.channel_properties_dialog, &appstatus.gui.channel_properties_dialog, TRUE);
        gtk_window_unfullscreen(GTK_WINDOW(widgets.main_window));
        appstatus.gui.main_window.fullscreen = 0;

        gdk_window_set_cursor(gtk_widget_get_window(widgets.main_window), NULL);

        appdata.command_mode = CMD_MODE_NORMAL;
    }

    /* focus main window */
    gtk_window_present(GTK_WINDOW(widgets.main_window));
}

gboolean main_window_status_configure_event(GtkWidget *window, GdkEventConfigure *event, GuiWindowStatus *win_status)
{
    gtk_window_get_position(GTK_WINDOW(window), &win_status->x, &win_status->y);
    win_status->width = event->width;
    win_status->height = event->height;

    if (!win_status->initialized) {
        win_status->show_window = 1;
        win_status->is_visible = 1;
        win_status->initialized = 1;
    }

    return FALSE;
}

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
        LOG("Failed to init channel db\n");
    g_free(db);
}

static void main_quit(GtkWidget *widget, gpointer data)
{
    gtk_main_quit();
}

void main_toggle_fullscreen(void)
{
    main_window_status_set_fullscreen(!appstatus.gui.main_window.fullscreen);
}

void main_action_snapshot(void)
{
    gchar *snapshot_dir = main_get_snapshot_dir();
    gchar *pattern = NULL;
    gchar *filename;
    if (config_get("main", "snapshot-filename", CFG_TYPE_STRING, &pattern) == 0) {
        filename = dvb_recorder_make_record_filename(appdata.recorder, snapshot_dir, pattern);
        g_free(pattern);
    }
    else {
        filename = dvb_recorder_make_record_filename(appdata.recorder, snapshot_dir, "snapshot-${service_name}-${program_name}-${date:%Y%m%d-%H%M%S}.png");
    }

    LOG("[snapshot] filename: %s\n", filename);

    video_output_snapshot(appdata.video_output, filename);

    gchar *basename = g_path_get_basename(filename);

    osd_begin_transaction(appdata.osd);
    osd_remove_text(appdata.osd, appdata.osd_snapshot_id);
    appdata.osd_snapshot_id= osd_add_text(appdata.osd, basename, OSD_ALIGN_VERT_BOTTOM | OSD_ALIGN_HORZ_CENTER, 2);
    osd_commit_transaction(appdata.osd);

    g_free(basename);
    g_free(filename);
    g_free(snapshot_dir);
}

void main_action_record(void)
{
    /* will get set by event */
    if (appdata.is_recording) {
        LOG("Stop recording.\n");
        dvb_recorder_record_stop(appdata.recorder);
    }
    else {
        LOG("Start recording.\n");
        dvb_recorder_record_start(appdata.recorder);
/*        appdata.record_status_update_source = g_timeout_add_seconds(1, main_update_record_status, NULL);*/
    }
}

void main_action_set_show_clock(gboolean show)
{
    appstatus.recorder.show_clock = show;

    /* Use another format string */
    g_free(appdata.rec_status_format_normal);
    appdata.rec_status_format_normal = NULL;

    g_signal_handler_block(widgets.buttons.show_clock, widgets.buttons.clock_toggled_signal);
    if (show) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.buttons.show_clock), TRUE);
        if (!appdata.record_status_update_source)
            appdata.record_status_update_source = g_timeout_add_seconds(1, main_update_record_status, NULL);
        main_update_record_status(NULL);
    }
    else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.buttons.show_clock), FALSE);
    }
    g_signal_handler_unblock(widgets.buttons.show_clock, widgets.buttons.clock_toggled_signal);
}

void main_action_toggle_clock(void)
{
    main_action_set_show_clock(!appstatus.recorder.show_clock);
}

void main_action_set_mute(gboolean mute)
{
    appstatus.recorder.mute = mute;
    video_output_set_mute(appdata.video_output, mute);

    g_signal_handler_block(widgets.buttons.volume, widgets.buttons.volume_changed_signal);

    if (mute)
        gtk_scale_button_set_value(GTK_SCALE_BUTTON(widgets.buttons.volume), 0.0);
    else
        gtk_scale_button_set_value(GTK_SCALE_BUTTON(widgets.buttons.volume),
                video_output_get_volume(appdata.video_output));

    g_signal_handler_unblock(widgets.buttons.volume, widgets.buttons.volume_changed_signal);
}

void main_action_toggle_mute(void)
{
    gboolean muted = video_output_get_mute(appdata.video_output);

    main_action_set_mute(!muted);
}

void main_action_set_volume(gdouble volume)
{
    video_output_set_volume(appdata.video_output, volume);
    appstatus.recorder.volume = volume;

    g_signal_handler_block(widgets.buttons.volume, widgets.buttons.volume_changed_signal);
    gtk_scale_button_set_value(GTK_SCALE_BUTTON(widgets.buttons.volume), volume);
    g_signal_handler_unblock(widgets.buttons.volume, widgets.buttons.volume_changed_signal);
}

void main_action_audio_next(void)
{
    LOG("main_action_audio_next\n");
    video_output_audio_channel_next(appdata.video_output);
}

void main_action_switch_channel_main_focus(void)
{
    if (!gtk_window_has_toplevel_focus(GTK_WINDOW(widgets.main_window)))
        gtk_window_present(GTK_WINDOW(widgets.main_window));
    else
        gtk_window_present(GTK_WINDOW(widgets.channels_dialog));
}

static void main_ui_volume_value_changed(GtkScaleButton *button, gdouble value, gpointer data)
{
    video_output_set_volume(appdata.video_output, value);
    appstatus.recorder.volume = value;
}

static gboolean main_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    LOG("key press event\n");
    GtkWidget *focus_widget = gtk_window_get_focus(GTK_WINDOW(widget));
    fprintf(stderr, "key press for event: type: %s\n", focus_widget ?
            G_OBJECT_TYPE_NAME(focus_widget) : "GtkNone");

    /* ignore simple key presses if we are in a GtkEntry */
    if (GTK_IS_ENTRY(focus_widget)) {
        if ((event->state & ~(GDK_SHIFT_MASK | GDK_RELEASE_MASK)) == 0)
            if (gtk_widget_event(focus_widget, (GdkEvent *)event))
                return TRUE;
    }

    /* Mask innocuous modifiers like NumLock, CapsLock, that are not actively pressed. */
    guint modifier_mask = gtk_accelerator_get_default_mod_mask();
    Command *cmd = cmd_find(appdata.command_mode, event->keyval, event->state & modifier_mask);
    if (!cmd)
        return FALSE;

    cmd_run(cmd);

    return TRUE;
}

static gboolean main_key_release_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    LOG("key release event\n");

    return FALSE;
}

static gboolean main_button_event(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
    if (event->type == GDK_BUTTON_PRESS) {
        if (event->button == GDK_BUTTON_SECONDARY) {
            GtkWidget *popup = main_create_context_menu();
#if GTK_CHECK_VERSION(3,22,0)
            gtk_menu_popup_at_pointer(GTK_MENU(popup), (GdkEvent *)event);
#else
            gtk_menu_popup(GTK_MENU(popup), NULL, NULL, NULL, NULL,
                    event->button, event->time);
#endif
            return TRUE;
        }
    }

    return FALSE;
}

static void main_drawing_area_size_allocate(GtkWidget *widget, GtkAllocation *alloc, gpointer data)
{
    LOG("drawing_area_size_allocate\n");
}

static void main_drawing_area_realize(GtkWidget *widget, gpointer data)
{
    LOG("drawing_area_realize\n");
}

gboolean main_dialog_delete_event(GtkWidget *widget, GdkEvent *event, gpointer userdata)
{
    return TRUE;
}

void main_dialog_response_event(GtkWidget *widget, gint response_id, GuiWindowStatus *winstatus)
{
    if (response_id == GTK_RESPONSE_CLOSE || response_id == GTK_RESPONSE_DELETE_EVENT ||
            response_id == GTK_RESPONSE_NONE) {
        main_window_status_set_show(widget, winstatus, FALSE);
    }
}

void main_menu_show_channels_dialog(gpointer userdata)
{
    main_window_status_toggle_show(widgets.channels_dialog, &appstatus.gui.channels_dialog);
}

void main_menu_show_epg_dialog(gpointer userdata)
{
    main_window_status_toggle_show(widgets.epg_dialog, &appstatus.gui.epg_dialog);
}

void main_menu_show_control_dialog(gpointer userdata)
{
    main_window_status_toggle_show(widgets.control_dialog, &appstatus.gui.control_dialog);
}

void main_menu_show_channel_properties_dialog(gpointer userdata)
{
    main_window_status_toggle_show(widgets.channel_properties_dialog, &appstatus.gui.channel_properties_dialog);
}

void main_menu_refresh_video_output(gpointer userdata)
{
    video_output_set_infile(appdata.video_output,
                            dvb_recorder_enable_video_source(appdata.recorder, TRUE));
}

void main_action_quit(gpointer userdata)
{
    DVBRecorderRecordStatus recstatus;

    dvb_recorder_query_record_status(appdata.recorder, &recstatus);
    if (recstatus.status == DVB_RECORD_STATUS_RECORDING) {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(widgets.main_window),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_WARNING,
                GTK_BUTTONS_YES_NO,
                "A recording is running and has to be stopped first. Stop the recording and proceed?");
        GtkResponseType response = gtk_dialog_run(GTK_DIALOG(dialog));
        if (GTK_IS_DIALOG(dialog))
            gtk_widget_destroy(dialog);

        if (response == GTK_RESPONSE_YES) {
            dvb_recorder_record_stop(appdata.recorder);
        }
        else {
            return;
        }
    }

    gtk_main_quit();
}

gboolean main_update_record_status(gpointer userdata)
{
    DVBRecorderRecordStatus recstatus;
    dvb_recorder_query_record_status(appdata.recorder, &recstatus);
    gchar tbuf[256];
    gchar *markup;

    if (recstatus.status == DVB_RECORD_STATUS_RECORDING || !appstatus.recorder.show_clock) {
        util_duration_to_string_iso(tbuf, 256, recstatus.elapsed_time);
    }
    else {
        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        strftime(tbuf, 256, "%H:%M:%S", tm);
    }

    if (recstatus.status == DVB_RECORD_STATUS_RECORDING) {
        if (G_UNLIKELY(appdata.rec_status_format_recording == NULL)) {
            gchar *font = NULL;
            gchar *color = NULL;
            gchar *font_format;
            gchar *color_format;
            if (config_get("main", "record-status-font", CFG_TYPE_STRING, &font) != 0) {
                font_format = g_strdup("size=\"48000\"");
            }
            else {
                font_format = g_strdup_printf("font=\"%s\"", font);
                g_free(font);
            }
            if (config_get("main", "record-status-color-recording", CFG_TYPE_STRING, &color) != 0) {
                color_format = g_strdup("color=\"red\"");
            }
            else {
                color_format = g_strdup_printf("color=\"%s\"", color);
                g_free(color);
            }

            appdata.rec_status_format_recording = g_strdup_printf("<span %s %s>%%s</span>",
                                                        font_format, color_format);
            g_free(font_format);
            g_free(color_format);
        }
        markup = g_markup_printf_escaped(appdata.rec_status_format_recording, tbuf);
    }
    else {
        if (G_UNLIKELY(appdata.rec_status_format_normal == NULL)) {
            gchar *font = NULL;
            gchar *color = NULL;
            gchar *font_format;
            gchar *color_format;

            if (appstatus.recorder.show_clock) {
                /* Use record-status-font/color as first fallback */
                if (config_get("main", "clock-font", CFG_TYPE_STRING, &font) != 0 /*&&
                    config_get("main", "record-status-font", CFG_TYPE_STRING, &font) != 0*/) {
                    font_format = g_strdup("size=\"48000\"");
                }
                else {
                    font_format = g_strdup_printf("font=\"%s\"", font);
                    g_free(font);
                }
                if (config_get("main", "clock-color", CFG_TYPE_STRING, &color) != 0 /*&&
                    config_get("main", "record-status-color-normal", CFG_TYPE_STRING, &color) != 0*/) {
                    color_format = g_strdup("color=\"black\"");
                }
                else {
                    color_format = g_strdup_printf("color=\"%s\"", color);
                    g_free(color);
                }
            }
            else {
                if (config_get("main", "record-status-font", CFG_TYPE_STRING, &font) != 0 /*&&
                    config_get("main", "clock-font", CFG_TYPE_STRING, &font) != 0*/) {
                    font_format = g_strdup("size=\"48000\"");
                }
                else {
                    font_format = g_strdup_printf("font=\"%s\"", font);
                    g_free(font);
                }
                if (config_get("main", "record-status-color-normal", CFG_TYPE_STRING, &color) != 0 /*&&
                    config_get("main", "clock-color", CFG_TYPE_STRING, &color) != 0*/) {
                    color_format = g_strdup("color=\"black\"");
                }
                else {
                    color_format = g_strdup_printf("color=\"%s\"", color);
                    g_free(color);
                }
            }
            appdata.rec_status_format_normal = g_strdup_printf("<span %s %s>%%s</span>",
                                                        font_format, color_format);
            g_free(font_format);
            g_free(color_format);
        }
        markup = g_markup_printf_escaped(appdata.rec_status_format_normal, tbuf);
    }

    if (markup) {
        gtk_label_set_markup(GTK_LABEL(widgets.status_label), markup);
        g_free(markup);
    }

    if (recstatus.status != DVB_RECORD_STATUS_RECORDING && !appstatus.recorder.show_clock)
        appdata.record_status_update_source = 0;
    return (gboolean)(recstatus.status == DVB_RECORD_STATUS_RECORDING || appstatus.recorder.show_clock);
}

void main_menu_show_favourites_dialog(void)
{
    favourites_dialog_show(widgets.main_window, appdata.recorder,
            ui_sidebar_channels_get_current_list(UI_SIDEBAR_CHANNELS(widgets.channel_list)),
            (CHANNEL_FAVOURITES_DIALOG_UPDATE_NOTIFY)ui_sidebar_channels_update_favourites,
            widgets.channel_list);
}

void main_menu_show_recorder_settings_dialog(void)
{
    ui_recorder_settings_dialog_show(widgets.main_window, appdata.recorder);
}

void main_menu_show_video_settings_dialog(void)
{
    video_settings_dialog_show(widgets.main_window, appdata.video_output);
}

void main_menu_show_add_scheduled_events_dialog(void)
{
    ui_add_scheduled_event_dialog_show(widgets.main_window, appdata.recorder, 0);
}

void main_menu_show_scheduled_events_dialog(void)
{
    ui_scheduled_events_dialog_show(widgets.main_window, appdata.recorder);
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
    item = gtk_menu_item_new_with_label(_("Show controls"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_menu_show_control_dialog), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    item = gtk_menu_item_new_with_label(_("Show channels"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_menu_show_channels_dialog), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    item = gtk_menu_item_new_with_label(_("Show EPG"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_menu_show_epg_dialog), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    item = gtk_menu_item_new_with_label(_("Show channel properties"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_menu_show_channel_properties_dialog), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    item = gtk_menu_item_new_with_label(_("Edit channel lists"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_menu_show_favourites_dialog), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    item = gtk_menu_item_new_with_label(_("Edit recorder settings"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_menu_show_recorder_settings_dialog), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    item = gtk_menu_item_new_with_label(_("Edit video settings"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_menu_show_video_settings_dialog), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    item = gtk_menu_item_new_with_label(_("Scheduled recordings"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_menu_show_scheduled_events_dialog), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    item = gtk_menu_item_new_with_label(_("Refresh video"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_menu_refresh_video_output), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(popup), item);

    item = gtk_menu_item_new_with_label(_("Quit"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_action_quit), NULL);
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

    item = gtk_menu_item_new_with_label(_("Edit recorder settings"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_menu_show_recorder_settings_dialog), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

    item = gtk_menu_item_new_with_label(_("Quit"));
    g_signal_connect_swapped(G_OBJECT(item), "activate",
            G_CALLBACK(main_action_quit), NULL);
    _main_add_accelerator(item, "activate", widgets.accelerator_group, GDK_KEY_q,
            GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE,
            G_CALLBACK(main_action_quit), NULL);
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
            G_CALLBACK(main_key_release_event), NULL);
    g_signal_connect(G_OBJECT(widgets.main_window), "key-press-event",
            G_CALLBACK(main_key_press_event), NULL);
    g_signal_connect(G_OBJECT(widgets.main_window), "button-press-event",
            G_CALLBACK(main_button_event), NULL);
    g_signal_connect(G_OBJECT(widgets.main_window), "configure-event",
            G_CALLBACK(main_window_status_configure_event), &appstatus.gui.main_window);
    g_signal_connect(G_OBJECT(widgets.main_window), "motion-notify-event",
            G_CALLBACK(main_handle_motion_event), NULL);

    widgets.drawing_area = gtk_drawing_area_new();
    gtk_widget_add_events(widgets.drawing_area, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK | GDK_POINTER_MOTION_MASK);

    g_signal_connect(G_OBJECT(widgets.drawing_area), "size-allocate",
            G_CALLBACK(main_drawing_area_size_allocate), NULL);
    g_signal_connect(G_OBJECT(widgets.drawing_area), "realize",
            G_CALLBACK(main_drawing_area_realize), NULL);

    gtk_container_add(GTK_CONTAINER(widgets.main_window), widgets.drawing_area);

    widgets.accelerator_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(widgets.main_window), widgets.accelerator_group);

    gtk_window_set_gravity(GTK_WINDOW(widgets.main_window), GDK_GRAVITY_STATIC);
    if (appstatus.gui.main_window.initialized) {
        gtk_window_move(GTK_WINDOW(widgets.main_window),
                appstatus.gui.main_window.x, appstatus.gui.main_window.y);
        gtk_window_set_default_size(GTK_WINDOW(widgets.main_window),
                appstatus.gui.main_window.width, appstatus.gui.main_window.height);
    }
    else {
        gtk_window_set_default_size(GTK_WINDOW(widgets.main_window), 320, 240);
    }

    gtk_widget_show_all(widgets.main_window);
}

GtkWidget *main_init_channels_dialog_buttons(void)
{
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 1);

    widgets.buttons.record = gtk_toggle_button_new();
    gtk_button_set_image(GTK_BUTTON(widgets.buttons.record),
            gtk_image_new_from_icon_name("media-record", GTK_ICON_SIZE_LARGE_TOOLBAR));
    gtk_widget_set_tooltip_text(widgets.buttons.record, _("Start recording"));
    /* FIXME: signal handler */
    widgets.buttons.record_toggled_signal =
        g_signal_connect_swapped(G_OBJECT(widgets.buttons.record), "toggled",
            G_CALLBACK(main_action_record), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), widgets.buttons.record, FALSE, FALSE, 0);

    widgets.buttons.show_clock = gtk_toggle_button_new_with_label(_("Show Clock"));
    widgets.buttons.clock_toggled_signal =
        g_signal_connect_swapped(G_OBJECT(widgets.buttons.show_clock), "toggled",
                G_CALLBACK(main_action_toggle_clock), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), widgets.buttons.show_clock, FALSE, FALSE, 0);

    widgets.buttons.snapshot = gtk_button_new();
    gtk_widget_set_tooltip_text(widgets.buttons.snapshot, _("Make snapshot"));
    gtk_button_set_image(GTK_BUTTON(widgets.buttons.snapshot),
            gtk_image_new_from_icon_name("document-save", GTK_ICON_SIZE_LARGE_TOOLBAR));
    g_signal_connect_swapped(G_OBJECT(widgets.buttons.snapshot), "clicked",
            G_CALLBACK(main_action_snapshot), NULL);
    gtk_box_pack_end(GTK_BOX(hbox), widgets.buttons.snapshot, FALSE, FALSE, 0);

    widgets.buttons.volume = gtk_volume_button_new();
    gtk_widget_set_tooltip_text(widgets.buttons.volume, _("Change Volume"));
    widgets.buttons.volume_changed_signal =
        g_signal_connect(G_OBJECT(widgets.buttons.volume), "value-changed",
                G_CALLBACK(main_ui_volume_value_changed), NULL);
    gtk_box_pack_end(GTK_BOX(hbox), widgets.buttons.volume, FALSE, FALSE, 0);

    widgets.buttons.refresh = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(widgets.buttons.refresh),
            gtk_image_new_from_icon_name("view-refresh", GTK_ICON_SIZE_LARGE_TOOLBAR));
    gtk_widget_set_tooltip_text(widgets.buttons.refresh, _("Refresh video"));
    g_signal_connect_swapped(G_OBJECT(widgets.buttons.refresh), "clicked",
            G_CALLBACK(main_menu_refresh_video_output), NULL);
    gtk_box_pack_end(GTK_BOX(hbox), widgets.buttons.refresh, FALSE, FALSE, 0);

    return hbox;
}

void main_position_dialog(GtkWidget *dialog, GuiWindowStatus *status, gint default_width, gint default_height)
{
    gtk_window_set_gravity(GTK_WINDOW(dialog), GDK_GRAVITY_STATIC);
    if (status->initialized) {
        gtk_window_move(GTK_WINDOW(dialog), status->x, status->y);
        gtk_window_set_default_size(GTK_WINDOW(dialog), status->width, status->height);
    }
    else {
        gtk_window_set_default_size(GTK_WINDOW(dialog), default_width, default_height);
    }
}

void main_init_channels_dialog(void)
{
    widgets.channels_dialog = gtk_dialog_new();
    g_signal_connect(G_OBJECT(widgets.channels_dialog), "configure-event",
            G_CALLBACK(main_window_status_configure_event), &appstatus.gui.channels_dialog);
    g_signal_connect(G_OBJECT(widgets.channels_dialog), "delete-event",
            G_CALLBACK(main_dialog_delete_event), NULL);
    g_signal_connect(G_OBJECT(widgets.channels_dialog), "key-press-event",
            G_CALLBACK(main_key_press_event), NULL);
    gtk_window_set_transient_for(GTK_WINDOW(widgets.channels_dialog), GTK_WINDOW(widgets.main_window));
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(widgets.channels_dialog));

    gtk_window_set_title(GTK_WINDOW(widgets.channels_dialog), _("Channels"));

/*    GtkWidget *entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(content), entry, TRUE, TRUE, 0);*/
    widgets.channel_list = ui_sidebar_channels_new();
    g_signal_connect(G_OBJECT(widgets.channel_list), "channel-selected",
            G_CALLBACK(main_recorder_channel_selected_cb), NULL);
    g_signal_connect(G_OBJECT(widgets.channel_list), "favourites-list-changed",
            G_CALLBACK(main_recorder_favourites_list_changed_cb), NULL);
    g_signal_connect(G_OBJECT(widgets.channel_list), "signal-source-changed",
            G_CALLBACK(main_recorder_sidebar_signal_source_changed_cb), NULL);
    g_signal_connect(G_OBJECT(widgets.channel_list), "channel-context-action::show-details",
            G_CALLBACK(main_recorder_sidebar_show_channel_details_cb), NULL);


    gtk_box_pack_start(GTK_BOX(content), widgets.channel_list, TRUE, TRUE, 0);

    gtk_window_add_accel_group(GTK_WINDOW(widgets.channels_dialog), widgets.accelerator_group);

    main_position_dialog(widgets.channels_dialog, &appstatus.gui.channels_dialog, 200, 400);

    gtk_widget_show_all(widgets.channels_dialog);
}

void main_init_epg_dialog(void)
{
    widgets.epg_dialog = gtk_dialog_new();
    g_signal_connect(G_OBJECT(widgets.epg_dialog), "configure-event",
            G_CALLBACK(main_window_status_configure_event), &appstatus.gui.epg_dialog);
    g_signal_connect(G_OBJECT(widgets.epg_dialog), "delete-event",
            G_CALLBACK(main_dialog_delete_event), NULL);
    g_signal_connect(G_OBJECT(widgets.epg_dialog), "key-press-event",
            G_CALLBACK(main_key_press_event), NULL);
    gtk_window_set_transient_for(GTK_WINDOW(widgets.epg_dialog), GTK_WINDOW(widgets.main_window));
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(widgets.epg_dialog));

    gtk_window_set_title(GTK_WINDOW(widgets.epg_dialog), _("Program information"));

    widgets.epg_list = ui_epg_list_new();

    gtk_box_pack_start(GTK_BOX(content), widgets.epg_list, TRUE, TRUE, 0);

    gtk_window_add_accel_group(GTK_WINDOW(widgets.epg_dialog), widgets.accelerator_group);

    main_position_dialog(widgets.epg_dialog, &appstatus.gui.epg_dialog, 400, 300);

    gtk_widget_show_all(widgets.epg_dialog);
}

void main_init_control_dialog(void)
{
    widgets.control_dialog = gtk_dialog_new();
    g_signal_connect(G_OBJECT(widgets.control_dialog), "configure-event",
            G_CALLBACK(main_window_status_configure_event), &appstatus.gui.control_dialog);
    g_signal_connect(G_OBJECT(widgets.control_dialog), "delete-event",
            G_CALLBACK(main_dialog_delete_event), NULL);
    g_signal_connect(G_OBJECT(widgets.control_dialog), "key-press-event",
            G_CALLBACK(main_key_press_event), NULL);
    gtk_window_set_transient_for(GTK_WINDOW(widgets.control_dialog), GTK_WINDOW(widgets.main_window));
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(widgets.control_dialog));

    gtk_window_set_title(GTK_WINDOW(widgets.control_dialog), _("Controls"));

    GtkWidget *menu_bar = main_create_main_menu();
    gtk_box_pack_start(GTK_BOX(content), menu_bar, FALSE, FALSE, 0);

    GtkWidget *button_bar = main_init_channels_dialog_buttons();
    gtk_box_pack_start(GTK_BOX(content), button_bar, FALSE, FALSE, 0);

    widgets.status_label = gtk_label_new("Ready");
    gtk_label_set_markup(GTK_LABEL(widgets.status_label), "<span size=\"48000\"></span>");
    gtk_box_pack_start(GTK_BOX(content), widgets.status_label, TRUE, TRUE, 0);

    gtk_window_add_accel_group(GTK_WINDOW(widgets.control_dialog), widgets.accelerator_group);

    main_position_dialog(widgets.control_dialog, &appstatus.gui.control_dialog, 130, 60);

    gtk_widget_show_all(widgets.control_dialog);
}

void main_init_channel_properties_dialog(void)
{
    widgets.channel_properties_dialog = ui_channel_properties_dialog_new(GTK_WINDOW(widgets.main_window));
    g_signal_connect(G_OBJECT(widgets.channel_properties_dialog), "configure-event",
            G_CALLBACK(main_window_status_configure_event), &appstatus.gui.channel_properties_dialog);
    g_signal_connect(G_OBJECT(widgets.channel_properties_dialog), "delete-event",
            G_CALLBACK(main_dialog_delete_event), NULL);
    g_signal_connect(G_OBJECT(widgets.channel_properties_dialog), "response",
            G_CALLBACK(main_dialog_response_event), &appstatus.gui.channel_properties_dialog);
    g_signal_connect(G_OBJECT(widgets.channel_properties_dialog), "key-press-event",
            G_CALLBACK(main_key_press_event), NULL);

    gtk_window_set_title(GTK_WINDOW(widgets.channel_properties_dialog), _("Channel Properties"));

    main_position_dialog(widgets.channel_properties_dialog, &appstatus.gui.channel_properties_dialog, 300, 200);

    gtk_widget_show_all(widgets.channel_properties_dialog);
}

void main_recorder_channel_selected_cb(UiSidebarChannels *sidebar, guint channel_id, gpointer userdata)
{
    LOG("channel-selected: (%p, %u, %p)\n", sidebar, channel_id, userdata);

    DVBRecorderRecordStatus recstatus;

    dvb_recorder_query_record_status(appdata.recorder, &recstatus);
    if (recstatus.status == DVB_RECORD_STATUS_RECORDING) {
        GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(widgets.channels_dialog),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_WARNING,
                GTK_BUTTONS_YES_NO,
                "A recording is running and has to be stopped first. Stop the recording and proceed?");
        GtkResponseType response = gtk_dialog_run(GTK_DIALOG(dialog));
        if (GTK_IS_DIALOG(dialog))
            gtk_widget_destroy(dialog);

        if (response == GTK_RESPONSE_YES) {
            dvb_recorder_record_stop(appdata.recorder);
        }
        else {
            ui_sidebar_channels_set_current_channel(UI_SIDEBAR_CHANNELS(widgets.channel_list), appstatus.recorder.channel_id, FALSE);
            return;
        }
    }

    dvb_recorder_set_channel(appdata.recorder, (guint64)channel_id);

    /*
    gtk_window_present(GTK_WINDOW(widgets.main_window));
    */
}

void main_recorder_favourites_list_changed_cb(UiSidebarChannels *sidebar, guint fav_list_id, gpointer userdata)
{
    appstatus.recorder.fav_list_id = fav_list_id;
}

void main_recorder_sidebar_signal_source_changed_cb(UiSidebarChannels *sidebar, gchar *signal_source, gpointer userdata)
{
    fprintf(stderr, "signal source changed to %s\n", signal_source);
    g_free(appstatus.recorder.signal_source);
    appstatus.recorder.signal_source = g_strdup(signal_source);
}

void main_recorder_sidebar_show_channel_details_cb(UiSidebarChannels *sidebar, GQuark action, guint32 channel_id, gpointer userdata)
{
    main_window_status_set_show(
            widgets.channel_properties_dialog,
            &appstatus.gui.channel_properties_dialog,
            TRUE);
    ui_channel_properties_dialog_set_channel_id(
            UI_CHANNEL_PROPERTIES_DIALOG(widgets.channel_properties_dialog),
            channel_id);
}

void main_ui_update_button_status(void)
{
    DVBRecorderRecordStatus status;
    dvb_recorder_query_record_status(appdata.recorder, &status);
    g_signal_handler_block(widgets.buttons.record, widgets.buttons.record_toggled_signal);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widgets.buttons.record),
            (gboolean)(status.status == DVB_RECORD_STATUS_RECORDING));
    if (status.status == DVB_RECORD_STATUS_RECORDING)
        gtk_widget_set_tooltip_text(widgets.buttons.record, _("Stop recording"));
    else
        gtk_widget_set_tooltip_text(widgets.buttons.record, _("Start recording"));
    g_signal_handler_unblock(widgets.buttons.record, widgets.buttons.record_toggled_signal);
}

void _dump_event(EPGEvent *event)
{
    size_t sz = sizeof(EPGEvent);
    gchar *ptr = (gchar *)event;
    gchar *end = ptr + sz;
    LOG("[main] dump event %p [%zd]\n", event, sz);
    while (ptr < end) {
        LOG("%02x ", *ptr & 0xff);
        ++ptr;
    }
    LOG("\n");
}

void main_osd_overlay_removed(guint32 id, gpointer userdata)
{
    if (id == appdata.osd_channel_id)
        appdata.osd_channel_id = 0;
    else if (id == appdata.osd_snapshot_id)
        appdata.osd_snapshot_id = 0;
}

void main_notify_channel_change(void)
{
    if (!appdata.notified_channel_change && appdata.is_sdt_ready && appdata.is_video_ready) {
        DVBStreamInfo *info = dvb_recorder_get_stream_info(appdata.recorder);
        osd_begin_transaction(appdata.osd);
        osd_remove_text(appdata.osd, appdata.osd_channel_id);
        appdata.osd_channel_id = osd_add_text(appdata.osd, info->service_name, OSD_ALIGN_VERT_TOP | OSD_ALIGN_HORZ_LEFT, 3);
        osd_commit_transaction(appdata.osd);
        dvb_stream_info_free(info);
        appdata.notified_channel_change = 1;
    }
}

void main_video_output_event_callback(VideoOutput *video_output, VideoOutputEventType event, gpointer userdata)
{
    switch (event) {
        case VIDEO_OUTPUT_EVENT_PLAYING:
            fprintf(stderr, "event playing\n");
            appdata.is_video_ready = 1;
            main_notify_channel_change();
            break;
        case VIDEO_OUTPUT_EVENT_READY:
            fprintf(stderr, "Video output ready\n");
            dvb_recorder_video_source_run(appdata.recorder);
            break;
        default:
            break;
    }
}

void main_recorder_event_callback(DVBRecorderEvent *event, gpointer userdata)
{
    if (appdata.ignore_events)
        return;
    switch (event->type) {
        case DVB_RECORDER_EVENT_RECORD_STATUS_CHANGED:
            LOG("record status changed: %d\n", ((DVBRecorderEventRecordStatusChanged *)event)->status);
            switch (((DVBRecorderEventRecordStatusChanged *)event)->status) {
                case DVB_RECORD_STATUS_RECORDING:
                    appdata.is_recording = 1;
                    if (!appdata.record_status_update_source)
                        appdata.record_status_update_source = g_timeout_add_seconds(1, main_update_record_status, NULL);
                    break;
                default:
                    if (appdata.is_recording) {
                        appdata.is_recording = 0;
                        DVBRecorderRecordStatus status;
                        dvb_recorder_query_record_status(appdata.recorder, &status);
                        LOG("%zd bytes (%.2f MiB), time: %f.0 seconds\n",
                                status.filesize, ((double)status.filesize)/(1024*1024), status.elapsed_time);
                    }
                    break;
            }
            main_ui_update_button_status();
            break;
        case DVB_RECORDER_EVENT_STREAM_STATUS_CHANGED:
            LOG("stream status changed: %d\n", ((DVBRecorderEventStreamStatusChanged *)event)->status);
            switch (((DVBRecorderEventStreamStatusChanged *)event)->status) {
                case DVB_STREAM_STATUS_TUNED:
                    LOG("dvb-recorder: tuned in\n");
                    video_output_set_infile(appdata.video_output,
                                            dvb_recorder_enable_video_source(appdata.recorder, TRUE));
                    ui_epg_list_reset_events(UI_EPG_LIST(widgets.epg_list));
                    break;
                case DVB_STREAM_STATUS_TUNE_FAILED:
                    LOG("dvb-recorder: tune failed\n");
                    video_output_set_infile(appdata.video_output, -1);
                    break;
                case DVB_STREAM_STATUS_EOS:
                    LOG("dvb-recorder: STREAM EOS\n");
                    break;
                case DVB_STREAM_STATUS_STOPPED:
                    LOG("dvb-recorder: stopped\n");
                    video_output_set_infile(appdata.video_output, -1);
                    appdata.is_sdt_ready = 0;
                    appdata.is_video_ready = 0;
                    appdata.notified_channel_change = 0;
                    break;
                case DVB_STREAM_STATUS_RUNNING:
                    LOG("dvb-recorder: running\n");
                    break;
                default:
                    break;
            }
            break;
        case DVB_RECORDER_EVENT_VIDEO_DIED:
            LOG("dvb-recorder: video died\n");
            video_output_set_infile(appdata.video_output,
                                    dvb_recorder_enable_video_source(appdata.recorder, TRUE));
            break;
        case DVB_RECORDER_EVENT_EIT_CHANGED:
            {
                LOG("EIT changed\n");

                GList *events = dvb_recorder_get_epg(appdata.recorder);
                ui_epg_list_update_events(UI_EPG_LIST(widgets.epg_list), events);

                LOG("Updated events\n");

                g_list_free(events);
            }
            break;
        case DVB_RECORDER_EVENT_SDT_CHANGED:
            LOG("SDT changed\n");
            appdata.is_sdt_ready = 1;
            main_notify_channel_change();
            break;
        case DVB_RECORDER_EVENT_CHANNEL_CHANGED:
            LOG("Channel changed to %u\n", ((DVBRecorderEventChannelChanged *)event)->channel_id);
            appstatus.recorder.channel_id = ((DVBRecorderEventChannelChanged *)event)->channel_id;
            appstatus.recorder.running = 1;

            ui_sidebar_channels_set_current_channel(UI_SIDEBAR_CHANNELS(widgets.channel_list),
                    appstatus.recorder.channel_id, FALSE);
            break;
        default:
            break;
    }
}

void main_init_actions(void)
{
    cmd_action_register("toggle_fullscreen", (CmdCallbackProc)main_toggle_fullscreen, NULL);
    cmd_action_register("toggle_record", (CmdCallbackProc)main_action_record, NULL);
    cmd_action_register("toggle_clock", (CmdCallbackProc)main_action_toggle_clock, NULL);
    cmd_action_register("snapshot", (CmdCallbackProc)main_action_snapshot, NULL);
    cmd_action_register("toggle_mute", (CmdCallbackProc)main_action_toggle_mute, NULL);
    cmd_action_register("quit", (CmdCallbackProc)main_action_quit, NULL);
    cmd_action_register("audio_next", (CmdCallbackProc)main_action_audio_next, NULL);
    cmd_action_register("focus_switch_channel_main", (CmdCallbackProc)main_action_switch_channel_main_focus, NULL);
}


void main_init_commands(void)
{
    config_enum_bindings((CfgEnumBindingProc)cmd_add, NULL);
}

gboolean main_debug_timestamp_to_stderr(gpointer userdata)
{
    time_t t;
    static char buf[64];
    static struct tm bdt, *pdt = NULL;

    time(&t);
    localtime_r(&t, &bdt);
    pdt = &bdt;

    float strength = dvb_recorder_get_signal_strength(appdata.recorder);

    strftime(buf, 63, "[%Y%m%d-%H%M%S]", pdt);
    fputs(buf, stderr);
    fprintf(stderr, " signal strength: %f\n", strength);

    return TRUE;
}

int main(int argc, char **argv)
{
    if (!XInitThreads()) {
        fprintf(stderr, "XInitThreads() failed.\n");
        return 1;
    }

    gtk_init(&argc, &argv);

    /* <DEBUG> */
    main_debug_timestamp_to_stderr(NULL);
    g_timeout_add_seconds(10, main_debug_timestamp_to_stderr, NULL);
    /* </DEBUG> */

    gchar *config = g_build_filename(
            g_get_user_config_dir(),
            "dvb-recorder",
            "config",
            NULL);

    if (config_load(config) != 0)
        fprintf(stderr, "Failed to load config.\n");
    g_free(config);

    gchar *status_file = g_build_filename(
            g_get_user_config_dir(),
            "dvb-recorder",
            "status",
            NULL);
    status_restore(&appstatus, status_file);

    main_init_channel_db();

    /* setup librecorder */
    appdata.recorder = dvb_recorder_new(main_recorder_event_callback, NULL);
    dvb_recorder_set_logger(appdata.recorder, (DVBRecorderLoggerProc)log_logger_cb, NULL);

    gchar *value;
    value = main_get_capture_dir();
    dvb_recorder_set_capture_dir(appdata.recorder, value);
    g_free(value);
    if (config_get("main", "capture-filename", CFG_TYPE_STRING, &value) == 0) {
        dvb_recorder_set_record_filename_pattern(appdata.recorder, value);
        g_free(value);
    }
    else {
        dvb_recorder_set_record_filename_pattern(appdata.recorder, "capture-${service_name}-${program_name}-${date:%Y%m%d-%H%M%S}.ts");
    }

    gint ival;
    if (config_get("dvb", "record-streams", CFG_TYPE_INT, &ival) == 0) {
        LOG("record streams from config: 0x%x\n", ival);
        dvb_recorder_set_record_filter(appdata.recorder, ival);
    }

    main_init_actions();
    main_init_commands();

    appdata.command_mode = CMD_MODE_NORMAL;

    main_init_window();
    main_init_control_dialog();
    main_init_channels_dialog();
    main_init_epg_dialog();
    ui_epg_list_set_recorder_handle(UI_EPG_LIST(widgets.epg_list), appdata.recorder);
    main_init_channel_properties_dialog();

    if (appstatus.gui.main_window.initialized &&
            appstatus.gui.main_window.fullscreen)
        main_window_status_set_fullscreen(TRUE);

    if (appstatus.gui.control_dialog.initialized)
        main_window_status_set_show(widgets.control_dialog, &appstatus.gui.control_dialog,
                                    appstatus.gui.control_dialog.show_window);
    if (appstatus.gui.channels_dialog.initialized)
        main_window_status_set_show(widgets.channels_dialog, &appstatus.gui.channels_dialog,
                                    appstatus.gui.channels_dialog.show_window);
    if (appstatus.gui.epg_dialog.initialized)
        main_window_status_set_show(widgets.epg_dialog, &appstatus.gui.epg_dialog,
                                    appstatus.gui.epg_dialog.show_window);
    if (appstatus.gui.channel_properties_dialog.initialized)
        main_window_status_set_show(widgets.channel_properties_dialog, &appstatus.gui.channel_properties_dialog,
                                    appstatus.gui.channel_properties_dialog.show_window);

    gtk_window_present(GTK_WINDOW(widgets.main_window));

    appdata.video_output = video_output_new(widgets.drawing_area, main_video_output_event_callback, NULL);

    if (config_get("video", "brightness", CFG_TYPE_INT, &ival) == 0)
        video_output_set_brightness(appdata.video_output, ival);
    if (config_get("video", "contrast", CFG_TYPE_INT, &ival) == 0)
        video_output_set_contrast(appdata.video_output, ival);
    if (config_get("video", "hue", CFG_TYPE_INT, &ival) == 0)
        video_output_set_hue(appdata.video_output, ival);
    if (config_get("video", "saturation", CFG_TYPE_INT, &ival) == 0)
        video_output_set_saturation(appdata.video_output, ival);

    appdata.osd = osd_new(appdata.video_output, (OSDOverlayRemovedCallback)main_osd_overlay_removed, NULL);

    gboolean bval;

    if (!appstatus.recorder.initialized) {
        appstatus.recorder.initialized = 1;
        appstatus.recorder.volume = 1.0;
        appstatus.recorder.running = 0;
        appstatus.recorder.channel_id = 0;
        appstatus.recorder.mute = 0;
        appstatus.recorder.show_clock = 0;

        main_action_set_volume(1.0);
    }
    else {
        main_action_set_volume(appstatus.recorder.volume);
        main_action_set_mute(appstatus.recorder.mute);

        if (config_get("dvb", "resume-channel-at-startup", CFG_TYPE_BOOLEAN, &bval) != 0)
            bval = TRUE;

        if (appstatus.recorder.running && bval)
            dvb_recorder_set_channel(appdata.recorder, (guint64)appstatus.recorder.channel_id);
        else
            appstatus.recorder.running = 0;

        main_action_set_show_clock(appstatus.recorder.show_clock);

        ui_sidebar_channels_set_current_list(UI_SIDEBAR_CHANNELS(widgets.channel_list), appstatus.recorder.fav_list_id);
        ui_sidebar_channels_set_current_signal_source(UI_SIDEBAR_CHANNELS(widgets.channel_list), appstatus.recorder.signal_source);
        ui_sidebar_channels_set_current_channel(UI_SIDEBAR_CHANNELS(widgets.channel_list), appstatus.recorder.channel_id, FALSE);
    }

    dvb_recorder_enable_scheduled_events(appdata.recorder, TRUE);

    gtk_main();

    appdata.ignore_events = 1;
    status_save(&appstatus, status_file);
    g_free(status_file);

    osd_cleanup(appdata.osd);
    LOG("disable video source\n");
    dvb_recorder_enable_video_source(appdata.recorder, FALSE);
    LOG("destroying video\n");
    video_output_destroy(appdata.video_output);
    LOG("destroying recorder\n");
    dvb_recorder_destroy(appdata.recorder);

    cmd_clear_list();
    cmd_action_cleanup();

    config = g_build_filename(
            g_get_user_config_dir(),
            "dvb-recorder",
            "config",
            NULL);
    config_save(config);
    g_free(config);

    config_free();

    if (appdata.hide_cursor_timer)
        g_timer_destroy(appdata.hide_cursor_timer);
    if (appdata.record_status_update_source)
        g_source_remove(appdata.record_status_update_source);
    if (appdata.hide_cursor_source)
        g_source_remove(appdata.hide_cursor_source);
    if (appdata.blank_cursor)
        g_object_unref(G_OBJECT(appdata.blank_cursor));
    g_free(appdata.rec_status_format_normal);
    g_free(appdata.rec_status_format_recording);

    LOG("done\n");
    return 0;
}
