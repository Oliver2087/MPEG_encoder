#include "quantization.h"

const unsigned char quantization_table_y[BLOCKSIZE * BLOCKSIZE] = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

const unsigned char quantization_table_c[BLOCKSIZE * BLOCKSIZE] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 13, 21, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

void performFastDCT(double block[BLOCKSIZE*BLOCKSIZE]) {
    double temp[BLOCKSIZE*BLOCKSIZE];
    double cu, cv;

    // 1D DCT on rows
    for(int u = 0; u < BLOCKSIZE; u++) {
        for(int x = 0; x < BLOCKSIZE; x++) {
            temp[u*BLOCKSIZE+x] = 0;
            cu = (u == 0) ? 1.0 / sqrt(2.0) : 1.0;
            for(int i = 0; i < BLOCKSIZE; i++) {
                temp[u*BLOCKSIZE+x] += block[u*BLOCKSIZE+i] * cos((2.0 * i + 1.0) * x * PI / (2.0 * BLOCKSIZE));
            }
            temp[u*BLOCKSIZE+x] *= 0.5 * cu;
        }
    }

    // 1D DCT on columns
    for(int v = 0; v < BLOCKSIZE; v++) {
        for(int y = 0; y < BLOCKSIZE; y++) {
            block[y*BLOCKSIZE+v] = 0;
            cv = (v == 0) ? 1.0 / sqrt(2.0) : 1.0;
            for(int j = 0; j < BLOCKSIZE; j++) {
                block[y*BLOCKSIZE+v] += temp[j*BLOCKSIZE+v] * cos((2.0 * j + 1.0) * y * PI / (2.0 * BLOCKSIZE));
            }
            block[y*BLOCKSIZE+v] *= 0.5 * cv;
        }
    }
}

// Function to apply 2D DCT on an 8x8 block
void performDCT(double block[BLOCKSIZE][BLOCKSIZE]) {
    double temp[BLOCKSIZE][BLOCKSIZE]; // Temporary array to store DCT results
    double cu, cv, sum;

    // Perform the 2D DCT
    for(int u = 0; u < BLOCKSIZE; u++) {
        for(int v = 0; v < BLOCKSIZE; v++) {
            sum = 0.0; // Initialize sum for each (u, v)
            
            // Calculate normalization factors
            cu = (u == 0) ? 1.0 / sqrt(2.0) : 1.0;
            cv = (v == 0) ? 1.0 / sqrt(2.0) : 1.0;

            // Perform the summation over x and y
            for(int x = 0; x < BLOCKSIZE; x++) {
                for(int y = 0; y < BLOCKSIZE; y++) {
                    sum += block[x][y] * 
                        cos((2.0 * x + 1.0) * u * PI / (2.0 * BLOCKSIZE)) *
                        cos((2.0 * y + 1.0) * v * PI / (2.0 * BLOCKSIZE));
                }
            }
            // Scale the result and store it in the temp array
            temp[u][v] = 0.25 * cu * cv * sum;
        }
    }

    // Copy the result back into the original block
    for (int u = 0; u < BLOCKSIZE; u++) {
        for (int v = 0; v < BLOCKSIZE; v++) {
            block[u][v] = temp[u][v];
        }
    }
}

// scale from 0 to 51
void quantizeBlock(uint8_t block[BLOCKSIZE*BLOCKSIZE], const unsigned char* quantization_table, uint8_t scale) {
    double tmp[BLOCKSIZE * BLOCKSIZE];
    for(int i = 0; i < BLOCKSIZE * BLOCKSIZE; i++) {
        tmp[i] = block[i];
    }
    performFastDCT(tmp);
    for(int i = 0; i < BLOCKSIZE; i++) {
        for(int j = 0; j < BLOCKSIZE; j++) {
            block[i*BLOCKSIZE+j] = round(tmp[i*BLOCKSIZE+j] / (scale * quantization_table[i * BLOCKSIZE + j]));
        }
    }
}