#ifndef ENCODER_H
#define ENCODER_H
#include "processFrame.h"

void writeSequenceHeader(FILE* output, int width, int height, int frame_rate_code, int bitrate, int vbv_buffer_size);
void encodeGOP(FILE* output, const char* gop_file, int width, int height, int frame_rate_code);

#endif