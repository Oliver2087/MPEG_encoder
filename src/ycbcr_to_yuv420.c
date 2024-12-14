#include "ycbcr_to_yuv420.h"
#include <stdio.h> // Optional, for error handling or debugging

void convertYCbCrToYUV420(unsigned char *input, 
                          unsigned char **Y, unsigned char **U, unsigned char **V, 
                          int width, int height) {
    // Allocate memory for the YUV 4:2:0 components
    *Y = (unsigned char*)malloc(width * height);
    if (*Y == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for Y component.\n");
        exit(EXIT_FAILURE);
    }

    *U = (unsigned char*)malloc(width / 2 * height / 2);
    if (*U == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for U component.\n");
        free(*Y);
        exit(EXIT_FAILURE);
    }

    *V = (unsigned char*)malloc(width / 2 * height / 2);
    if (*V == NULL) {
        fprintf(stderr, "Error: Failed to allocate memory for V component.\n");
        free(*Y);
        free(*U);
        exit(EXIT_FAILURE);
    }

    // Perform the conversion
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            int idx = (j * width + i) * 3; // 3 components per pixel: Y, Cb, Cr
            unsigned char yVal = input[idx];
            unsigned char cbVal = input[idx + 1];
            unsigned char crVal = input[idx + 2];
            
            // Store Y component
            (*Y)[j * width + i] = yVal;
            
            // Downsample Cb/Cr for 4:2:0 (simple top-left sampling)
            if ((j % 2 == 0) && (i % 2 == 0)) {
                (*U)[(j / 2) * (width / 2) + (i / 2)] = cbVal;
                (*V)[(j / 2) * (width / 2) + (i / 2)] = crVal;
            }
        }
    }
}