#ifndef CREATEMLV_H
#define CREATEMLV_H

#include <stdio.h>
#include <stdint.h>

#include "readImage.h"

#define LEN_SYS_HEADER 9

// Write the sequence header of the mpeg file
// bit_rate: choose one of the following:
//     1: 23.976 fps
//     2: 24 fps
//     3: 25 fps
//     4: 29.97 fps
//     5: 30 fps
//     6: 50 fps
//     7: 59.94 fps
//     8: 60 fps
// n is the index of the image
void writePackHeader(FILE* file_mpeg, int width, int height, uint16_t frame_rate, int bit_rate);

// Optional. Including ratebound. Not used now
void writeSystemHeader(FILE* file_mpeg, uint16_t len_header, int bitrat);

// the unit of buffer size is 128
void writePacket(FILE* file_mpeg, uint16_t size_buffer, int bitrate, uint16_t fps);

void writeSequenceHeader(FILE* file_mpeg, uint16_t width, uint16_t height);

void writeGOPHeader(FILE* file_mlv);

void writePictureHeader(FILE* file_mlv);

// Parameter:
// index: the vertical position of slice (only 4 bit is allowed)
// scal: quatization scale 0-51
void writeSliceHeader(FILE* file_mlv, uint8_t index, uint8_t scal);

FILE* createMLV(char* filename_p, ImageInfo imageinfo);
#endif