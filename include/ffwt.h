#ifndef FFWT_H
#define FFWT_H
#include <fftw3.h>
#include <stdio.h>

#define N 8  // 8x8 DCT size

// Function to perform 2D DCT-II on an 8x8 matrix
void performdct2d(double mat[8][8]);
#endif