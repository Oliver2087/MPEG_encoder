#ifndef MPEG1_ENCODER_H
#define MPEG1_ENCODER_H

#include <stdint.h>

// Constants
#define MAX_BITSTREAM_SIZE 1024

// Huffman encoding tables for DC coefficients (Luminance and Chrominance)
extern const uint16_t ff_mpeg12_vlc_dc_lum_code[12];
extern const unsigned char ff_mpeg12_vlc_dc_lum_bits[12];
extern const uint16_t ff_mpeg12_vlc_dc_chroma_code[12];
extern const unsigned char ff_mpeg12_vlc_dc_chroma_bits[12];

// Huffman encoding table for AC coefficients
extern const uint16_t ff_mpeg1_vlc_table[113][2];

// Zigzag scan table
extern const int zigzag_scan[64];

// Function prototypes
/**
 * Encodes a DC coefficient using the provided Huffman code and bit tables.
 *
 * @param dc_val       DC coefficient value.
 * @param code_table   Huffman code table for DC coefficients.
 * @param bit_table    Huffman bit table for DC coefficients.
 * @param code         Pointer to store the generated Huffman code.
 * @param bits         Pointer to store the number of bits in the Huffman code.
 */
void encode_dc(int dc_val, const uint16_t* code_table, const unsigned char* bit_table, int* code, int* bits);

/**
 * Encodes an AC coefficient using the MPEG-1 Huffman encoding table.
 *
 * @param ac_val       AC coefficient value.
 * @param code         Pointer to store the generated Huffman code.
 * @param bits         Pointer to store the number of bits in the Huffman code.
 */
void encode_ac(int ac_val, int* code, int* bits);

/**
 * Encodes an 8x8 luminance block of coefficients using the MPEG-1 Zigzag order and Huffman tables.
 *
 * @param matrix       Pointer to the 8x8 coefficient matrix (64 elements).
 * @param buffer       Pointer to the buffer where encoded data will be written.
 * @return             The length of the encoded data in bytes.
 */
int encode_mpeg1_y(uint8_t *matrix, uint8_t *buffer);

/**
 * Encodes an 8x8 chrominance block of coefficients using the MPEG-1 Zigzag order and Huffman tables.
 *
 * @param matrix       Pointer to the 8x8 coefficient matrix (64 elements).
 * @param buffer       Pointer to the buffer where encoded data will be written.
 * @return             The length of the encoded data in bytes.
 */
int encode_mpeg1_c(uint8_t *matrix, uint8_t *buffer);

#endif // MPEG1_ENCODER_H
