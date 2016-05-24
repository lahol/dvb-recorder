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
#include <gst/audio/streamvolume.h>
#include <gst/video/video.h>
#include <gdk/gdkx.h>

#include "video-output.h"
#include "utils.h"

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
    GstElement *cairooverlay;
    GstElement *videoconvert[2];
    GstVideoInfo videoinfo;

    GThread *thread;
    GMutex pipeline_mutex;
    GMutex infile_mutex;

    gdouble volume;
    uint32_t write_error : 1;
    uint32_t is_muted : 1;
    uint32_t videoinfo_valid : 1;
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

    vo->volume = 1.0;

    g_mutex_init(&vo->pipeline_mutex);
    g_mutex_init(&vo->infile_mutex);

    return vo;
}

void video_output_destroy(VideoOutput *vo)
{
    fprintf(stderr, "video_output_destroy\n");
    if (vo == NULL)
        return;

    g_mutex_lock(&vo->infile_mutex);
    video_output_clear_pipeline(vo);
    g_mutex_unlock(&vo->infile_mutex);
    g_free(vo);
}

void video_output_stream_start(VideoOutput *vo)
{
    fprintf(stderr, "video_output_stream_start\n");
/*    pipe(vo->communication_pipe);
    pipe(vo->video_pipe);
    vo->thread = g_thread_new("VideoThread", (GThreadFunc)video_output_thread_proc, vo);*/
    if (vo->infile == -1) {
        return;
    }
    video_output_setup_pipeline(vo);
    gst_element_set_state(vo->pipeline, GST_STATE_PLAYING);
}

void video_output_stream_stop(VideoOutput *vo)
{
    fprintf(stderr, "video_output_stream_stop\n");
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
    fprintf(stderr, "video_output_set_infile: %d\n", fd);
    g_return_if_fail(vo != NULL);

    g_mutex_lock(&vo->infile_mutex);
    fprintf(stderr, "vo->infile: %d\n", vo->infile);
    if (vo->infile != -1) {
        vo->infile = -1;
        video_output_stream_stop(vo);
    }

    vo->infile = fd;
    if (vo->infile != -1) {
        video_output_stream_start(vo);
    }

    g_mutex_unlock(&vo->infile_mutex);
}

gboolean video_output_snapshot(VideoOutput *vo, const gchar *filename)
{
    GstCaps *caps = NULL;
    GstStructure *s;
    gint width = 0, height = 0, bpp = 0;
    gboolean result = FALSE;

#if GST_CHECK_VERSION(1, 0, 0)
    GstSample *sample = NULL;
    GstBuffer *buffer = NULL;
    GstMemory *memory = NULL;

    caps = gst_caps_new_simple("video/x-raw",
            "format", G_TYPE_STRING, "RGB",
            "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
            NULL);

    g_signal_emit_by_name(vo->playsink, "convert-sample", caps, &sample);
    gst_caps_unref(caps);
    caps = NULL;

    if (sample == NULL)
        goto error;

    caps = gst_sample_get_caps(sample);
    if (caps == NULL)
        goto error;

    gchar *strcaps = gst_caps_to_string(caps);
    fprintf(stderr, "Buffer caps: %s\n", strcaps);
    g_free(strcaps);

    s = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);
    bpp = 24;
    
    fprintf(stderr, "w: %d, h: %d, bpp: %d\n", width, height, bpp);


    buffer = gst_sample_get_buffer(sample);
    if (buffer == NULL)
        goto error;

    memory = gst_buffer_get_all_memory(buffer);
    if (memory == NULL)
        goto error;

    fprintf(stderr, "memory: %p\n", memory);
    GstMapInfo mapinfo;
    gst_memory_map(memory, &mapinfo, GST_MAP_READ);

    fprintf(stderr, "memory size: %zd\n", mapinfo.size);

    if (!util_write_data_to_png(filename, mapinfo.data, width, height, bpp)) {
        gst_memory_unmap(memory, &mapinfo);
        goto error;
    }

    gst_memory_unmap(memory, &mapinfo);

    result = TRUE;

error:
    if (memory)
        gst_memory_unref(memory);
    if (sample)
        gst_sample_unref(sample);
    
    return result;
    
