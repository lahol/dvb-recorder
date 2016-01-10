#pragma once

#include <gtk/gtk.h>

typedef struct _VideoOutput VideoOutput;

VideoOutput *video_output_new(GtkWidget *drawing_area);
void video_output_destroy(VideoOutput *vo);

void video_output_set_infile(VideoOutput *vo, int fd);
