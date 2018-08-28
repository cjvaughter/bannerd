#include <errno.h>
#include <fcntl.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "png.h"
#include "fb.h"
#include "log.h"

#ifndef NO_PNG

int png_read(const char *filename, struct image_info *image)
{
    png_structp png_ptr;
    png_infop info_ptr;
    png_uint_32 width, height;
    png_bytepp row_pointers;
    FILE *fp;
    int image_size;
    int n;
    unsigned long *buf;

    if ((fp = fopen(filename, "rb")) == NULL) {
        ERR_RET(-1, "Could not open file %s", filename);
    }

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        ERR_RET(-1, "libpng: Error creating read struct");
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        ERR_RET(-1, "libpng: Error creating info struct");
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        ERR_RET(-1, "libpng: setjmp error");
    }

    png_init_io(png_ptr, fp);

    png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_BGR, NULL);

    width = image->width = png_get_image_width(png_ptr, info_ptr);
    height = image->height = png_get_image_height(png_ptr, info_ptr);
    image_size = width * height * 4;

    if(!(image->pixel_buffer = malloc(image_size))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        ERR_RET(-1, "Could not allocate %d bytes of memory for the image", image_size);
    }    
    
    row_pointers = png_get_rows(png_ptr, info_ptr);
    for (n = 0, buf = image->pixel_buffer; n < height; n++, buf += width) {
        memcpy(buf, row_pointers[n], width * 4);
    }

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    fclose(fp);

#if 1
    LOG(LOG_DEBUG, "Parsed PNG %s: %dx%d, %d bytes",
        filename, width, height, image_size);
#endif

    return 0;
}

#endif /* NO_PNG */

