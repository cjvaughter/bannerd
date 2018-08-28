#ifndef PNG_H
#define PNG_H

struct image_info;

int png_read(const char *filename, struct image_info *bitmap);

#endif /* PNG_H */
