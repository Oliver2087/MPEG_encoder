#include "quantization.h"

const unsigned char quantization_table_y[BLOCKSIZE * BLOCKSIZE] = {
    8, 16, 19, 22, 26, 27, 29, 34,
    16, 16, 22, 24, 27, 29, 34, 37,
    19, 22, 26, 27, 29, 34, 34, 38,
    22, 22, 26, 27, 29, 34, 37, 40,
    22, 26, 27, 29, 32, 35, 40, 48,
    26, 27, 29, 32, 35, 40, 48, 58,
    26, 27, 29, 34, 38, 46, 56, 69,
    27, 29, 35, 38, 46, 56, 69, 83
};
/*{
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};*/

const unsigned char quantization_table_c[BLOCKSIZE * BLOCKSIZE] = {
    16, 17, 18, 19, 20, 21, 22, 23,
    17, 18, 19, 20, 21, 22, 23, 24,
    18, 19, 20, 21, 22, 23, 24, 25,
    19, 20, 21, 22, 23, 24, 25, 26,
    20, 21, 22, 23, 24, 25, 26, 27,
    21, 22, 23, 24, 25, 26, 27, 28,
    22, 23, 24, 25, 26, 27, 28, 29,
    23, 24, 25, 26, 27, 28, 29, 30
};
/*{
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 13, 21, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};*/

// 预计算常量 (AAN 优化系数)
const float C1 = 0.49039;  // cos(pi/16)
const float C2 = 0.46194;  // cos(2pi/16)
const float C3 = 0.41573;  // cos(3pi/16)
const float C4 = 0.35355;  // cos(4pi/16)
const float C5 = 0.27779;  // cos(5pi/16)
const float C6 = 0.19134;  // cos(6pi/16)
const float C7 = 0.09755;  // cos(7pi/16)

// 一维 Fast DCT
void fastDCT8(float *data) {
    float tmp[8];

    float s07 = data[0] + data[7];
    float d07 = data[0] - data[7];
    float s16 = data[1] + data[6];
    float d16 = data[1] - data[6];
    float s25 = data[2] + data[5];
    float d25 = data[2] - data[5];
    float s34 = data[3] + data[4];
    float d34 = data[3] - data[4];

    // Even terms
    float s0734 = s07 + s34;
    float d0734 = s07 - s34;
    float s1625 = s16 + s25;
    float d1625 = s16 - s25;

    tmp[0] = C4 * (s0734 + s1625);
    tmp[4] = C4 * (s0734 - s1625);

    tmp[2] = C2 * d0734 + C6 * d1625;
    tmp[6] = C6 * d0734 - C2 * d1625;

    // Odd terms
    tmp[1] = C1 * d07 + C3 * d16 + C5 * d25 + C7 * d34;
    tmp[3] = C3 * d07 - C7 * d16 - C1 * d25 - C5 * d34;
    tmp[5] = C5 * d07 - C1 * d16 + C7 * d25 + C3 * d34;
    tmp[7] = C7 * d07 - C5 * d16 + C3 * d25 - C1 * d34;

    for (int i = 0; i < 8; i++) {
        data[i] = tmp[i];
    }
}

// 二维 Fast DCT
void performFastDCT(double block[64]) {
    float tmp[8];

    // 行变换
    for(int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            tmp[j] = block[i * 8 + j];
        }
        fastDCT8(tmp);
        for (int j = 0; j < 8; j++) {
            block[i * 8 + j] = tmp[j];
        }
    }

    // 列变换
    for (int j = 0; j < 8; j++) {
        for (int i = 0; i < 8; i++) {
            tmp[i] = block[i * 8 + j];
        }
        fastDCT8(tmp);
        for (int i = 0; i < 8; i++) {
            block[i * 8 + j] = tmp[i];
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
void quantizeBlock(int mat[BLOCKSIZE * BLOCKSIZE], uint8_t block[BLOCKSIZE*BLOCKSIZE], const unsigned char* quantization_table, uint8_t scale) {
    double tmp[BLOCKSIZE][BLOCKSIZE];
    for(int i = 0; i < BLOCKSIZE; i++) {
        for(int j = 0; j < BLOCKSIZE; j++) {
            tmp[i][j] = block[i * 8 + j];
        }
    }
    performdct2d(tmp);
    for(int i = 0; i < BLOCKSIZE; i++) {
        for(int j = 0; j < BLOCKSIZE; j++) {
            mat[i*BLOCKSIZE+j] = round(tmp[i][j] * 10 / (scale * quantization_table[i * BLOCKSIZE + j]));
        }
    }
    return;
}