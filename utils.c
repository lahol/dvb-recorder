#include "utils.h"
#include <png.h>
#include <gst/gst.h>

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


