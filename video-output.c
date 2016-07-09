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
#include <gst/base/gstbasesrc.h>
#include <gst/audio/streamvolume.h>
#include <gst/video/video.h>
#include <gdk/gdkx.h>

#include "video-output.h"
#include "utils.h"
#include "logging.h"

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
    GstElement *asink;

    GstElement *audio_input_selector;
    GQueue audio_channels;

    GList *request_pads;

    guint width;
    guint height;
    gdouble pixel_aspect;

    GThread *thread;
    GMutex pipeline_mutex;
    GMutex infile_mutex;

    VIDEOOUTPUTEVENTCALLBACK event_cb;
    gpointer event_data;

    gdouble volume;
    uint32_t write_error : 1;
    uint32_t is_muted : 1;
    uint32_t videoinfo_valid : 1;

    gint video_brightness;
    gint video_contrast;
    gint video_hue;
    gint video_saturation;
};

struct VOPadInfo {
    GstElement *element;
    GstPad *pad;
};

void video_output_add_request_pad(VideoOutput *vo, GstElement *element, GstPad *pad);
void video_output_release_request_pads(VideoOutput *vo);

void video_output_setup_pipeline(VideoOutput *vo);
void video_output_clear_pipeline(VideoOutput *vo);
gpointer video_output_thread_proc(VideoOutput *vo);

VideoOutput *video_output_new(GtkWidget *drawing_area, VIDEOOUTPUTEVENTCALLBACK event_cb, gpointer event_data)
{
    VideoOutput *vo = g_malloc0(sizeof(VideoOutput));

    gst_init(NULL, NULL);

    vo->drawing_area = drawing_area;
    vo->window_id = GDK_WINDOW_XID(gtk_widget_get_window(GTK_WIDGET(drawing_area)));
    LOG("vo->window_id: %lu\n", vo->window_id);
    vo->infile = -1;
    vo->communication_pipe[0] = -1;
    vo->communication_pipe[1] = -1;

    vo->volume = 1.0;

    g_mutex_init(&vo->pipeline_mutex);
    g_mutex_init(&vo->infile_mutex);

    vo->event_cb = event_cb;
    vo->event_data = event_data;

    g_queue_init(&vo->audio_channels);

    return vo;
}

void video_output_destroy(VideoOutput *vo)
{
    LOG("video_output_destroy\n");
    if (vo == NULL)
        return;

    g_mutex_lock(&vo->infile_mutex);
    video_output_clear_pipeline(vo);
    g_mutex_unlock(&vo->infile_mutex);
    LOG("video_output_destroy: free vo\n");
    g_free(vo);
    LOG("video_output_destroy: done\n");
}

