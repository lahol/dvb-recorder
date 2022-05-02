#include "osd.h"
#include <cairo.h>
#include <pango/pangocairo.h>
#include <stdio.h>
#include "config.h"
#include <gdk/gdk.h>
#include <math.h>

struct OSDEntry {
    guint32 id;

    gchar *text;
    OSDTextAlignFlags align;
    GTimer *timer;
    guint timeout;

    guint added_in_transaction : 1;
};

struct _OSD {
    VideoOutput *vo;

    OSDOverlayRemovedCallback callback;
    gpointer userdata;

    guint hide_osd_source;
    gint width;
    gint height;

    GList *entries;

    guint in_transaction : 1;
    guint force_update : 1;
    guint hidden : 1;

    guint64 last_osd_id;
};

void osd_update_overlay(OSD *osd);

void osd_entry_free(struct OSDEntry *entry)
{
    if (entry) {
        g_free(entry->text);
        g_timer_destroy(entry->timer);
        g_free(entry);
    }
}

OSD *osd_new(VideoOutput *vo, OSDOverlayRemovedCallback callback, gpointer userdata)
{
    OSD *osd = g_malloc0(sizeof(OSD));

    osd->vo = vo;

    return osd;
}

void osd_cleanup(OSD *osd)
{
    if (!osd)
        return;
    g_list_free_full(osd->entries, (GDestroyNotify)osd_entry_free);
    if (osd->hide_osd_source)
        g_source_remove(osd->hide_osd_source);
    g_free(osd);
}

gboolean osd_check_timer_elapsed(OSD *osd)
{
    g_return_val_if_fail(osd != NULL, FALSE);

    GList *tmp, *next;
    struct OSDEntry *entry;
    guint valid_elements = 0;
    tmp = osd->entries;
    gdouble elapsed;
    gboolean needs_update = FALSE;
    while (tmp) {
        next = tmp->next;
        entry = (struct OSDEntry *)tmp->data;

        elapsed = g_timer_elapsed(entry->timer, NULL);
        if (entry->timeout && elapsed > entry->timeout) {
            osd_entry_free(entry);
            osd->entries = g_list_delete_link(osd->entries, tmp);
            needs_update = TRUE;
        }
        else {
            ++valid_elements;
        }

        tmp = next;
    }

    if (needs_update)
        osd_update_overlay(osd);

    if (!valid_elements)
        osd->hide_osd_source = 0;

    return (valid_elements > 0);
}

void osd_render_text(OSD *osd, cairo_t *cr, PangoFontDescription *fdesc, GdkRGBA *color, gboolean dark,
                     const gchar *text, OSDTextAlignFlags align)
{
    int w, h;
    gdouble width, height;
    PangoLayout *layout = pango_cairo_create_layout(cr);
    pango_layout_set_font_description(layout, fdesc);
    pango_layout_set_text(layout, text, -1);
    pango_layout_get_size(layout, &w, &h);

    width = ((gdouble)w)/PANGO_SCALE;
    height = ((gdouble)h)/PANGO_SCALE;

    gdouble x = 0, y = 0;
    if (align & OSD_ALIGN_HORZ_LEFT)
        x = 10.0f;
    else if (align & OSD_ALIGN_HORZ_CENTER)
        x = (osd->width - width) * 0.5;
    else if (align & OSD_ALIGN_HORZ_RIGHT)
        x = osd->width - width - 10.0f;

    if (align & OSD_ALIGN_VERT_TOP)
        y = 10.0f;
    else if (align & OSD_ALIGN_VERT_CENTER)
        y = (osd->height - height) * 0.5;
    else if (align & OSD_ALIGN_VERT_BOTTOM)
        y = osd->height - height - 10.0f;

    cairo_save(cr);
    cairo_translate(cr, x, y);

    pango_cairo_update_layout(cr, layout);

    pango_cairo_layout_path(cr, layout);
    if (dark)
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    else
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
    cairo_set_line_width(cr, 2.0);
    cairo_stroke(cr);

    gdk_cairo_set_source_rgba(cr, color);
    pango_cairo_show_layout(cr, layout);

    cairo_restore(cr);

    g_object_unref(layout);
}

