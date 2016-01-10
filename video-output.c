#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <sys/poll.h>
#include <fcntl.h>

#include <gst/gst.h>
#if GST_CHECK_VERSION(1, 0, 0)
#include <gst/video/videooverlay.h>
#else
#include <gst/interfaces/xoverlay.h>
#endif
#include <gst/base/gstbasesink.h>
#include <gdk/gdkx.h>

#include "video-output.h"

struct _VideoOutput {
    GtkWidget *drawing_area;
    gulong window_id;

    int infile;
    int communication_pipe[2];
    int video_pipe[2];

    GstElement *pipeline;
    GstState state;

    GstElement *vsink;
    GstElement *playsink;

    GThread *thread;

    uint32_t write_error : 1;
};

void video_output_setup_pipeline(VideoOutput *vo);
void video_output_clear_pipeline(VideoOutput *vo);
gpointer video_output_thread_proc(VideoOutput *vo);

VideoOutput *video_output_new(GtkWidget *drawing_area)
{
    VideoOutput *vo = g_malloc0(sizeof(VideoOutput));

    gst_init(NULL, NULL);

    vo->drawing_area = drawing_area;
    vo->window_id = GDK_WINDOW_XID(gtk_widget_get_window(GTK_WIDGET(drawing_area)));
    fprintf(stderr, "vo->window_id: %lu\n", vo->window_id);
    vo->infile = -1;
    vo->communication_pipe[0] = -1;
    vo->communication_pipe[1] = -1;

    return vo;
}

void video_output_destroy(VideoOutput *vo)
{
    fprintf(stderr, "video_output_destroy\n");
    if (vo == NULL)
        return;

    video_output_clear_pipeline(vo);
    g_free(vo);
}

void video_output_stream_start(VideoOutput *vo)
{
/*    pipe(vo->communication_pipe);
    pipe(vo->video_pipe);
    vo->thread = g_thread_new("VideoThread", (GThreadFunc)video_output_thread_proc, vo);*/
    video_output_setup_pipeline(vo);
    gst_element_set_state(vo->pipeline, GST_STATE_PLAYING);
}

void video_output_stream_stop(VideoOutput *vo)
{
 /*   if (vo->thread) {
        write(vo->communication_pipe[1], "stop", 4);
        g_thread_join(vo->thread);
        vo->thread = NULL;

        close(vo->communication_pipe[0]);
        close(vo->communication_pipe[1]);
        close(vo->video_pipe[0]);
        close(vo->video_pipe[1]);
        vo->communication_pipe[0] = -1;
        vo->communication_pipe[1] = -1;
        vo->video_pipe[0] = -1;
        vo->video_pipe[1] = -1;
    }*/
    video_output_clear_pipeline(vo);
}

void video_output_set_infile(VideoOutput *vo, int fd)
{
    g_return_if_fail(vo != NULL);

    if (vo->infile != -1) {
        video_output_stream_stop(vo);
    }

    vo->infile = fd;
    if (vo->infile != -1) {
        video_output_stream_start(vo);
    }
}

gpointer video_output_thread_proc(VideoOutput *vo)
{
    fprintf(stderr, "video_output_thread_proc\n");

    uint8_t buffer[16384];
    ssize_t bytes_read;
    struct pollfd pfd[2];

    pfd[0].fd = vo->communication_pipe[0];
    pfd[0].events = POLLIN;

    pfd[1].fd = vo->infile;
    pfd[1].events = POLLIN;

    vo->write_error = 0;

    fprintf(stderr, "fd = { %d, %d }\n", pfd[0].fd, pfd[1].fd);

    while (1) {
        if (poll(pfd, 2, 1500)) {
            fprintf(stderr, "poll (events: 0x%02x, 0x%02x)\n", pfd[0].revents, pfd[1].revents);
            
            if (pfd[1].revents & POLLIN) {
                bytes_read = read(pfd[1].fd, buffer, 16384);
                if (bytes_read <= 0) {
                    if (errno == EAGAIN)
                        continue;
                    fprintf(stderr, "Error reading data. Stopping thread. (%d) %s\n", errno, strerror(errno));
                    break;
                }
                /* write to video pipe */
                fprintf(stderr, "writing %zd bytes\n", bytes_read);
                fflush(stderr);
                write(vo->video_pipe[1], buffer, bytes_read);
            }
            else if (pfd[1].revents & POLLNVAL) {
                fprintf(stderr, "Remote hung up\n");
                break;
            }
            else if (pfd[0].revents & POLLIN) {
                fprintf(stderr, "Received data on control pipe. Stopping thread.\n");
                break;
            }
        }
    }

    return NULL;
}