#else
    GstBuffer *buffer = NULL;

    caps = gst_caps_new_simple("video/x-raw-rgb",
            "format", G_TYPE_STRING, "RGB",
            "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
            NULL);

    g_signal_emit_by_name(vo->playsink, "convert-frame", caps, &buffer);
    gst_caps_unref(caps);
    caps = NULL;

    if (buffer == NULL)
        goto error;

    caps = gst_buffer_get_caps(buffer);
    if (caps == NULL)
        goto error;

    gchar *strcaps = gst_caps_to_string(caps);
    fprintf(stderr, "Buffer caps: %s\n", strcaps);
    g_free(strcaps);

    s = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);
    gst_structure_get_int(s, "bpp", &bpp);

    if (!util_write_data_to_png(filename, buffer->data, width, height, bpp))
        goto error;

    result = TRUE;

error:
    if (caps)
        gst_caps_unref(caps);
    if (buffer)
        gst_buffer_unref(buffer);

    return result;
#endif
}

void video_output_set_mute(VideoOutput *vo, gboolean mute)
{
    g_return_if_fail(vo != NULL);

    vo->is_muted = mute;

    if (vo->playsink)
        gst_stream_volume_set_mute(GST_STREAM_VOLUME(vo->playsink), (gboolean)vo->is_muted);
}

gboolean video_output_toggle_mute(VideoOutput *vo)
{
    g_return_val_if_fail(vo != NULL, FALSE);

    video_output_set_mute(vo, !vo->is_muted);

    return (gboolean)vo->is_muted;
}

gboolean video_output_get_mute(VideoOutput *vo)
{
    g_return_val_if_fail(vo != NULL, FALSE);

    /* return last status, even if no pipeline is active */
    return (gboolean)vo->is_muted;
}

void video_output_set_volume(VideoOutput *vo, gdouble volume)
{
    g_return_if_fail(vo != NULL);

    if (volume < 0.0)
        volume = 0.0;
    if (volume > 1.0)
        volume = 1.0;
    vo->volume = volume;

    if (vo->playsink)
        gst_stream_volume_set_volume(GST_STREAM_VOLUME(vo->playsink),
                                     GST_STREAM_VOLUME_FORMAT_LINEAR,
                                     volume);
}

