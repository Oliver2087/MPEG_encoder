#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "processFrame.h"
#include "motionCompEst.h"
#include "huffmancoding.h"
#include "encoder.h"

#define BLOCK_SIZE 16
#define GOP_SIZE 8
#define MAX_BITSTREAM_SIZE 1024 * 1024 // 1 MB buffer for bitstream

// Global buffer for bitstream
int bitstream_index = 0; // Initialize bitstream index to 0
uint8_t bitstream_buffer[MAX_BITSTREAM_SIZE];

// Function to write a frame's DCT coefficients using Huffman encoding
void writeSliceData(FILE* output, unsigned char* frame_data, int width, int height, double* previous_dc_coeff) {
    for (int y = 0; y < height; y += BLOCK_SIZE) {
        for (int x = 0; x < width; x += BLOCK_SIZE) {
            // Extract the block
            double block[BLOCK_SIZE * BLOCK_SIZE] = {0};
            for (int by = 0; by < BLOCK_SIZE; ++by) {
                for (int bx = 0; bx < BLOCK_SIZE; ++bx) {
                    block[by * BLOCK_SIZE + bx] = frame_data[(y + by) * width + (x + bx)];
                }
            }

            // Perform DCT on the block
            performFastDCT(block);

            // Huffman encode the DCT coefficients
            performHuffmanCoding(bitstream_buffer, block, *previous_dc_coeff);

            // Update the DC coefficient for differential encoding
            *previous_dc_coeff = block[0];
        }
    }

    // Write the bitstream buffer to the output
    fwrite(bitstream_buffer, 1, (bitstream_index + 7) / 8, output); // Ensure full bytes are written
    bitstream_index = 0; // Reset buffer index
}

// Function to encode a GOP using Huffman coding
void encodeGOP(FILE* output, const char* gop_file, int width, int height, int frame_rate_code) {
    FILE* gop_input = fopen(gop_file, "rb");
    if (!gop_input) {
        fprintf(stderr, "Error opening GOP file: %s\n", gop_file);
        exit(EXIT_FAILURE);
    }

    writeGOPHeader(output, 0, 1); // Example time code

    unsigned char* frame_data = malloc(width * height);
    if (!frame_data) {
        fprintf(stderr, "Memory allocation error for frame data\n");
        fclose(gop_input);
        exit(EXIT_FAILURE);
    }

    double previous_dc_coeff = 0.0; // Track the previous DC coefficient

    // Read and process each frame in the GOP
    while (fread(frame_data, 1, width * height, gop_input) == (size_t)(width * height)) {
        writePictureHeader(output, 0, 1); // Example temporal reference and I-frame
        writeSliceData(output, frame_data, width, height, &previous_dc_coeff);
    }

    free(frame_data);
    fclose(gop_input);
}