static void video_output_pad_added_handler(GstElement *src, GstPad *new_pad, VideoOutput *vo)
{
    g_print("Pad added: %s from %s\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    const gchar *new_pad_type = NULL;
    GstPad *sink_pad = NULL;
    GstPadLinkReturn ret;

#if GST_CHECK_VERSION(1, 0, 0)
    new_pad_caps = gst_pad_get_current_caps(new_pad);
#else
    new_pad_caps = gst_pad_get_caps(new_pad);
#endif
    new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
    new_pad_type = gst_structure_get_name(new_pad_struct);

    g_print("new type: %s\n", new_pad_type);

    GstElementClass *cls = GST_ELEMENT_GET_CLASS(vo->playsink);
    GstPadTemplate *templ = NULL;

    if (g_str_has_prefix(new_pad_type, "audio")) {
        templ = gst_element_class_get_pad_template(cls, "audio_sink");
    }
    else if (g_str_has_prefix(new_pad_type, "video")) {
        templ = gst_element_class_get_pad_template(cls, "video_sink");
    }

    if (templ) {
        sink_pad = gst_element_request_pad(vo->playsink, templ, NULL, NULL);

        if (gst_pad_is_linked(sink_pad)) {
            g_print("Already linked\n");
            goto done;
        }

        ret = gst_pad_link(new_pad, sink_pad);
        if (GST_PAD_LINK_FAILED(ret))
            g_print("Linking failed\n");
        else
            g_print("Linkings successful\n");
    }

done:
    if (new_pad_caps)
        gst_caps_unref(new_pad_caps);
    if (sink_pad)
        gst_object_unref(sink_pad);
}

static GstBusSyncReply video_output_bus_sync_handler(GstBus *bus, GstMessage *message, VideoOutput *vo)
{
/*    fprintf(stderr, "bus_sync_handler\n");*/
#if GST_CHECK_VERSION(1,0,0)
    if (!gst_is_video_overlay_prepare_window_handle_message(message))
        return GST_BUS_PASS;
    if (vo->window_id != 0) {
        GstVideoOverlay *overlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(message));
        gst_video_overlay_set_window_handle(overlay, vo->window_id);
    }
    else {
        fprintf(stderr, "VideoOutput: should have window id now\n");
    }

    gst_message_unref(message);
    return GST_BUS_DROP;
#else
    if (GST_MESSAGE_TYPE(message) != GST_MESSAGE_ELEMENT)
        return GST_BUS_PASS;
    fprintf(stderr, "structure name: %s\n", gst_structure_get_name(message->structure));
    if (!gst_structure_has_name(message->structure, "prepare-xwindow-id"))
        return GST_BUS_PASS;

    if (vo->window_id != 0) {
        fprintf(stderr, "VideoOutput: set window id %lu\n", vo->window_id);
        gst_x_overlay_set_window_handle(GST_X_OVERLAY(GST_MESSAGE_SRC(message)), vo->window_id);
    }
    else {
        fprintf(stderr, "VideoOutput: should have window id now\n");
    }

    gst_message_unref(message);
    return GST_BUS_DROP;
#endif
}