void video_output_stream_start(VideoOutput *vo)
{
    LOG("video_output_stream_start\n");
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
    LOG("video_output_stream_stop\n");
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
    LOG("video_output_set_infile: %d\n", fd);
    g_return_if_fail(vo != NULL);

    g_mutex_lock(&vo->infile_mutex);
    LOG("vo->infile: %d\n", vo->infile);
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
    LOG("Buffer caps: %s\n", strcaps);
    g_free(strcaps);

    s = gst_caps_get_structure(caps, 0);
    gst_structure_get_int(s, "width", &width);
    gst_structure_get_int(s, "height", &height);
    bpp = 24;
    
    LOG("w: %d, h: %d, bpp: %d\n", width, height, bpp);


    buffer = gst_sample_get_buffer(sample);
    if (buffer == NULL)
        goto error;

    memory = gst_buffer_get_all_memory(buffer);
    if (memory == NULL)
        goto error;

    LOG("memory: %p\n", memory);
    GstMapInfo mapinfo;
    gst_memory_map(memory, &mapinfo, GST_MAP_READ);

    LOG("memory size: %zd\n", mapinfo.size);

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
    LOG("Buffer caps: %s\n", strcaps);
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
    LOG("video_output_thread_proc\n");

    uint8_t buffer[16384];
    ssize_t bytes_read;
    struct pollfd pfd[2];

    pfd[0].fd = vo->communication_pipe[0];
    pfd[0].events = POLLIN;

    pfd[1].fd = vo->infile;
    pfd[1].events = POLLIN;

    vo->write_error = 0;

    LOG("fd = { %d, %d }\n", pfd[0].fd, pfd[1].fd);

    while (1) {
        if (poll(pfd, 2, 1500)) {
            LOG("poll (events: 0x%02x, 0x%02x)\n", pfd[0].revents, pfd[1].revents);
            
            if (pfd[1].revents & POLLIN) {
                bytes_read = read(pfd[1].fd, buffer, 16384);
                if (bytes_read <= 0) {
                    if (errno == EAGAIN)
                        continue;
                    LOG("Error reading data. Stopping thread. (%d) %s\n", errno, strerror(errno));
                    break;
                }
                /* write to video pipe */
                LOG("writing %zd bytes\n", bytes_read);
                fflush(stderr);
                write(vo->video_pipe[1], buffer, bytes_read);
            }
            else if (pfd[1].revents & POLLNVAL) {
                LOG("Remote hung up\n");
                break;
            }
            else if (pfd[0].revents & POLLIN) {
                LOG("Received data on control pipe. Stopping thread.\n");
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

    LOG("unknown type: %s\n", type);
}

static void video_output_decoder_drained_cb(GstElement *bin, VideoOutput *vo)
{
    LOG("decoder drained\n");
}

static void video_output_pad_added_handler(GstElement *src, GstPad *new_pad, VideoOutput *vo)
{
    LOG("Pad added: %s from %s\n", GST_PAD_NAME(new_pad), GST_ELEMENT_NAME(src));
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

    LOG("VideoOutput, pad new type: %s\n", new_pad_type);

    if (g_str_has_prefix(new_pad_type, "audio")) {
        sink_pad = gst_element_get_request_pad(vo->audio_input_selector, "sink_%u");
        video_output_add_request_pad(vo, vo->audio_input_selector, sink_pad);
        if (sink_pad)
            g_queue_push_tail(&vo->audio_channels, sink_pad);
    }
    else if (g_str_has_prefix(new_pad_type, "video")) {
        sink_pad = gst_element_get_static_pad(vo->cairooverlay, "sink");
    }

    if (sink_pad) {
        if (gst_pad_is_linked(sink_pad)) {
            LOG("Already linked\n");
            gst_element_set_locked_state(GST_ELEMENT(new_pad), TRUE);
            gst_element_set_state(GST_ELEMENT(new_pad), GST_STATE_NULL);
            goto done;
        }

        ret = gst_pad_link(new_pad, sink_pad);
        if (GST_PAD_LINK_FAILED(ret)) {
            LOG("Linking failed\n");
            gst_element_set_locked_state(GST_ELEMENT(new_pad), TRUE);
            gst_element_set_state(GST_ELEMENT(new_pad), GST_STATE_NULL);
        }
        else
            LOG("Linkings successful\n");
    }
    else {
        LOG("could not get sink pad\n");
        gst_element_set_locked_state(GST_ELEMENT(new_pad), TRUE);
        gst_element_set_state(GST_ELEMENT(new_pad), GST_STATE_NULL);
    }

done:
    if (new_pad_caps)
        gst_caps_unref(new_pad_caps);
    if (sink_pad)
        gst_object_unref(sink_pad);
}

static GstBusSyncReply video_output_bus_sync_handler(GstBus *bus, GstMessage *message, VideoOutput *vo)
{
    LOG("bus sync handler message: %p\n", message);
    if (GST_MESSAGE_TYPE(message) != GST_MESSAGE_TAG)
        LOG("bus_sync_handler: %s\n", GST_MESSAGE_TYPE_NAME(message));
#if GST_CHECK_VERSION(1,0,0)
*   if (GST_MESSAGE_TYPE(message) == GST_MESSAGE_STATE_CHANGED) {
        GstState oldstate, newstate, pending;
        gst_message_parse_state_changed(message, &oldstate, &newstate, &pending);
        LOG("Element %s changed state from %s to %s (pending: %s)\n",
                GST_OBJECT_NAME(message->src),
                gst_element_state_get_name(oldstate),
                gst_element_state_get_name(newstate),
                gst_element_state_get_name(pending));
    }
    LOG("retrun bus pass: %d\n", gst_is_video_overlay_prepare_window_handle_message(message));
    if (!gst_is_video_overlay_prepare_window_handle_message(message))
        return GST_BUS_PASS;
    if (vo->window_id != 0) {
        GstVideoOverlay *overlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(message));
        gst_video_overlay_set_window_handle(overlay, vo->window_id);
    }
    else {
        LOG("VideoOutput: should have window id now\n");
    }

    LOG("bus_sync_handler message unref\n");
    gst_message_unref(message);
    LOG("bus_sync_handler message unref done\n");
    return GST_BUS_DROP;
#else
    if (GST_MESSAGE_TYPE(message) != GST_MESSAGE_ELEMENT)
        return GST_BUS_PASS;
    LOG("structure name: %s\n", gst_structure_get_name(message->structure));
    if (!gst_structure_has_name(message->structure, "prepare-xwindow-id"))
        return GST_BUS_PASS;

    if (vo->window_id != 0) {
        LOG("VideoOutput: set window id %lu\n", vo->window_id);
        gst_x_overlay_set_window_handle(GST_X_OVERLAY(GST_MESSAGE_SRC(message)), vo->window_id);
    }
    else {
        LOG("VideoOutput: should have window id now\n");
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
    LOG("Error received from element %s: %s\n", GST_OBJECT_NAME(msg->src), err->message);
    LOG("Debugging information: %s\n", debug_info ? debug_info : "none");
    g_clear_error(&err);
    g_free(debug_info);

    gst_element_set_state(vo->pipeline, GST_STATE_READY);
}

static void video_output_gst_eos_cb(GstBus *bus, GstMessage *msg, VideoOutput *vo)
{
    LOG("End-Of-Stream reached.\n");
    gst_element_set_state(vo->pipeline, GST_STATE_READY);
}

static void video_output_gst_state_changed_cb(GstBus *bus, GstMessage *msg, VideoOutput *vo)
{
    GstState old_state, new_state, pending_state;
    gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(vo->pipeline)) {
        LOG("State set from %s to %s (pending: %s)\n",
                gst_element_state_get_name(old_state),
                gst_element_state_get_name(new_state),
                gst_element_state_get_name(pending_state));
    }

    /* pipeline is ready when we are in paused or playing, obviously */
    if (vo->event_cb) {
        switch (new_state) {
            case GST_STATE_READY:
                g_object_set(G_OBJECT(vo->vsink),
                             "brightness", vo->video_brightness,
                             "contrast", vo->video_contrast,
                             "hue", vo->video_hue,
                             "saturation", vo->video_saturation,
                             NULL);
                             
                vo->event_cb(vo, VIDEO_OUTPUT_EVENT_READY, vo->event_data);
                break;
            case GST_STATE_PAUSED:
                vo->event_cb(vo, VIDEO_OUTPUT_EVENT_PAUSED, vo->event_data);
                break;
            case GST_STATE_PLAYING:
                vo->event_cb(vo, VIDEO_OUTPUT_EVENT_PLAYING, vo->event_data);
                break;
            default:
                break;
        }
    }
}

static void video_output_gst_message(GstBus *bus, GstMessage *msg, VideoOutput *vo)
{
/*    if (GST_MESSAGE_TYPE(msg) != GST_MESSAGE_TAG)
        LOG("message from %s: %s\n", GST_OBJECT_NAME(msg->src), gst_message_type_get_name(msg->type));*/
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_WARNING:
            {
                gchar *err_msg = NULL;
                gst_message_parse_warning(msg, NULL, &err_msg);
                LOG(" message: %s\n", err_msg);
                g_free(err_msg);
            }
            break;
        case GST_MESSAGE_ERROR:
            {
                gchar *err_msg = NULL;
                gst_message_parse_error(msg, NULL, &err_msg);
                LOG(" message: %s\n", err_msg);
                g_free(err_msg);
            }
            break;
        default:
            break;
    }
}

static void video_output_cairo_info_changed(GstElement *overlay, guint width, guint height, gdouble pixel_aspect, VideoOutput *vo)
{
    vo->width = width;
    vo->height = height;
    vo->pixel_aspect = pixel_aspect;
    vo->videoinfo_valid = 1;
}

gboolean video_output_get_overlay_surface_parameters(VideoOutput *vo, gint *width, gint *height, gdouble *pixel_aspect)
{
    g_return_val_if_fail(vo != NULL, FALSE);

    if (vo->videoinfo_valid == 0)
        return FALSE;

    if (width)
        *width = vo->width;
    if (height)
        *height = vo->height;
    if (pixel_aspect)
        *pixel_aspect = vo->pixel_aspect;

    return TRUE;
}

void video_output_set_overlay_surface(VideoOutput *vo, cairo_surface_t *overlay)
{
    if (G_IS_OBJECT(vo->cairooverlay))
        g_object_set(G_OBJECT(vo->cairooverlay), "surface", overlay, NULL);
    else if (overlay)
        cairo_surface_destroy(overlay);
}

void video_output_setup_pipeline(VideoOutput *vo)
{
    LOG("video_output_setup_pipeline: %p\n", vo->pipeline);
    g_mutex_lock(&vo->pipeline_mutex);
    if (vo->pipeline) {
        g_mutex_unlock(&vo->pipeline_mutex);
        return;
    }

    LOG("new pipeline\n");
    vo->pipeline = gst_pipeline_new(NULL);

    LOG("make xvimagesink (br: %d, cont: %d, hue: %d, sat: %d)\n",
            vo->video_brightness, vo->video_contrast, vo->video_hue, vo->video_saturation);
    vo->vsink = gst_element_factory_make("xvimagesink", "xvimagesink");
    g_object_set(G_OBJECT(vo->vsink),
                 "force-aspect-ratio", TRUE,
                 NULL);
    LOG("xvimagesink: %p\n", vo->vsink);

/*    vo->asink = gst_element_factory_make("alsasink", "alsasink");
    LOG("audiosink: %p\n", vo->asink);
*/
    vo->audio_input_selector = gst_element_factory_make("input-selector", "audioinputselector");
    LOG("input-selector: %p\n", vo->audio_input_selector);

    LOG("make playsink\n");
    vo->playsink = gst_element_factory_make("playsink", "playsink");
    g_object_set(G_OBJECT(vo->playsink),
            "video-sink", vo->vsink,
/*            "audio-sink", vo->asink,*/
            "volume", vo->volume,
            "mute", vo->is_muted,
            NULL);

    LOG("video_output_setup_pipeline, make fdsrc\n");
    GstElement *source = gst_element_factory_make("fdsrc", "fdsrc");
    LOG("fdsrc: %d\n", vo->infile);
    g_object_set(G_OBJECT(source), "fd", vo->infile, NULL);
    gst_base_src_set_live(GST_BASE_SRC(source), TRUE);

    /* check if textoverlay/textrender suffices and we can get rid of videoconvert */
    vo->cairooverlay = gst_element_factory_make("cairostaticoverlay", "cairooverlay");
    /* videoconvert was ffmpegcolorspace before 1.0 */
    g_signal_connect(G_OBJECT(vo->cairooverlay), "info-changed",
            G_CALLBACK(video_output_cairo_info_changed), vo);

#if GST_CHECK_VERSION(1, 0, 0)
    GstElement *decoder = gst_element_factory_make("decodebin", "decodebin");
#else
    GstElement *decoder = gst_element_factory_make("decodebin2", "decodebin");
#endif
    g_signal_connect(G_OBJECT(decoder), "pad-added",
            G_CALLBACK(video_output_pad_added_handler), vo);
    g_signal_connect(G_OBJECT(decoder), "unknown-type",
            G_CALLBACK(video_output_unknown_type_handler), NULL);
    g_signal_connect(G_OBJECT(decoder), "drained",
            G_CALLBACK(video_output_decoder_drained_cb), vo);

    LOG("pipeline: %p, source: %p, decoder: %p\n",
            vo->pipeline, source, decoder);
    gst_bin_add_many(GST_BIN(vo->pipeline), source, decoder, vo->audio_input_selector, vo->cairooverlay,
            vo->playsink, NULL);
    if (!gst_element_link(source, decoder)) {
        LOG("Elements could not be linked. (source -> decoder)\n");
    }

    GstPad *playsink_video_pad = gst_element_get_request_pad(vo->playsink, "video_sink");
    video_output_add_request_pad(vo, vo->playsink, playsink_video_pad);
    GstPad *videoconvert_pad = gst_element_get_static_pad(vo->cairooverlay, "src");
    GstPadLinkReturn ret;
    if (playsink_video_pad && videoconvert_pad) {
        ret = gst_pad_link(videoconvert_pad, playsink_video_pad);
        if (GST_PAD_LINK_FAILED(ret))
            LOG("Linking overlay -> playsink failed\n");
        else
            LOG("Linking successful\n");
    }
    else {
        LOG("Could not request video pad or convert pad: %p, %p\n", playsink_video_pad, videoconvert_pad);
    }

    GstPad *playsink_audio_pad = gst_element_get_request_pad(vo->playsink, "audio_sink");
    video_output_add_request_pad(vo, vo->playsink, playsink_audio_pad);
    GstPad *input_selector_pad = gst_element_get_static_pad(vo->audio_input_selector, "src");
    if (playsink_audio_pad && input_selector_pad) {
        ret = gst_pad_link(input_selector_pad, playsink_audio_pad);
        if (GST_PAD_LINK_FAILED(ret))
            LOG("Linking input-selector -> playsink failed\n");
        else
            LOG("Linking successful\n");
    }
    else {
        LOG("Could not request audio pad or input-selector pad: %p, %p\n", playsink_audio_pad, input_selector_pad);
    }

    if (playsink_audio_pad)
        gst_object_unref(playsink_audio_pad);
    if (playsink_video_pad)
        gst_object_unref(playsink_video_pad);
    if (videoconvert_pad)
        gst_object_unref(videoconvert_pad);
    if (input_selector_pad)
        gst_object_unref(input_selector_pad);

    GstBus *bus = gst_element_get_bus(GST_ELEMENT(vo->pipeline));
#if GST_CHECK_VERSION(1, 0, 0)
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)video_output_bus_sync_handler, vo, NULL);
#else
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)video_output_bus_sync_handler, vo);
#endif
    gst_bus_add_signal_watch(bus);
