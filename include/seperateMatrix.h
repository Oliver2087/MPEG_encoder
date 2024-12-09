#ifndef SEPERATEMATRIX_H
#define SEPERATEMATRIX_H
#include <stdint.h>

#define BLOCKSIZE 8

int seperateMatrix(uint8_t y_b[4][BLOCKSIZE * BLOCKSIZE], uint8_t y_macro[256]);
#endif