#ifndef MPEG1_ENCODER_H
#define MPEG1_ENCODER_H

#include <stdint.h>

#define MAX_BITSTREAM_SIZE 1024

// MPEG-1 DC coefficient Huffman encoding table (12 categories)
extern const uint16_t ff_mpeg12_vlc_dc_lum_code[12];
extern const unsigned char ff_mpeg12_vlc_dc_lum_bits[12];

extern const uint16_t ff_mpeg12_vlc_dc_chroma_code[12];
extern const unsigned char ff_mpeg12_vlc_dc_chroma_bits[12];

// MPEG-1 AC coefficient Huffman encoding table (including symbols like 0x00, 0x01)
extern const uint16_t ff_mpeg1_vlc_table[113][2];

// Zigzag scan table
extern const int zigzag_scan[64];

// Function declarations
void encode_dc(int dc_val, int prev_dc_val, const uint16_t* code_table, const unsigned char* bit_table, int* code, int* bits);
void encode_ac(int ac_val, int* code, int* bits);
int encode_mpeg1(uint8_t* buffer, int matrix[64], int prev_dc, const uint16_t* huff_code, const unsigned char* huff_bits);
int encode_mpeg1_y(uint8_t* buffer, int matrix[64], int prev_dc);
int encode_mpeg1_c(uint8_t* buffer, int matrix[64], int prev_dc);

#endif // MPEG1_ENCODER_H
