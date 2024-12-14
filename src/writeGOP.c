#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// Mock functions to represent already encoded frames
extern uint8_t* getIFrameData(int* size); // Returns encoded I-frame data, size in bytes
extern uint8_t* getPFrameData(int* size); // Returns encoded P-frame data, size in bytes
extern uint8_t* getBFrameData(int* size); // Returns encoded B-frame data, size in bytes

// Write a 32-bit start code helper
static void write_start_code(FILE* f, uint32_t code) {
    fputc(0x00, f);
    fputc(0x00, f);
    fputc(0x01, f);
    fputc((uint8_t)code, f);
}

// Write a simple MPEG-1 sequence header
static void write_sequence_header(FILE* f, int width, int height) {
    write_start_code(f, 0xB3);

    uint32_t width_height = ((width & 0xFFF) << 20) | ((height & 0xFFF) << 8) | (1 << 4) | 3;
    fputc((width_height >> 24) & 0xFF, f);
    fputc((width_height >> 16) & 0xFF, f);
    fputc((width_height >> 8) & 0xFF, f);
    fputc(width_height & 0xFF, f);

    uint32_t bitrate_vbv = (0x1FFF << 6) | 0x3F;
    fputc((bitrate_vbv >> 16) & 0xFF, f);
    fputc((bitrate_vbv >> 8) & 0xFF, f);
    fputc(bitrate_vbv & 0xFF, f);
}

// Write a simple GOP header
static void write_gop_header(FILE* f) {
    write_start_code(f, 0xB8);
    uint8_t gop_header_bytes[4] = {0x00, 0x00, 0x00, 0x10};
    fwrite(gop_header_bytes, 1, sizeof(gop_header_bytes), f);
}

// Write an MPEG-1 frame
static void write_frame(FILE* f, uint8_t* frame_data, int size) {
    fwrite(frame_data, 1, size, f);
}

int main() {
    FILE* outfile = fopen("output.m1v", "wb");
    if (!outfile) {
        perror("Failed to open output file");
        return 1;
    }

    int i_size, p_size, b_size;

    // Call frame retrieval functions correctly
    uint8_t* i_frame = getIFrameData(&i_size);
    uint8_t* p_frame = getPFrameData(&p_size);
    uint8_t* b_frame = getBFrameData(&b_size);

    // Write sequence header and GOP header
    write_sequence_header(outfile, 352, 288);
    write_gop_header(outfile);

    // Write frames in decoding order
    write_frame(outfile, i_frame, i_size); // I-frame
    write_frame(outfile, p_frame, p_size); // P-frame
    write_frame(outfile, b_frame, b_size); // B-frame
    write_frame(outfile, b_frame, b_size); // Another B-frame

    fclose(outfile);

    return 0;
}