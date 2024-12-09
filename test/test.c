#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <mpeg1_encoder.h>
#include "readImage.h"

#define INPUTFILENAME "../../data/Video1/Images/image0005.jpeg"
#define FILENAME_OUTPUT "output.mpeg"
#define BLOCK_SIZE 8
#define PI 3.1415927
#define FRAMERATE 10
#define SCALE_QUANT 1

const unsigned char quantization_table_y[BLOCK_SIZE * BLOCK_SIZE] = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

const unsigned char quantization_table_c[BLOCK_SIZE * BLOCK_SIZE] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 13, 21, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

void performFastDCT(double block[BLOCK_SIZE*BLOCK_SIZE]) {
    double temp[BLOCK_SIZE*BLOCK_SIZE];
    double cu, cv;

    // 1D DCT on rows
    for(int u = 0; u < BLOCK_SIZE; u++) {
        for(int x = 0; x < BLOCK_SIZE; x++) {
            temp[u*BLOCK_SIZE+x] = 0;
            cu = (u == 0) ? 1.0 / sqrt(2.0) : 1.0;
            for(int i = 0; i < BLOCK_SIZE; i++) {
                temp[u*BLOCK_SIZE+x] += block[u*BLOCK_SIZE+i] * cos((2.0 * i + 1.0) * x * PI / (2.0 * BLOCK_SIZE));
            }
            temp[u*BLOCK_SIZE+x] *= 0.5 * cu;
        }
    }

    // 1D DCT on columns
    for(int v = 0; v < BLOCK_SIZE; v++) {
        for(int y = 0; y < BLOCK_SIZE; y++) {
            block[y*BLOCK_SIZE+v] = 0;
            cv = (v == 0) ? 1.0 / sqrt(2.0) : 1.0;
            for(int j = 0; j < BLOCK_SIZE; j++) {
                block[y*BLOCK_SIZE+v] += temp[j*BLOCK_SIZE+v] * cos((2.0 * j + 1.0) * y * PI / (2.0 * BLOCK_SIZE));
            }
            block[y*BLOCK_SIZE+v] *= 0.5 * cv;
        }
    }
}

// Function to apply 2D DCT on an 8x8 block
void performDCT(double block[BLOCK_SIZE][BLOCK_SIZE]) {
    double temp[BLOCK_SIZE][BLOCK_SIZE]; // Temporary array to store DCT results
    double cu, cv, sum;

    // Perform the 2D DCT
    for(int u = 0; u < BLOCK_SIZE; u++) {
        for(int v = 0; v < BLOCK_SIZE; v++) {
            sum = 0.0; // Initialize sum for each (u, v)
            
            // Calculate normalization factors
            cu = (u == 0) ? 1.0 / sqrt(2.0) : 1.0;
            cv = (v == 0) ? 1.0 / sqrt(2.0) : 1.0;

            // Perform the summation over x and y
            for(int x = 0; x < BLOCK_SIZE; x++) {
                for(int y = 0; y < BLOCK_SIZE; y++) {
                    sum += block[x][y] * 
                        cos((2.0 * x + 1.0) * u * PI / (2.0 * BLOCK_SIZE)) *
                        cos((2.0 * y + 1.0) * v * PI / (2.0 * BLOCK_SIZE));
                }
            }
            // Scale the result and store it in the temp array
            temp[u][v] = 0.25 * cu * cv * sum;
        }
    }

    // Copy the result back into the original block
    for (int u = 0; u < BLOCK_SIZE; u++) {
        for (int v = 0; v < BLOCK_SIZE; v++) {
            block[u][v] = temp[u][v];
        }
    }
}

// scale from 0 to 51
void quantizeBlock(double block[BLOCK_SIZE*BLOCK_SIZE], const unsigned char* quantization_table, uint8_t scale) {
    for(int i = 0; i < BLOCK_SIZE; i++) {
        for(int j = 0; j < BLOCK_SIZE; j++) {
            block[i*BLOCK_SIZE+j] = round(block[i*BLOCK_SIZE+j] / (scale * quantization_table[i * BLOCK_SIZE + j]));
        }
    }
}

