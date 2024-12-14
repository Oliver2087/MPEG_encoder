#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// Buffer to store the bitstream and index to keep track of current position in buffer
#define MAX_BITSTREAM_SIZE 1024

// MPEG-1 DC coefficient Huffman encoding table (12 categories)
const uint16_t ff_mpeg12_vlc_dc_lum_code[12] = {
    0x4, 0x0, 0x1, 0x5, 0x6, 0xe, 0x1e, 0x3e, 0x7e, 0xfe, 0x1fe, 0x1ff,
};
const unsigned char ff_mpeg12_vlc_dc_lum_bits[12] = {
    3, 2, 2, 3, 3, 4, 5, 6, 7, 8, 9, 9,
};

const uint16_t ff_mpeg12_vlc_dc_chroma_code[12] = {
    0x0, 0x1, 0x2, 0x6, 0xe, 0x1e, 0x3e, 0x7e, 0xfe, 0x1fe, 0x3fe, 0x3ff,
};
const unsigned char ff_mpeg12_vlc_dc_chroma_bits[12] = {
    2, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 10,
};

// MPEG-1 AC coefficient Huffman encoding table (including symbols like 0x00, 0x01)
const uint16_t ff_mpeg1_vlc_table[113][2] = {
 { 0x3, 2 }, { 0x4, 4 }, { 0x5, 5 }, { 0x6, 7 },
 { 0x26, 8 }, { 0x21, 8 }, { 0xa, 10 }, { 0x1d, 12 },
 { 0x18, 12 }, { 0x13, 12 }, { 0x10, 12 }, { 0x1a, 13 },
 { 0x19, 13 }, { 0x18, 13 }, { 0x17, 13 }, { 0x1f, 14 },
 { 0x1e, 14 }, { 0x1d, 14 }, { 0x1c, 14 }, { 0x1b, 14 },
 { 0x1a, 14 }, { 0x19, 14 }, { 0x18, 14 }, { 0x17, 14 },
 { 0x16, 14 }, { 0x15, 14 }, { 0x14, 14 }, { 0x13, 14 },
 { 0x12, 14 }, { 0x11, 14 }, { 0x10, 14 }, { 0x18, 15 },
 { 0x17, 15 }, { 0x16, 15 }, { 0x15, 15 }, { 0x14, 15 },
 { 0x13, 15 }, { 0x12, 15 }, { 0x11, 15 }, { 0x10, 15 },
 { 0x3, 3 }, { 0x6, 6 }, { 0x25, 8 }, { 0xc, 10 },
 { 0x1b, 12 }, { 0x16, 13 }, { 0x15, 13 }, { 0x1f, 15 },
 { 0x1e, 15 }, { 0x1d, 15 }, { 0x1c, 15 }, { 0x1b, 15 },
 { 0x1a, 15 }, { 0x19, 15 }, { 0x13, 16 }, { 0x12, 16 },
 { 0x11, 16 }, { 0x10, 16 }, { 0x5, 4 }, { 0x4, 7 },
 { 0xb, 10 }, { 0x14, 12 }, { 0x14, 13 }, { 0x7, 5 },
 { 0x24, 8 }, { 0x1c, 12 }, { 0x13, 13 }, { 0x6, 5 },
 { 0xf, 10 }, { 0x12, 12 }, { 0x7, 6 }, { 0x9, 10 },
 { 0x12, 13 }, { 0x5, 6 }, { 0x1e, 12 }, { 0x14, 16 },
 { 0x4, 6 }, { 0x15, 12 }, { 0x7, 7 }, { 0x11, 12 },
 { 0x5, 7 }, { 0x11, 13 }, { 0x27, 8 }, { 0x10, 13 },
 { 0x23, 8 }, { 0x1a, 16 }, { 0x22, 8 }, { 0x19, 16 },
 { 0x20, 8 }, { 0x18, 16 }, { 0xe, 10 }, { 0x17, 16 },
 { 0xd, 10 }, { 0x16, 16 }, { 0x8, 10 }, { 0x15, 16 },
 { 0x1f, 12 }, { 0x1a, 12 }, { 0x19, 12 }, { 0x17, 12 },
 { 0x16, 12 }, { 0x1f, 13 }, { 0x1e, 13 }, { 0x1d, 13 },
 { 0x1c, 13 }, { 0x1b, 13 }, { 0x1f, 16 }, { 0x1e, 16 },
 { 0x1d, 16 }, { 0x1c, 16 }, { 0x1b, 16 },
 { 0x1, 6 }, /* escape */
 { 0x2, 2 }, /* EOB */
};

// Zigzag scan table
const int zigzag_scan[64] = {
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51, 52, 45, 38,
    31, 39, 46, 53, 54, 47, 55, 56, 57, 58, 59, 60, 61, 62, 63
};

