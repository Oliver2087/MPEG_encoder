#ifndef YCBCR_TO_YUV420_H
#define YCBCR_TO_YUV420_H

#include <stdlib.h>
#include <stdint.h>

// Function to convert interleaved YCbCr data to planar YUV 4:2:0
void convertYCbCrToYUV420(unsigned char *input, 
                          unsigned char **Y, unsigned char **U, unsigned char **V, 
                          int width, int height);

#endif // YCBCR_TO_YUV420_H