#pragma once

#include <glib.h>
#include "video-output.h"
#include <dvbrecorder/dvbrecorder.h>

typedef struct _OSD OSD;

OSD *osd_new(DVBRecorder *recorder, VideoOutput *vo);
void osd_cleanup(OSD *osd);

void osd_update_channel_display(OSD *osd, guint timeout);