// Helper function to get the DC coefficient Huffman code and bits
void encode_dc(int dc_val, const uint16_t* code_table, const unsigned char* bit_table, int* code, int* bits) {
    int category = dc_val < 0 ? -dc_val : dc_val;  // Absolute value for category
    if (category > 11) category = 11;  // Limit to the max category (12 categories)

    *code = code_table[category];
    *bits = bit_table[category];
}

// Helper function to get the AC coefficient Huffman code and bits
void encode_ac(int ac_val, int* code, int* bits) {
    if (ac_val == 0) {
        *code = 0x00;  // EOB (End of Block)
        *bits = 1;
    } else {
        if (ac_val < 0 || ac_val > 113) {
            *code = 0x00;  // Handle out-of-range AC values
            *bits = 1;
        } else { // maybe we only need this
            *code = ff_mpeg1_vlc_table[ac_val][0];
            *bits = ff_mpeg1_vlc_table[ac_val][1];
        }
    }
}

// Function to encode the 8x8 matrix with Zigzag scan order and write to the buffer
int encode_mpeg1_y(uint8_t *matrix, uint8_t *buffer) {
    int buffer_index = 0;
    int dc_code, dc_bits;
    int ac_code, ac_bits;
    int run_length = 0;  // 用于RLE的计数器

    // Zigzag scan the 8x8 matrix and encode DC and AC coefficients
    encode_dc(matrix[zigzag_scan[0]], ff_mpeg12_vlc_dc_lum_code, ff_mpeg12_vlc_dc_lum_bits, &dc_code, &dc_bits);
    buffer[buffer_index++] = (uint8_t)(dc_code & 0xFF);  // Write code byte
    buffer[buffer_index++] = (uint8_t)(dc_bits);         // Write number of bits

    // Encode the AC coefficients (remaining values in the matrix, in Zigzag order)
    for (int i = 1; i < 64; i++) {
        int zigzag_index = zigzag_scan[i];
        if (matrix[zigzag_index] == 0) {
            run_length++;  // 增加零的计数
        } else {
            // 如果不是零，先编码所有的零（如果有的话）
            while (run_length > 15) {
                buffer[buffer_index++] = 0xF0;  // 代表15个零
                buffer[buffer_index++] = 0x0;   // 代码字节
                buffer[buffer_index++] = 4;     // 编码长度
                run_length -= 15;
            }
            // 编码当前非零系数
            encode_ac(matrix[zigzag_index], &ac_code, &ac_bits);
            buffer[buffer_index++] = (uint8_t)(ac_code & 0xFF);  // Write code byte
            buffer[buffer_index++] = (uint8_t)(ac_bits);         // Write number of bits
        }
    }
    // 在编码结束时加入EOB标记
    if (run_length > 0) {
        buffer[buffer_index++] = 0x00;  // EOB (End of Block)
        buffer[buffer_index++] = 1;     // 编码长度
    }

    return buffer_index;  // 返回写入缓冲区的字节数
}

// Function to encode the 8x8 matrix with Zigzag scan order and write to the buffer
int encode_mpeg1_c(uint8_t *matrix, uint8_t *buffer) {
    int buffer_index = 0;
    int dc_code, dc_bits;
    int ac_code, ac_bits;
    int run_length = 0;  // 用于RLE的计数器

    // Zigzag scan the 8x8 matrix and encode DC and AC coefficients
    encode_dc(matrix[zigzag_scan[0]], ff_mpeg12_vlc_dc_chroma_code, ff_mpeg12_vlc_dc_chroma_bits, &dc_code, &dc_bits);
    buffer[buffer_index++] = (uint8_t)(dc_code & 0xFF);  // Write code byte
    buffer[buffer_index++] = (uint8_t)(dc_bits);         // Write number of bits

    // Encode the AC coefficients (remaining values in the matrix, in Zigzag order)
    for (int i = 1; i < 64; i++) {
        int zigzag_index = zigzag_scan[i];
        if (matrix[zigzag_index] == 0) {
            run_length++;  // 增加零的计数
        } else {
            // 如果不是零，先编码所有的零（如果有的话）
            while (run_length > 15) {
                buffer[buffer_index++] = 0xF0;  // 代表15个零
                buffer[buffer_index++] = 0x0;   // 代码字节
                buffer[buffer_index++] = 4;     // 编码长度
                run_length -= 15;
            }
            // 编码当前非零系数
            encode_ac(matrix[zigzag_index], &ac_code, &ac_bits);
            buffer[buffer_index++] = (uint8_t)(ac_code & 0xFF);  // Write code byte
            buffer[buffer_index++] = (uint8_t)(ac_bits);         // Write number of bits
        }
    }
    // 在编码结束时加入EOB标记
    if (run_length > 0) {
        buffer[buffer_index++] = 0x00;  // EOB (End of Block)
        buffer[buffer_index++] = 1;     // 编码长度
    }

    return buffer_index;  // 返回写入缓冲区的字节数
}
