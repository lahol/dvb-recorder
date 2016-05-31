#include "osd.h"
#include <cairo.h>
#include <dvbrecorder/streaminfo.h>
#include <stdio.h>

struct _OSD {
    DVBRecorder *recorder;
    VideoOutput *vo;

    guint hide_osd_source;
    guint timeout;
    GTimer *hide_osd_timer;
};

OSD *osd_new(DVBRecorder *recorder, VideoOutput *vo)
{
    OSD *osd = g_malloc0(sizeof(OSD));

    osd->recorder = recorder;
    osd->vo = vo;

    return osd;
}

void osd_cleanup(OSD *osd)
{
    if (!osd)
        return;
    if (osd->hide_osd_timer)
        g_timer_destroy(osd->hide_osd_timer);
    if (osd->hide_osd_source)
        g_source_remove(osd->hide_osd_source);
    g_free(osd);
}

gboolean osd_check_timer_elapsed(OSD *osd)
{
    gdouble elapsed = g_timer_elapsed(osd->hide_osd_timer, NULL);
    if (osd->timeout && elapsed > osd->timeout) {
        g_timer_stop(osd->hide_osd_timer);
        g_timer_reset(osd->hide_osd_timer);
        osd->hide_osd_source = 0;
        video_output_set_overlay_surface(osd->vo, NULL);
        return FALSE;
    }

    return TRUE;
}

void osd_update_channel_display(OSD *osd, guint timeout)
{
    fprintf(stderr, "osd_update_channel_display, osd: %p\n", osd);
    g_return_if_fail(osd != NULL);

    DVBStreamInfo *info = dvb_recorder_get_stream_info(osd->recorder);
    fprintf(stderr, "stream info: %p\n", info);
    if (!info)
        return;

    gint width, height;
    double pixelaspect;

    if (!video_output_get_overlay_surface_parameters(osd->vo, &width, &height, &pixelaspect)) {
        fprintf(stderr, "could not get overlay surface parameters\n");
        return;
    }
    fprintf(stderr, "surface parameters: %d, %d, %f\n", width, height, pixelaspect);

    gint scaledwidth = width * pixelaspect;

    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if (!surf)
        return;

    cairo_t *cr = cairo_create(surf);
    if (!cr)
        goto err;

    cairo_scale(cr, 1.0/pixelaspect, 1.0);

    /* FIXME: use pango, make box around osd */

    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 32);

    cairo_move_to(cr, 10, 42);
    cairo_text_path(cr, info->service_name);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_fill_preserve(cr);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_destroy(cr);

    dvb_stream_info_free(info);

    video_output_set_overlay_surface(osd->vo, surf);

    osd->timeout = timeout;
    if (!osd->hide_osd_timer)
        osd->hide_osd_timer = g_timer_new();
    g_timer_start(osd->hide_osd_timer);
    if (!osd->hide_osd_source)
        osd->hide_osd_source = g_idle_add((GSourceFunc)osd_check_timer_elapsed, osd);
    
    return;

err:
    if (cr)
        cairo_destroy(cr);
    if (surf)
        cairo_surface_destroy(surf);
    dvb_stream_info_free(info);
}