gdouble video_output_get_volume(VideoOutput *vo)
{
    g_return_val_if_fail(vo != NULL, 0.0);

    /* see also: http://gstreamer.freedesktop.org/data/doc/gstreamer/head/gst-plugins-base-libs/html/gst-plugins-base-libs-gststreamvolume.html#gst-stream-volume-convert-volume */
    return vo->volume;
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

static void video_output_unknown_type_handler(GstElement *bin, GstPad *pad, GstCaps *caps, gpointer data)
{
    GstStructure *structure = gst_caps_get_structure(caps, 0);
    const gchar *type = gst_structure_get_name(structure);

    fprintf(stderr, "unknown type: %s\n", type);
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

    fprintf(stderr, "VideoOutput, pad new type: %s\n", new_pad_type);

    GstElementClass *cls;
    GstPadTemplate *templ = NULL;

    if (g_str_has_prefix(new_pad_type, "audio")) {
        cls = GST_ELEMENT_GET_CLASS(vo->playsink);
        templ = gst_element_class_get_pad_template(cls, "audio_sink");
        if (templ)
            sink_pad = gst_element_request_pad(vo->playsink, templ, NULL, NULL);
    }
    else if (g_str_has_prefix(new_pad_type, "video")) {
/*        cls = GST_ELEMENT_GET_CLASS(vo->cairooverlay);
        templ = gst_element_class_get_pad_template(cls, "sink");
        if (templ)
            sink_pad = gst_element_request_pad(vo->cairooverlay, templ, NULL, NULL);*/
        sink_pad = gst_element_get_static_pad(vo->videoconvert[0], "sink");

    }

    if (sink_pad) {
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
    else {
        g_printerr("could not get sink pad (templ: %p)\n", templ);
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

static void video_output_cairo_caps_changed(GstElement *overlay, GstCaps *caps, VideoOutput *vo)
{
    vo->videoinfo_valid  = !!gst_video_info_from_caps(&vo->videoinfo, caps);
}

static void video_output_cairo_draw(GstElement *overlay, cairo_t *cr, guint64 timestamp, guint64 duration, VideoOutput *vo)
{
    double scale = 1.0f;
    int width, height;

    if (vo->videoinfo_valid == 0)
        return;
    width = GST_VIDEO_INFO_WIDTH(&vo->videoinfo);
    height = GST_VIDEO_INFO_HEIGHT(&vo->videoinfo);

    cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.8);
    cairo_rectangle(cr, 0, 0, 20, 20);
    cairo_fill(cr);
}

void video_output_setup_pipeline(VideoOutput *vo)
{
    fprintf(stderr, "video_output_setup_pipeline: %p\n", vo->pipeline);
    g_mutex_lock(&vo->pipeline_mutex);
    if (vo->pipeline) {
        g_mutex_unlock(&vo->pipeline_mutex);
        return;
    }

    fprintf(stderr, "new pipeline\n");
    vo->pipeline = gst_pipeline_new(NULL);

    fprintf(stderr, "make xvimagesink\n");
    vo->vsink = gst_element_factory_make("xvimagesink", "xvimagesink");
    g_object_set(G_OBJECT(vo->vsink), "force-aspect-ratio", TRUE, NULL);

    fprintf(stderr, "make playsink\n");
    vo->playsink = gst_element_factory_make("playsink", "playsink");
    g_object_set(G_OBJECT(vo->playsink),
            "video-sink", vo->vsink,
            "volume", vo->volume,
            "mute", vo->is_muted,
            NULL);

    fprintf(stderr, "video_output_setup_pipeline, make fdsrc\n");
    GstElement *source = gst_element_factory_make("fdsrc", "fdsrc");
    fprintf(stderr, "fdsrc: %d\n", vo->infile);
    g_object_set(G_OBJECT(source), "fd", vo->infile, NULL);

    /* check if textoverlay/textrender suffices and we can get rid of videoconvert */
    vo->cairooverlay = gst_element_factory_make("cairooverlay", "cairooverlay");
    /* videoconvert was ffmpegcolorspace before 1.0 */
    vo->videoconvert[0] = gst_element_factory_make("videoconvert", "videoconvert0");
    vo->videoconvert[1] = gst_element_factory_make("videoconvert", "videoconvert1");
    if (!vo->cairooverlay || !vo->videoconvert[0] || !vo->videoconvert[1])
        fprintf(stderr, "cairooverlay: %p, videoconvert: %p, colorspace: %p\n",
                vo->cairooverlay, vo->videoconvert[0], vo->videoconvert[1]);
    g_signal_connect(G_OBJECT(vo->cairooverlay), "draw",
            G_CALLBACK(video_output_cairo_draw), vo);
    g_signal_connect(G_OBJECT(vo->cairooverlay), "caps-changed",
            G_CALLBACK(video_output_cairo_caps_changed), vo);

#if GST_CHECK_VERSION(1, 0, 0)
    GstElement *decoder = gst_element_factory_make("decodebin", "decodebin");
#else
    GstElement *decoder = gst_element_factory_make("decodebin2", "decodebin");
#endif
    g_signal_connect(G_OBJECT(decoder), "pad-added",
            G_CALLBACK(video_output_pad_added_handler), vo);
    g_signal_connect(G_OBJECT(decoder), "unknown-type",
            G_CALLBACK(video_output_unknown_type_handler), NULL);

    fprintf(stderr, "pipeline: %p, source: %p, decoder: %p\n",
            vo->pipeline, source, decoder);
    gst_bin_add_many(GST_BIN(vo->pipeline), source, decoder, vo->videoconvert[0], vo->cairooverlay,
            vo->videoconvert[1], vo->playsink, NULL);
    if (!gst_element_link(source, decoder)) {
        g_printerr("Elements could not be linked. (source -> decoder)\n");
    }
    if (!gst_element_link_many(vo->videoconvert[0], vo->cairooverlay, vo->videoconvert[1], NULL)) {
        g_printerr("Elements could not be linked. (videoconvert[0] -> cairooverlay -> videoconvert[1])\n");
    }

    GstPad *playsink_video_pad = gst_element_get_request_pad(vo->playsink, "video_sink");
    GstPad *videoconvert_pad = gst_element_get_static_pad(vo->videoconvert[1], "src");
    GstPadLinkReturn ret;
    if (playsink_video_pad && videoconvert_pad) {
        ret = gst_pad_link(videoconvert_pad, playsink_video_pad);
        if (GST_PAD_LINK_FAILED(ret))
            g_print("Linking convert -> playsink failed\n");
        else
            g_print("Linking successful\n");
    }
    else {
        g_printerr("Could not request video pad or convert pad: %p, %p\n", playsink_video_pad, videoconvert_pad);
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

    g_mutex_unlock(&vo->pipeline_mutex);
}

void video_output_clear_pipeline(VideoOutput *vo)
{
    fprintf(stderr, "video_output_clear_pipeline: %p\n", vo->pipeline);
    g_mutex_lock(&vo->pipeline_mutex);
    if (vo->pipeline) {
        gst_element_set_state(vo->pipeline, GST_STATE_NULL);
        gst_object_unref(vo->pipeline);
    }

    vo->pipeline = NULL;
    g_mutex_unlock(&vo->pipeline_mutex);
}