void doIntraframeCompression(char* filename_o, char* filename_i) {
    // decompress the image and get the information
    ImageInfo imageinfo;
    readImage(&imageinfo, filename_i);

    FILE* file_mlv = createMLV(filename_o, imageinfo);

    // writePackHeader(file_mpeg, width, height, 10, (uint32_t)(bit_rate/400));
    

    // Establish the dynamic display
    int i_CurrentBlock = 0;
    printf("Processing blocks: 0/%d", 10000);
    fflush(stdout); // Ensure the output is flushed to the terminal

    // Process each 8x8 block in Y, Cb, and Cr
    double prev_dc_coeffi_y = 0.0;
    double prev_dc_coeffi_cb = 0.0;
    double prev_dc_coeffi_cr = 0.0;
    uint8_t* bitstream_buffer = (uint8_t*)malloc(MAX_BITSTREAM_SIZE * 3 * sizeof(uint8_t));
    int bitstream_index = 0;
    size_t length_buffer = 0;
    //#pragma omp parallel for collapse(2) // Parallelize two nested loops
    for(int y_block = 0; y_block < imageinfo.height / BLOCK_SIZE; y_block++) { //height / BLOCK_SIZE
        writeSliceHeader(file_mlv, y_block, SCALE_QUANT);
        for(int x_block = 0; x_block < imageinfo.width / BLOCK_SIZE; x_block++) { // width / BLOCK_SIZE
            i_CurrentBlock++;
            // Dynamic progress update
            printf("\rProcessing blocks: %d/%d", i_CurrentBlock, 10000);
            fflush(stdout);

            // Use 1D arrays instead of 2D arrays
            double ym[4][BLOCK_SIZE * BLOCK_SIZE];  // Y matrix for this block
            double cbm[BLOCK_SIZE * BLOCK_SIZE]; // Cb matrix for this block
            double crm[BLOCK_SIZE * BLOCK_SIZE]; // Cr matrix for this block

            // Copy Y, Cb, Cr values into 1D arrays (this assumes YCbCr format)
            for(int y = 0; y < BLOCK_SIZE; y++) {
                for(int x = 0; x < BLOCK_SIZE; x++) {
                    ym[0][y * BLOCK_SIZE + x] = imageinfo.buf_p[ 
                        (y_block * BLOCK_SIZE * 2 + y) * imageinfo.width + 
                        (x_block * BLOCK_SIZE * 2 + x)
                    ];

                    ym[1][y * BLOCK_SIZE + x] = imageinfo.buf_p[ 
                        (y_block * BLOCK_SIZE * 2 + y) * imageinfo.width + 
                        (x_block * BLOCK_SIZE * 2 + BLOCK_SIZE + x)
                    ];

                    ym[2][y * BLOCK_SIZE + x] = imageinfo.buf_p[ 
                        (y_block * BLOCK_SIZE * 2 + BLOCK_SIZE + y) * imageinfo.width + 
                        (x_block * BLOCK_SIZE * 2 + x)
                    ];

                    ym[3][y * BLOCK_SIZE + x] = imageinfo.buf_p[ 
                        (y_block * BLOCK_SIZE * 2 + BLOCK_SIZE + y) * imageinfo.width + 
                        (x_block * BLOCK_SIZE * 2 + BLOCK_SIZE + x)
                    ];

                    cbm[y * BLOCK_SIZE + x] = imageinfo.buf_p[ 
                        imageinfo.width * imageinfo.height + 
                        (y_block * BLOCK_SIZE + y) * imageinfo.width + 
                        (x_block * BLOCK_SIZE + x)
                    ];

                    crm[y * BLOCK_SIZE + x] = imageinfo.buf_p[ 
                        imageinfo.width * imageinfo.height * 5 / 4 + 
                        (y_block * BLOCK_SIZE + y) * imageinfo.width + 
                        (x_block * BLOCK_SIZE + x)
                    ];
                }
            }

            // Apply DCT, quantization and Huffman encoding to the blocks
            for(int i = 0; i < 4; i++) {
                performFastDCT(ym[i]);
                quantizeBlock(ym[i], quantization_table_y, SCALE_QUANT);
                bitstream_index = encode_mpeg1_y(bitstream_buffer, (uint8_t*)ym[i], (int)prev_dc_coeffi_y);
                fwrite(bitstream_buffer, sizeof(uint8_t), bitstream_index, file_mlv);
                prev_dc_coeffi_y = ym[i][0]; // First element (DC coefficient for Y)
            }
            performFastDCT(cbm);
            quantizeBlock(cbm, quantization_table_c, SCALE_QUANT);
            bitstream_index = encode_mpeg1_c(bitstream_buffer, (uint8_t*)cbm, (int)prev_dc_coeffi_cb);
            fwrite(bitstream_buffer, sizeof(uint8_t), bitstream_index, file_mlv);
            performFastDCT(crm);
            quantizeBlock(crm, quantization_table_c, SCALE_QUANT);
            bitstream_index = encode_mpeg1_c(bitstream_buffer, (uint8_t*)crm, (int)prev_dc_coeffi_cr);
            fwrite(bitstream_buffer, sizeof(uint8_t), bitstream_index, file_mlv);
            prev_dc_coeffi_cb = cbm[0]; // First element (DC coefficient for Cb)
            prev_dc_coeffi_cr = crm[0]; // First element (DC coefficient for Cr)
        }
    }
    fclose(file_mlv);
}

int main() {
    struct timeval start, end;
    
    gettimeofday(&start, NULL);

    doIntraframeCompression(FILENAME_OUTPUT, INPUTFILENAME);

    gettimeofday(&end, NULL);
    
    double total_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    printf("Total execution time: %.2f seconds\n", total_time);

    return 0;
}
