#ifndef READIMAGE_H
#define READIMAGE_H
#include <stdio.h>
#include <stdlib.h>

#include <jpeglib.h>

#define NUMOFLINESREADINONETIME 16

typedef struct ImageInfo {
    unsigned char* buf_p;
    int width;
    int height;
    size_t buf_size;
} ImageInfo;

int readImage(ImageInfo* imageinfo, char* filename);
#endif