/*    gst_bus_add_watch(bus, (GstBusFunc)video_output_gst_message, vo);*/
    g_signal_connect(G_OBJECT(bus), "message::error",
            G_CALLBACK(video_output_gst_error_cb), vo);
    g_signal_connect(G_OBJECT(bus), "message::eos",
            G_CALLBACK(video_output_gst_eos_cb), vo);
    g_signal_connect(G_OBJECT(bus), "message::state-changed",
            G_CALLBACK(video_output_gst_state_changed_cb), vo);
    g_signal_connect(G_OBJECT(bus), "message",
            G_CALLBACK(video_output_gst_message), vo);
    g_object_unref(bus);

    g_object_set(G_OBJECT(vo->vsink),
                 "brightness", vo->video_brightness,
                 "contrast", vo->video_contrast,
                 "hue", vo->video_hue,
                 "saturation", vo->video_saturation,
                 NULL);
    g_mutex_unlock(&vo->pipeline_mutex);
}

void video_output_clear_pipeline(VideoOutput *vo)
{
    LOG("video_output_clear_pipeline: %p\n", vo->pipeline);
    g_mutex_lock(&vo->pipeline_mutex);
    if (vo->pipeline) {
        LOG("set pipeline to state NULL\n");
        gst_element_set_state(vo->pipeline, GST_STATE_NULL);

        LOG("release request pads\n");
        video_output_release_request_pads(vo);
        LOG("clear queue\n");
        g_queue_clear(&vo->audio_channels);
        LOG("unref pipeline\n");

        gst_object_unref(vo->pipeline);
    }

    vo->pipeline = NULL;
    g_mutex_unlock(&vo->pipeline_mutex);
    LOG("video_output_clear_pipeline done\n");
}

