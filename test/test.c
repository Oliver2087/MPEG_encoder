#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <mpeg1_encoder.h>
#include "readImage.h"
#include "createMLV.h"
#include "seperateMatrix.h"
#include "quantization.h"

#define INPUTFILENAME "../../data/Video1/output_images/image_001.jpg"
#define FILENAME_OUTPUT "output.mpeg"
#define BLOCK_SIZE 8
#define FRAMERATE 10
#define SCALE_QUANT 8

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
    int prev_dc_coeffi_y = 0;
    int prev_dc_coeffi_cb = 0;
    int prev_dc_coeffi_cr = 0;
    uint8_t* bitstream_buffer = (uint8_t*)malloc(MAX_BITSTREAM_SIZE * 3 * sizeof(uint8_t));
    int bitstream_index = 0;
    size_t length_buffer = 0;
    //#pragma omp parallel for collapse(2) // Parallelize two nested loops
    for(int y_block = 0; y_block < imageinfo.height / BLOCK_SIZE / 2; y_block++) { //height / BLOCK_SIZE
        writeSliceHeader(file_mlv, y_block + 1, SCALE_QUANT);
        for(int x_block = 0; x_block < imageinfo.width / BLOCK_SIZE / 2; x_block++) { // width / BLOCK_SIZE
            i_CurrentBlock++;
            // Dynamic progress update
            printf("\rProcessing blocks: %d/%d", i_CurrentBlock, 10000);
            fflush(stdout);

            // Use 1D arrays instead of 2D arrays
            uint8_t ym[4][BLOCK_SIZE * BLOCK_SIZE];  // Y matrix for this block
            uint8_t cbm[BLOCK_SIZE * BLOCK_SIZE]; // Cb matrix for this block
            uint8_t crm[BLOCK_SIZE * BLOCK_SIZE]; // Cr matrix for this block
            uint8_t macro[2 * BLOCK_SIZE * 2 * BLOCK_SIZE];
            int mat_quan[BLOCKSIZE * BLOCK_SIZE];
            // Copy Y, Cb, Cr values into 1D arrays (this assumes YCbCr format)
            for(int i = 0; i < 2 * BLOCK_SIZE; i++) {
                for(int j = 0; j < 2 * BLOCK_SIZE; j++) {
                    macro[i * 2 * BLOCK_SIZE + j] = imageinfo.buf_p[ 
                        (y_block * 2 * BLOCK_SIZE + i) * imageinfo.width + 
                        (x_block * 2 * BLOCK_SIZE + j)
                    ];
                }
            }
            seperateMatrix(ym, macro);
            for(int y = 0; y < BLOCK_SIZE; y++) {
                for(int x = 0; x < BLOCK_SIZE; x++) {
                    cbm[y * BLOCK_SIZE + x] = imageinfo.buf_p[ 
                        imageinfo.width * imageinfo.height + 
                        (y_block * BLOCK_SIZE + y) * imageinfo.width + 
                        (x_block * BLOCK_SIZE + x)
                    ];
                    int check = imageinfo.width * imageinfo.height * 5 / 4 + 
                        (y_block * BLOCK_SIZE + y) * imageinfo.width / 2 + 
                        (x_block * BLOCK_SIZE + x);
                    crm[y * BLOCK_SIZE + x] = imageinfo.buf_p[ 
                        imageinfo.width * imageinfo.height * 5 / 4 + 
                        (y_block * BLOCK_SIZE + y) * imageinfo.width / 2 + 
                        (x_block * BLOCK_SIZE + x)
                    ];
                }
            }

            // Apply DCT, quantization and Huffman encoding to the blocks
            for(int i = 0; i < 4; i++) {
                quantizeBlock(mat_quan, ym[i], quantization_table_y, SCALE_QUANT);
                bitstream_index = encode_mpeg1_y(bitstream_buffer, mat_quan, prev_dc_coeffi_y);
                fwrite(bitstream_buffer, sizeof(uint8_t), bitstream_index, file_mlv);
                prev_dc_coeffi_y = mat_quan[0]; // First element (DC coefficient for Y)
            }
            quantizeBlock(mat_quan, cbm, quantization_table_y, SCALE_QUANT);
            bitstream_index = encode_mpeg1_c(bitstream_buffer, mat_quan, prev_dc_coeffi_cb);
            fwrite(bitstream_buffer, sizeof(uint8_t), bitstream_index, file_mlv);
            prev_dc_coeffi_cb = mat_quan[0]; // First element (DC coefficient for Cb)
            quantizeBlock(mat_quan, crm, quantization_table_y, SCALE_QUANT);
            bitstream_index = encode_mpeg1_c(bitstream_buffer, mat_quan, prev_dc_coeffi_cr);
            fwrite(bitstream_buffer, sizeof(uint8_t), bitstream_index, file_mlv);
            prev_dc_coeffi_cr = mat_quan[0]; // First element (DC coefficient for Cr)
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