static void video_output_gst_error_cb(GstBus *bus, GstMessage *msg, VideoOutput *vo)
{
    GError *err;
    gchar *debug_info;

    gst_message_parse_error(msg, &err, &debug_info);
    g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
    g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error(&err);
    g_free(debug_info);

    gst_element_set_state(vo->pipeline, GST_STATE_READY);
}

static void video_output_gst_eos_cb(GstBus *bus, GstMessage *msg, VideoOutput *vo)
{
    g_print("End-Of-Stream reached.\n");
    gst_element_set_state(vo->pipeline, GST_STATE_READY);
}

static void video_output_gst_state_changed_cb(GstBus *bus, GstMessage *msg, VideoOutput *vo)
{
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(vo->pipeline)) {
        fprintf(stderr, "State set from %s to %s (pending: %s)\n",
                gst_element_state_get_name(old_state),
                gst_element_state_get_name(new_state),
                gst_element_state_get_name(pending_state));
    }
}

static void video_output_gst_message(GstBus *bus, GstMessage *msg, VideoOutput *vo)
{
    fprintf(stderr, "message from %s: %s\n", GST_OBJECT_NAME(msg->src), gst_message_type_get_name(msg->type));
}

void video_output_setup_pipeline(VideoOutput *vo)
{
    if (vo->pipeline)
        return;

    vo->pipeline = gst_pipeline_new(NULL);

    vo->vsink = gst_element_factory_make("xvimagesink", "xvimagesink");
    g_object_set(G_OBJECT(vo->vsink), "force-aspect-ratio", TRUE, NULL);

    vo->playsink = gst_element_factory_make("playsink", "playsink");
    g_object_set(G_OBJECT(vo->playsink), "video-sink", vo->vsink, NULL);

    GstElement *source = gst_element_factory_make("fdsrc", "fdsrc");
    fprintf(stderr, "fdsrc: %d\n", vo->infile);
    g_object_set(G_OBJECT(source), "fd", vo->infile, NULL);
/*    GstElement *source = gst_element_factory_make("filesrc", "filesrc");
    g_object_set(G_OBJECT(source), "location", "/tmp/ts-dummy.ts", NULL);*/

#if GST_CHECK_VERSION(1, 0, 0)
    GstElement *decoder = gst_element_factory_make("decodebin", "decodebin");
#else
    GstElement *decoder = gst_element_factory_make("decodebin2", "decodebin");
#endif
    g_signal_connect(G_OBJECT(decoder), "pad-added",
            G_CALLBACK(video_output_pad_added_handler), vo);

    fprintf(stderr, "pipeline: %p, source: %p, decoder: %p\n",
            vo->pipeline, source, decoder);
    gst_bin_add_many(GST_BIN(vo->pipeline), source, decoder, vo->playsink, NULL);
    if (!gst_element_link(source, decoder)) {
        g_printerr("Elements could not be linked. (source -> decoder)\n");
    }

    GstBus *bus = gst_element_get_bus(GST_ELEMENT(vo->pipeline));
#if GST_CHECK_VERSION(1, 0, 0)
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)video_output_bus_sync_handler, vo, NULL);
#else
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)video_output_bus_sync_handler, vo);
#endif
    gst_bus_add_signal_watch(bus);
    g_signal_connect(G_OBJECT(bus), "message::error",
            G_CALLBACK(video_output_gst_error_cb), vo);
    g_signal_connect(G_OBJECT(bus), "message::eos",
            G_CALLBACK(video_output_gst_eos_cb), vo);
    g_signal_connect(G_OBJECT(bus), "message::state-changed",
            G_CALLBACK(video_output_gst_state_changed_cb), vo);
/*    g_signal_connect(G_OBJECT(bus), "message",
            G_CALLBACK(video_output_gst_message), vo);*/
    g_object_unref(bus);
}

void video_output_clear_pipeline(VideoOutput *vo)
{
    if (vo->pipeline) {
        gst_element_set_state(vo->pipeline, GST_STATE_NULL);
        gst_object_unref(vo->pipeline);
    }

    vo->pipeline = NULL;
}
