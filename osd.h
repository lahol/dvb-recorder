#pragma once

#include <glib.h>
#include "video-output.h"
#include <dvbrecorder/dvbrecorder.h>

typedef struct _OSD OSD;

typedef enum {
    OSD_ALIGN_HORZ_LEFT   = 1 << 0,
    OSD_ALIGN_HORZ_CENTER = 1 << 1,
    OSD_ALIGN_HORZ_RIGHT  = 1 << 2,
    OSD_ALIGN_VERT_TOP    = 1 << 3,
    OSD_ALIGN_VERT_CENTER = 1 << 4,
    OSD_ALIGN_VERT_BOTTOM = 1 << 5
} OSDTextAlignFlags;

typedef void (*OSDOverlayRemovedCallback)(guint32, gpointer);
OSD *osd_new(VideoOutput *vo, OSDOverlayRemovedCallback callback, gpointer userdata);
void osd_cleanup(OSD *osd);

guint32 osd_add_text(OSD *osd, const gchar *text, OSDTextAlignFlags align, guint timeout);
void osd_remove_text(OSD *osd, guint32 id);
void osd_begin_transaction(OSD *osd);
void osd_commit_transaction(OSD *osd);
