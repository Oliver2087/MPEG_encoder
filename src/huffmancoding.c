#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "huffmancoding.h"

// MPEG-1 DC coefficient Huffman encoding table (12 categories)
HuffmanCode dc_huffman_table[12] = {
    {0x0, 2},  // Category 0
    {0x1, 3},  // Category 1
    {0x3, 3},  // Category 2
    {0x7, 3},  // Category 3
    {0xF, 4},  // Category 4
    {0x1F, 5}, // Category 5
    {0x3F, 6}, // Category 6
    {0x7F, 7}, // Category 7
    {0xFF, 8}, // Category 8
    {0x1FF, 9}, // Category 9
    {0x3FF, 10}, // Category 10
    {0x7FF, 11}  // Category 11
};

// MPEG-1 AC coefficient Huffman encoding table (including symbols like 0x00, 0x01)
HuffmanCode ac_huffman_table[] = {
    {0x00, 0},  // End of Block (EOB)
    {0x01, 4},  // Run 0, Size 1
    {0x02, 5},  // Run 1, Size 1
    {0x03, 6},  // Run 2, Size 1
    {0x04, 7},  // Run 0, Size 2
    // Other entries should continue defining the table
};

// Zigzag scan table
const int zigzag_order[64] = {
    0,  1,  5,  6,  14, 15, 27, 28,
    2,  4,  7,  13, 16, 26, 29, 42,
    3,  8,  12, 17, 25, 30, 41, 43,
    9,  11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};

static int bitstream_index = 0;

// A dummy function to simulate bitstream writing
void writeBits(uint8_t* bitstream_buffer, uint16_t bitstring, uint8_t bitlength) {
    // Ensure the buffer has enough space
    if (bitstream_index + (bitlength / 8) + 1 > MAX_BITSTREAM_SIZE) {
        printf("Buffer overflow detected!\n");
        return;
    }

    // Writing the bitstring to the buffer
    for (int i = 0; i < bitlength; i++) {
        int byte_pos = bitstream_index + (i / 8);
        int bit_pos = 7 - (i % 8);
        uint8_t bit = (bitstring >> (bitlength - i - 1)) & 1;
        
        if (bit == 1) {
            bitstream_buffer[byte_pos] |= (1 << bit_pos);  // Set the bit
        } else {
            bitstream_buffer[byte_pos] &= ~(1 << bit_pos); // Clear the bit
        }
    }
    bitstream_index += (bitlength + 7) / 8; // Update buffer index
}

// Perform Huffman coding on the DCT coefficients
void performHuffmanCoding(uint8_t* bitstream_buffer, double* mat, double previous_dc_coeffi) {
    // 1. DC coefficient differential encoding
    int dc_coeffi = round(mat[0]); // Current block's DC coefficient
    int dc_diff = dc_coeffi - previous_dc_coeffi; // Difference
    if (dc_diff == 0) {
        dc_diff++;
    }
    int dc_category = log2(abs(dc_diff)) + 1; // Category of the difference (MPEG standard)
    if (dc_category > 11) dc_category = 11;  // Ensure the maximum category is 11
    HuffmanCode dc_code = dc_huffman_table[dc_category];
    writeBits(bitstream_buffer, dc_code.bitstring, dc_code.bitlength);

    // 2. AC coefficients encoding
    int run = 0; // Number of consecutive zeros
    for (int i = 1; i < 64; i++) {
        int index = zigzag_order[i];
        int coeff = round(mat[index]);
        
        if (coeff == 0) {
            run++;
            if (run == 16) { // More than 16 zeros, write the special code for Run = 15
                HuffmanCode zrl_code = ac_huffman_table[0x0f]; // Assume 0x0f represents 15 consecutive zeros
                writeBits(bitstream_buffer, zrl_code.bitstring, zrl_code.bitlength);
                run = 0;
            }
        } else {
            int size = log2(abs(coeff)) + 1; // Magnitude of the non-zero coefficient
            int run_size = (run << 4) | size; // Combination of run length and magnitude
            HuffmanCode ac_code = ac_huffman_table[run_size];
            writeBits(bitstream_buffer, ac_code.bitstring, ac_code.bitlength);
            // Encode the actual coefficient
            writeBits(bitstream_buffer, coeff, size);
            run = 0; // Reset the number of consecutive zeros
        }
    }

    // 3. Write End of Block (EOB)
    HuffmanCode eob_code = ac_huffman_table[0x00];  // Assume 0x00 is EOB
    writeBits(bitstream_buffer, eob_code.bitstring, eob_code.bitlength);
}