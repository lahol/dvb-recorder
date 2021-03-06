#pragma once

#include <gtk/gtk.h>

typedef struct _VideoOutput VideoOutput;

typedef enum {
    VIDEO_OUTPUT_EVENT_READY,
    VIDEO_OUTPUT_EVENT_PAUSED,
    VIDEO_OUTPUT_EVENT_PLAYING,
} VideoOutputEventType;
/* video output, event id, userdata */
typedef void (*VIDEOOUTPUTEVENTCALLBACK)(VideoOutput *, VideoOutputEventType, gpointer);

VideoOutput *video_output_new(GtkWidget *drawing_area, VIDEOOUTPUTEVENTCALLBACK event_cb, gpointer event_data);
void video_output_destroy(VideoOutput *vo);

void video_output_set_infile(VideoOutput *vo, int fd);

gboolean video_output_snapshot(VideoOutput *vo, const gchar *filename);
gboolean video_output_toggle_mute(VideoOutput *vo);
void video_output_set_mute(VideoOutput *vo, gboolean mute);
gboolean video_output_get_mute(VideoOutput *vo);
void video_output_set_volume(VideoOutput *vo, gdouble volume);
gdouble video_output_get_volume(VideoOutput *vo);

void video_output_set_overlay_surface(VideoOutput *vo, cairo_surface_t *overlay);
gboolean video_output_get_overlay_surface_parameters(VideoOutput *vo, gint *width, gint *height, gdouble *pixel_aspect);

void video_output_set_brightness(VideoOutput *vo, gint brightness);
void video_output_set_contrast(VideoOutput *vo, gint contrast);
void video_output_set_hue(VideoOutput *vo, gint hue);
void video_output_set_saturation(VideoOutput *vo, gint saturation);

void video_output_set_audio_channel(VideoOutput *vo, guint channel);
guint video_output_get_audio_channel(VideoOutput *vo, guint *total);
void video_output_audio_channel_next(VideoOutput *vo);
void video_output_audio_channel_prev(VideoOutput *vo);
