#ifndef HUFFMANENCODING_H
#define HUFFMANENCODING_H

#include <stdint.h>

// Buffer size to store the bitstream
#define MAX_BITSTREAM_SIZE 1024

// Simulate Huffman encoding (actual implementation should use standard tables)
typedef struct {
    uint16_t bitstring; // Encoded bitstring
    uint8_t bitlength;  // Length of the bitstring
} HuffmanCode;

// MPEG-1 DC coefficient Huffman encoding table (12 categories)
extern HuffmanCode dc_huffman_table[12];

// MPEG-1 AC coefficient Huffman encoding table (including symbols like 0x00, 0x01)
extern HuffmanCode ac_huffman_table[];

// Zigzag scan table
extern const int zigzag_order[64];

// Function prototypes
void writeBits(uint8_t* bitstream_buffer, uint16_t bitstring, uint8_t bitlength);
void performHuffmanCoding(uint8_t* bitstream_buffer, double* mat, double previous_dc_coeffi);
#endif // HUFFMAN_ENCODING_H