void video_output_add_request_pad(VideoOutput *vo, GstElement *element, GstPad *pad)
{
    if (!vo || !pad)
        return;

    struct VOPadInfo *entry = g_malloc(sizeof(struct VOPadInfo));
    entry->element = element;
    entry->pad = pad;
    gst_object_ref(GST_OBJECT(pad));

    vo->request_pads = g_list_prepend(vo->request_pads, entry);
}

void _video_output_request_pad_free(struct VOPadInfo *info)
{
    if (info) {
        gst_element_release_request_pad(info->element, info->pad);
        gst_object_unref(GST_OBJECT(info->pad));
        g_free(info);
    }
}

void video_output_release_request_pads(VideoOutput *vo)
{
    if (vo) {
        g_list_free_full(vo->request_pads, (GDestroyNotify)_video_output_request_pad_free);
        vo->request_pads = NULL;
    }
}

void video_output_set_brightness(VideoOutput *vo, gint brightness)
{
    g_return_if_fail(vo != NULL);

    if (brightness < -1000)
        brightness = -1000;
    if (brightness > 1000)
        brightness = 1000;

    vo->video_brightness = brightness;

    if (vo->vsink)
        g_object_set(G_OBJECT(vo->vsink), "brightness", vo->video_brightness, NULL);
}

