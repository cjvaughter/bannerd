#ifndef IMAGE_H
#define IMAGE_H

struct image_info {
    int width;
    int height;
    unsigned long *pixel_buffer;
};

int image_read(const char *filename, struct image_info *bitmap);

#endif /* IMAGE_H */
