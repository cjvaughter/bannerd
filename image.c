#include <string.h>

#include "bmp.h"
#include "image.h"
#include "log.h"
#include "png.h"

int image_read(const char *filename, struct image_info *bitmap)
{
    if (strstr(filename, ".bmp") || strstr(filename, ".BMP")) {
        return bmp_read(filename, bitmap);
    }

#ifndef NO_PNG
    if (strstr(filename, ".png") || strstr(filename, ".PNG")) {
        return png_read(filename, bitmap);
    }
#endif

    ERR_RET(-1, "Unknown filetype: %s\n", filename);
}

