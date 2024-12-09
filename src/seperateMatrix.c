#include "seperateMatrix.h"

int seperateMatrix(uint8_t y_b[4][BLOCKSIZE * BLOCKSIZE], uint8_t y_macro[256]) {
    for(int i = 0; i < BLOCKSIZE; i++) {
        for(int j = 0; j < BLOCKSIZE; j++) {
            y_b[0][i * BLOCKSIZE + j] = y_macro[i * 2 * BLOCKSIZE + j];
            y_b[1][i * BLOCKSIZE + j] = y_macro[i * 2 * BLOCKSIZE + BLOCKSIZE + j];
            y_b[2][i * BLOCKSIZE + j] = y_macro[BLOCKSIZE * 2 * BLOCKSIZE + i * 2 * BLOCKSIZE + j];
            y_b[3][i * BLOCKSIZE + j] = y_macro[BLOCKSIZE * 2 * BLOCKSIZE + i * 2 * BLOCKSIZE + BLOCKSIZE + j];
        }
    }
    return 0;
}