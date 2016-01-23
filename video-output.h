#pragma once

#include <gtk/gtk.h>

typedef struct _VideoOutput VideoOutput;

VideoOutput *video_output_new(GtkWidget *drawing_area);
void video_output_destroy(VideoOutput *vo);

void video_output_set_infile(VideoOutput *vo, int fd);

gboolean video_output_snapshot(VideoOutput *vo, const gchar *filename);
gboolean video_output_toggle_mute(VideoOutput *vo);
gboolean video_output_get_mute(VideoOutput *vo);
void video_output_set_volume(VideoOutput *vo, gdouble volume);
gdouble video_output_get_volume(VideoOutput *vo);