void osd_update_overlay(OSD *osd)
{
    if (osd->in_transaction)
        return;
    if (osd->hidden)
        return;

    if (!osd->entries) {
        video_output_set_overlay_surface(osd->vo, NULL);
        return;
    }

    gint width, height;
    double pixelaspect;
    if (!video_output_get_overlay_surface_parameters(osd->vo, &width, &height, &pixelaspect)) {
        fprintf(stderr, "could not get overlay surface parameters\n");
        osd->force_update = TRUE;
        return;
    }

    osd->width = width * pixelaspect;
    osd->height = height;

    cairo_t *cr = NULL;

    cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if (!surf) {
        goto err;
    }

    cr = cairo_create(surf);
    if (!cr)
        goto err;

    cairo_scale(cr, 1.0/pixelaspect, 1.0);

    GList *tmp;
    struct OSDEntry *entry;

    PangoFontDescription *fdesc = NULL;
    gchar *osd_font = NULL;
    if (config_get("main", "osd-font", CFG_TYPE_STRING, &osd_font) == 0) {
        fdesc = pango_font_description_from_string(osd_font);
        g_free(osd_font);
    }
    if (!fdesc) {
        fdesc = pango_font_description_from_string("Sans 32");
        if (!fdesc)
            goto err;
    }

    GdkRGBA font_color = { 1.0, 1.0, 1.0, 1.0 };
    gchar *osd_color = NULL;
    if (config_get("main", "osd-color", CFG_TYPE_STRING, &osd_color) == 0) {
        gdk_rgba_parse(&font_color, osd_color);
        g_free(osd_color);
    }

    gboolean dark = FALSE;
/*    if (sqrt(  0.299*font_color.red*font_color.red
             + 0.587*font_color.green*font_color.green
             + 0.114*font_color.blue*font_color.blue) < 0.5)*/
    if (0.299 * font_color.red + 0.587 * font_color.green + 0.114 * font_color.blue < 0.5)
        dark = TRUE;
    for (tmp = osd->entries; tmp; tmp = g_list_next(tmp)) {
        entry = (struct OSDEntry *)tmp->data;
        osd_render_text(osd, cr, fdesc, &font_color, dark, entry->text, entry->align);
    }

    pango_font_description_free(fdesc);
    cairo_destroy(cr);

    video_output_set_overlay_surface(osd->vo, surf);

    osd->force_update = FALSE;
    return;

err:
    osd->force_update = TRUE;
    if (cr)
        cairo_destroy(cr);
    if (surf)
        cairo_surface_destroy(surf);
}

void osd_run_timer(OSD *osd)
{
    if (osd->in_transaction)
        return;

    if (osd->hide_osd_source)
        return;

    osd->hide_osd_source = g_idle_add((GSourceFunc)osd_check_timer_elapsed, osd);
}

gint osd_compare_id_func(struct OSDEntry *a, guint id)
{
    if (a->id < id)
        return -1;
    if (a->id > id)
        return 1;
    return 0;
}

void osd_show_osd(OSD *osd, gboolean do_show)
{
    g_return_if_fail(osd != NULL);

    if (do_show) {
        osd->hidden = 0;
        osd_update_overlay(osd);
    }
    else {
        osd->hidden = 1;
        video_output_set_overlay_surface(osd->vo, NULL);
    }

}

guint32 osd_add_text(OSD *osd, const gchar *text, OSDTextAlignFlags align, guint timeout)
{
    g_return_val_if_fail(osd != NULL, 0);
    if (!text || text[0] == 0 || timeout == 0)
        return 0;

    struct OSDEntry *entry = g_malloc0(sizeof(struct OSDEntry));

    /* ensures that id is unique, but is this really necessary? */
    do {
        if (G_UNLIKELY(++osd->last_osd_id == 0))
            ++osd->last_osd_id;
    } while (g_list_find_custom(osd->entries, GUINT_TO_POINTER(osd->last_osd_id), (GCompareFunc)osd_compare_id_func));

    entry->id = osd->last_osd_id;
    entry->text = g_strdup(text);
    entry->align = align;
    entry->timeout = timeout;
    entry->timer = g_timer_new();
    entry->added_in_transaction = osd->in_transaction;

    osd->entries = g_list_prepend(osd->entries, entry);

    return entry->id;
}

void osd_remove_text(OSD *osd, guint32 id)
{
    g_return_if_fail(osd != NULL);

    GList *entry = g_list_find_custom(osd->entries, GUINT_TO_POINTER(id), (GCompareFunc)osd_compare_id_func);
    if (!entry)
        return;
    osd_entry_free((struct OSDEntry *)entry->data);
    osd->entries = g_list_delete_link(osd->entries, entry);

    osd_update_overlay(osd);

    if (osd->callback)
        osd->callback(id, osd->userdata);
}

void osd_begin_transaction(OSD *osd)
{
    g_return_if_fail(osd != NULL);

    osd->in_transaction = TRUE;
}

void osd_commit_transaction(OSD *osd)
{
    g_return_if_fail(osd != NULL);

    osd->in_transaction = FALSE;

    GList *tmp;
    for (tmp = osd->entries; tmp; tmp = g_list_next(tmp)) {
        if (((struct OSDEntry *)tmp->data)->added_in_transaction) {
            ((struct OSDEntry *)tmp->data)->added_in_transaction = FALSE;
            g_timer_reset(((struct OSDEntry *)tmp->data)->timer);
        }
    }

    osd_update_overlay(osd);
    osd_run_timer(osd);
}

