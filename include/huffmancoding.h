#ifndef HUFFMANCODING_H
#define HUFFMANCODING_H

#include <stdint.h> // For fixed-width integer types

// Maximum size for the bitstream buffer (adjust as needed)
#ifndef MAX_BITSTREAM_SIZE
#define MAX_BITSTREAM_SIZE 65536
#endif

// Structure for Huffman code: holds the bitstring and its length
typedef struct {
    uint32_t bitstring; // The code bits (stored in a 32-bit integer)
    uint8_t bitlength;  // Number of valid bits in bitstring
} HuffmanCode;

// Extern declarations of Huffman tables (defined in huffmancoding.c or another source file)
// For example, you might have:
extern HuffmanCode dc_huffman_table[12];
extern HuffmanCode ac_huffman_table[256]; 

// Zigzag order array for rearranging coefficients
extern const int zigzag_order[64];

// Global index tracking how many bits have been written into the current bitstream
extern int bitstream_index;

// Function prototypes

/**
 * @brief Perform Huffman coding on a single DCT block of coefficients.
 *
 * @param bitstream_buffer Buffer into which the Huffman-coded bits are written.
 * @param block Pointer to the block of quantized DCT coefficients (8x8 or 16x16 depending on your setup).
 * @param previous_dc_coeffi The DC coefficient of the previous block (for differential coding).
 */
void performHuffmanCoding(uint8_t* bitstream_buffer, double* block, double previous_dc_coeffi);

#endif // HUFFMANCODING_H