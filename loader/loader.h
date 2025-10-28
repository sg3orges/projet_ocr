#ifndef LOADER_H
#define LOADER_H

typedef struct {
    int width;
    int height;
    int channels;
    unsigned char *data;
} Image;

Image load_image(const char *filename);
void free_image(Image *img);
void to_black_and_white(Image *img);
void save_image(const char *filename, Image *img);
void deskew_image(Image *img);
void resize_image(Image *img, int width, int height);
void enhance_contrast_light(Image *img);
void enhance_contrast(Image *img);
void blur_image(Image *img);

#endif
