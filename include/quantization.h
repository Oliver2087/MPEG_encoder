#ifndef QUANTIZATION_H
#define QUANTIZATION_H

#include <math.h>

#include "seperateMatrix.h"
#include "ffwt.h"

#define PI 3.1415927

extern const unsigned char quantization_table_y[BLOCKSIZE * BLOCKSIZE];

extern const unsigned char quantization_table_c[BLOCKSIZE * BLOCKSIZE];

void performFastDCT(double block[BLOCKSIZE*BLOCKSIZE]);

// Function to apply 2D DCT on an 8x8 block
void performDCT(double block[BLOCKSIZE][BLOCKSIZE]);

// scale from 0 to 51
void quantizeBlock(int mat[BLOCKSIZE * BLOCKSIZE], uint8_t block[BLOCKSIZE*BLOCKSIZE], const unsigned char* quantization_table, uint8_t scale);
#endif