#ifndef READIMAGE_H
#define READIMAGE_H
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <jpeglib.h>

#define NUMOFLINESREADINONETIME 16

typedef struct ImageInfo {
    unsigned char* buf_p;
    int width;
    int height;
    size_t buf_size;
    uint16_t fps;
    int bitrate; // kbps
} ImageInfo;

int readImage(ImageInfo* imageinfo, char* filename);
#endif