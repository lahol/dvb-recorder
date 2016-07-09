#include "utils.h"
#include <png.h>
#include <gst/gst.h>
#include <stdio.h>

gboolean util_write_data_to_png(const gchar *filename, guint8 *buffer, guint width, guint height, guint bpp)
{
    png_bytep *rows = NULL;
    png_structp struct_ptr = NULL;
    png_infop info_ptr = NULL;

    FILE *out = NULL;
    gboolean result = FALSE;
    
    if ((out = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "Could not open file %s.\n", filename);
        goto done;
    }

    struct_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp)0, NULL, NULL);
    if (!struct_ptr)
        goto done;

    info_ptr = png_create_info_struct(struct_ptr);
    if (!info_ptr)
        goto done;

    rows = png_malloc(struct_ptr, height * sizeof(png_bytep));
    guint i;
    for (i = 0; i < height; ++i)
        rows[i] = buffer + i * width * (bpp/8);

    if (setjmp(png_jmpbuf(struct_ptr)))
        goto done;

    png_set_filter(struct_ptr, 0, PNG_FILTER_NONE | PNG_FILTER_VALUE_NONE);
    png_init_io(struct_ptr, out);
/*    png_set_write_status_fn(struct_ptr, write_row_callback);*/
    png_set_IHDR(struct_ptr, info_ptr, width, height,
            8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

/*    png_set_rows(struct_ptr, info_ptr, rows);
    png_write_png(struct_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);*/
    png_write_info(struct_ptr, info_ptr);
#if GST_CHECK_VERSION(1, 0, 0)
#else
    png_set_filler(struct_ptr, 0, PNG_FILLER_AFTER);
#endif

    png_write_image(struct_ptr, rows);
    png_write_end(struct_ptr, NULL);

    result = TRUE;

done:
    if (rows)
        png_free(struct_ptr, rows);
    if (info_ptr)
        png_destroy_info_struct(struct_ptr, &info_ptr);
    if (struct_ptr)
        png_destroy_write_struct(&struct_ptr, (png_infopp)NULL);
    if (out)
        fclose(out);
    return result;
}

/* in utf8-string with control codes : 0xc286, 0xc287, 0xc28a -> <i>, </i>, \n
 * control codes may be in the range 0xc280 to 0xc29f
 * returns newly allocated string */
gchar *util_translate_control_codes_to_markup(gchar *string)
{
    if (!string)
        return NULL;
    gchar *out = NULL;

    gssize j = 0;
    gssize k;
    gssize delta = 0;
    gssize length = 0;

    while (string[j]) {
        if ((guchar)string[j] == 0xc2) {
            if ((guchar)string[j+1] == 0x86)
                delta += 1;         /* c286 -> <i> */
            else if ((guchar)string[j+1] == 0x87)
                delta += 2;         /* c287 -> </i> */
            else if ((guchar)string[j+1] == 0x8a)
                delta -= 1;         /* c28a -> \n */
        }
        ++j;
    }

    length = j;

    out = g_malloc(sizeof(gchar)*(length + delta + 1));
    k = 0;
    j = 0;
    while (j <= length) {
        if ((guchar)string[j] == 0xc2) {
            if ((guchar)string[j+1] == 0x86) {
                out[k] = '<';
                out[k+1] = 'i';
                out[k+2] = '>';
                k += 3;
                j += 2;
            }
            else if ((guchar)string[j+1] == 0x87) {
                out[k] = '<';
                out[k+1] = '/';
                out[k+2] = 'i';
                out[k+3] = '>';
                k += 4;
                j += 2;
            }
            else if ((guchar)string[j+1] == 0x8a) {
                out[k] = '\n';
                ++k;
                j += 2;
            }
            else {
                out[k] = string[j];
                ++k;
                ++j;
            }
        }
        else {
            out[k] = string[j];
            ++k;
            ++j;
        }
    }

    return out;
}

void util_time_to_string(gchar *buffer, gsize buflen, time_t starttime, gboolean short_format)
{
    struct tm *tm = localtime(&starttime);

    if (short_format)
        strftime(buffer, buflen, "%a, %d.%m. %R", tm);
    else
        strftime(buffer, buflen, "%a, %x %R", tm);
}

void util_duration_to_string(gchar *buffer, gsize buflen, guint32 seconds)
{
    guint32 d, h, m, s, t;
    d = seconds / (24*3600);
    t = seconds % (24*3600);
    h = t / 3600;
    t = t % 3600;
    m = t / 60;
    s = t % 60;

    if (d == 0) {
        if (h == 0) {
            if (s == 0)
                snprintf(buffer, buflen, "%um", m);
            else
                snprintf(buffer, buflen, "%um %02us", m, s);
        }
        else {
            if (s == 0)
                snprintf(buffer, buflen, "%uh %02um", h, m);
            else
                snprintf(buffer, buflen, "%uh %02um %02us", h, m, s);
        }
    }
    else {
        if (s == 0) {
            if (m == 0)
                snprintf(buffer, buflen, "%ud %02uh", d, h);
            else
                snprintf(buffer, buflen, "%ud %02uh %02um", d, h, m);
        }
        else {
            snprintf(buffer, buflen, "%ud %02uh %02um %02us", d, h, m, s);
        }
    }
}

void util_duration_to_string_iso(gchar *buffer, gsize buflen, guint32 seconds)
{
    guint32 h, m, s, t;
    h = seconds / 3600;
    t = seconds % 3600;
    m = t / 60;
    s = t % 60;

    snprintf(buffer, buflen, "%02u:%02u:%02u", h, m, s);
}

