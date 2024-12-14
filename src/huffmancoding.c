#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "huffmancoding.h" // Ensure this contains HuffmanCode definition and MAX_BITSTREAM_SIZE

// Example DC Huffman table (category-indexed)
// These codes are placeholders; use the actual MPEG-1 standard table
HuffmanCode dc_huffman_table[] = {
    {0x00, 2},  // Category 0
    {0x02, 3},  // Category 1
    {0x03, 3},  // Category 2
    {0x04, 3},  // Category 3
    {0x0A, 4},  // Category 4
    {0x1A, 5},  // Category 5
    {0x3A, 6},  // Category 6
    {0x7A, 7},  // Category 7
    {0xFA, 8},  // Category 8
    {0x1FA,9},  // Category 9
    {0x3FA,10}, // Category 10
    {0x7FA,11}  // Category 11
};

// Example AC Huffman table for luminance (run-size indexed) as per JPEG baseline
// This is just a partial example. In reality, you'd fill out all 162 entries (for all run-size pairs).
// Index = run * 16 + size
// EOB = (run=0, size=0) -> index=0*16+0=0
// ZRL (run=15, size=0) -> index=15*16+0=240
HuffmanCode ac_huffman_table[256];  // You must populate this according to the standard

// Zigzag order
const int zigzag_order[64] = {
    0,  1,  5,  6, 14, 15, 27, 28,
    2,  4,  7, 13, 16, 26, 29, 42,
    3,  8, 12, 17, 25, 30, 41, 43,
    9, 11, 18, 24, 31, 40, 44, 53,
    10,19, 23, 32, 39, 45, 52, 54,
    20,22, 33, 38, 46, 51, 55, 60,
    21,34, 37, 47, 50, 56, 59, 61,
    35,36, 48, 49, 57, 58, 62, 63
};

int bitstream_index = 0;

static void writeBits(uint8_t* bitstream_buffer, uint32_t bits, int bitlength) {
    if (bitlength <= 0) return;
    
    // Ensure the buffer has enough space
    int required_bytes = (bitlength + 7) / 8;
    if (bitstream_index + required_bytes > MAX_BITSTREAM_SIZE) {
        fprintf(stderr, "Buffer overflow detected!\n");
        return;
    }

    for (int i = 0; i < bitlength; i++) {
        int byte_pos = bitstream_index;
        int bit_pos = 7 - (i % 8);
        uint8_t bit = (bits >> (bitlength - 1 - i)) & 1;

        // Set or clear the bit
        if (bit) {
            bitstream_buffer[byte_pos] |= (1 << bit_pos);
        } else {
            bitstream_buffer[byte_pos] &= ~(1 << bit_pos);
        }

        // Move to next byte if needed
        if (bit_pos == 0) byte_pos++;
        bitstream_index = byte_pos + (bit_pos == 0);
    }
}

// Helper function to get the number of bits (category) for a given value
static int getCategory(int val) {
    if (val == 0) return 0;
    return (int)floor(log2((double)abs(val))) + 1;
}

// Compute amplitude bits for DC/AC coefficient
static uint32_t getAmplitudeBits(int val, int size) {
    // For positive val: just val
    // For negative val: val + (2^size) -1
    if (val >= 0) {
        return (uint32_t)val;
    } else {
        return (uint32_t)(val + ( (1 << size) - 1 ));
    }
}

void performHuffmanCoding(uint8_t* bitstream_buffer, double* block, double previous_dc_coeffi) {
    // Clear the bitstream buffer for this block if needed
    // (Assumes caller clears/handles outside if encoding multiple blocks)
    // memset(bitstream_buffer, 0, MAX_BITSTREAM_SIZE); // If needed externally
    printf("Block size: %lu\n", sizeof(block) / sizeof(block[0])); // Printing the block size

    // Print all block values for debugging
    printf("Block values:\n");
    for (int i = 0; i < 64; i++) {
        printf("%f ", block[i]);
        if ((i + 1) % 8 == 0) {
            printf("\n"); // Print a newline every 8 values for better readability
        }
    }

    // DC Encoding
    int dc_coeff = (int)round(block[0]);
    int dc_diff = dc_coeff - (int)round(previous_dc_coeffi);

    int dc_cat = getCategory(dc_diff); // DC category
    HuffmanCode dc_code = dc_huffman_table[dc_cat];
    writeBits(bitstream_buffer, dc_code.bitstring, dc_code.bitlength);

    // Write DC amplitude bits (if category > 0)
    if (dc_cat > 0) {
        uint32_t amplitude = getAmplitudeBits(dc_diff, dc_cat);
        writeBits(bitstream_buffer, amplitude, dc_cat);
    }

    // AC Encoding
    int run = 0;
    for (int i = 1; i < 64; i++) {
        int idx = zigzag_order[i];
        printf("%f\n", block[idx]);
        int coeff = (int)round(block[idx]);

        if (coeff == 0) {
            run++;
            // If run is 16, write ZRL
            if (run == 16) {
                // ZRL is (run=15, size=0)
                int zrl_index = 15*16 + 0;
                HuffmanCode zrl_code = ac_huffman_table[zrl_index];
                writeBits(bitstream_buffer, zrl_code.bitstring, zrl_code.bitlength);
                run = 0;
            }
        } else {
            // Non-zero coefficient
            int size = getCategory(coeff);
            int run_size = run * 16 + size;
            HuffmanCode ac_code = ac_huffman_table[run_size];
            writeBits(bitstream_buffer, ac_code.bitstring, ac_code.bitlength);

            // Write AC amplitude bits
            uint32_t amplitude = getAmplitudeBits(coeff, size);
            writeBits(bitstream_buffer, amplitude, size);

            run = 0; // reset run
        }
    }

    // EOB if needed
    if (run > 0) {
        // EOB is (run=0, size=0)
        HuffmanCode eob_code = ac_huffman_table[0]; // index 0 = EOB
        writeBits(bitstream_buffer, eob_code.bitstring, eob_code.bitlength);
    }
}