void video_output_set_contrast(VideoOutput *vo, gint contrast)
{
    g_return_if_fail(vo != NULL);

    if (contrast < -1000)
        contrast = -1000;
    if (contrast > 1000)
        contrast = 1000;

    vo->video_contrast = contrast;

    if (vo->vsink)
        g_object_set(G_OBJECT(vo->vsink), "contrast", vo->video_contrast, NULL);
}

void video_output_set_hue(VideoOutput *vo, gint hue)
{
    g_return_if_fail(vo != NULL);

    if (hue < -1000)
        hue = -1000;
    if (hue > 1000)
        hue = 1000;

    vo->video_hue = hue;

    if (vo->vsink)
        g_object_set(G_OBJECT(vo->vsink), "hue", vo->video_hue, NULL);
}

void video_output_set_saturation(VideoOutput *vo, gint saturation)
{
    g_return_if_fail(vo != NULL);

    if (saturation < -1000)
        saturation = -1000;
    if (saturation > 1000)
        saturation = 1000;

    vo->video_saturation = saturation;

    if (vo->vsink)
        g_object_set(G_OBJECT(vo->vsink), "saturation", vo->video_saturation, NULL);
}

void video_output_set_audio_channel(VideoOutput *vo, guint channel)
{
}

guint video_output_get_audio_channel(VideoOutput *vo, guint *total)
{
    if (total)
        *total = 0;

    g_return_val_if_fail(vo != NULL, 0);
    g_return_val_if_fail(GST_IS_ELEMENT(vo->audio_input_selector), 0);

    guint n_pads;
    g_object_get(G_OBJECT(vo->audio_input_selector), "n-pads", &n_pads, NULL);

    if (total)
        *total = n_pads;

    return 0;
}

void video_output_audio_channel_next(VideoOutput *vo)
{
    g_return_if_fail(vo != NULL);
    if (!(GST_IS_ELEMENT(vo->audio_input_selector)))
        return;

    GList *active = NULL;
    GstPad *old_pad = NULL;

    g_object_get(G_OBJECT(vo->audio_input_selector), "active-pad", &old_pad, NULL);

    active = g_list_find(vo->audio_channels.head, old_pad);
    if (!active)
        goto done;

    if (active->next)
        active = active->next;
    else
        active = vo->audio_channels.head;

    if (active->data != old_pad)
        g_object_set(G_OBJECT(vo->audio_input_selector), "active-pad", active->data, NULL);

    LOG("audio switched from %s:%s to %s:%s\n", GST_DEBUG_PAD_NAME(old_pad), GST_DEBUG_PAD_NAME(active->data));

done:
    if (old_pad)
        gst_object_unref(GST_OBJECT(old_pad));
}

void video_output_audio_channel_prev(VideoOutput *vo)
{
}
