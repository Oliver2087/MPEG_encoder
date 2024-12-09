#include "createMLV.h"

// Write the sequence header of the mpeg file
// bit_rate: choose one of the following:
//     1: 23.976 fps
//     2: 24 fps
//     3: 25 fps
//     4: 29.97 fps
//     5: 30 fps
//     6: 50 fps
//     7: 59.94 fps
//     8: 60 fps
// n is the index of the image
void writePackHeader(FILE* file_mpeg, int width, int height, int frame_rate, uint32_t bit_rate) {
    unsigned char sequence_header[12];

    // Sync Byte
    sequence_header[0] = 0x00;
    sequence_header[1] = 0x00;
    sequence_header[2] = 0x01;
    sequence_header[3] = 0xBA; // Start of video stream

    // System clock reference
    size_t scr = 0 * 90000 / frame_rate;
    sequence_header[4] = 0x21;
    sequence_header[5] = 0x00;
    sequence_header[6] = 0x01;
    sequence_header[7] = 0x00;
    sequence_header[8] = 0x01;

    // Resolution (12 bits for horizontal, 12 bits for vertical)
    //sequence_header[4] = (width >> 4) & 0xFF;                   // Higher 8 bits of horizontal size
    //sequence_header[5] = ((width & 0x0F) << 4) | ((height >> 8) & 0x0F); // Lower 4 bits of horizontal and higher 4 bits of vertical
    //sequence_header[6] = height & 0xFF;                         // Lower 8 bits of vertical size

    // Frame rate and bit rate
    //sequence_header[7] = (bit_rate >> 10) & 0xFF;               // Higher 8 bits of bit rate
    //sequence_header[8] = (bit_rate >> 2) & 0xFF;                // Middle 8 bits of bit rate
    sequence_header[9] = (bit_rate >> 15) | 0x80;       // Lower 2 bits of bit rate and VBV buffer size

    // Frame rate code and marker bits
    sequence_header[10] = (bit_rate >> 7) & 0xFF;        // Frame rate code (4 bits)
    sequence_header[11] = ((bit_rate & 0x7F) << 1) | 0x01;                                 // Marker bits

    // Write Sequence Header to file
    fwrite(sequence_header, 1, sizeof(sequence_header), file_mpeg);
}

// Optional. Including ratebound. Not used now
void writeSystemHeader(FILE* file_mpeg, uint16_t len_header) {
    unsigned char system_header[12];
    system_header[0] = 0x00;
    system_header[1] = 0x00;
    system_header[2] = 0x01;
    system_header[3] = 0xBB;

    system_header[4] = len_header >> 8;
    system_header[5] = len_header & 0xFF;
}

// the unit of buffer size is 128
void writePacket(FILE* file_mpeg, uint16_t len_pack, uint16_t size_buffer) {
    unsigned char system_header[12];
    system_header[0] = 0x00;
    system_header[1] = 0x00;
    system_header[2] = 0x01;
    system_header[3] = 0xE1;

    system_header[4] = len_pack >> 8;
    system_header[5] = len_pack & 0xFF;

    system_header[6] = (size_buffer >> 8) | 0x40;
    system_header[7] = size_buffer & 0xFF;

    // PTS
    system_header[8] = 0x20;
    system_header[9] = 0x00;
    system_header[10] = 0x01;
    system_header[11] = 0x00;
    system_header[12] = 0x01;

    fwrite(system_header, 1, sizeof(system_header), file_mpeg);
}

void writeSequenceHeader(FILE* file_mpeg, uint16_t width, uint16_t height) {
    unsigned char sequence_header[12];
    sequence_header[0] = 0x00;
    sequence_header[1] = 0x00;
    sequence_header[2] = 0x01;
    sequence_header[3] = 0xB3;

    sequence_header[4] = width >> 4;
    sequence_header[5] = ((width & 0xF) << 1) | (height >> 8);
    sequence_header[6] = height & 0xFF;

    sequence_header[7] = 0x15; // 0100 0000
    sequence_header[8] = 0xFF; // 0001 1100
    sequence_header[9] = 0xFF; // 0111 0000
    uint16_t size_buf = (uint16_t)(width * height * 1.5) + 1;
    sequence_header[10] = 0xE0 | (size_buf >> 5);
    sequence_header[11] = ((size_buf & 0x1F) << 3) | 0x00;
    fwrite(sequence_header, 1, sizeof(sequence_header), file_mpeg);
}

void writeGOPHeader(FILE* file_mlv) {
    unsigned char header_GOP[8];
    header_GOP[0] = 0x00;
    header_GOP[1] = 0x00;
    header_GOP[2] = 0x01;
    header_GOP[3] = 0xB8;

    header_GOP[4] = 0x80;// drop frame & time
    header_GOP[5] = 0x08;
    header_GOP[6] = 0x00;
    header_GOP[7] = 0x40;
    fwrite(header_GOP, 1, sizeof(header_GOP), file_mlv);
}

void writePictureHeader(FILE* file_mlv) {
    unsigned char header_pic[8];
    header_pic[0] = 0x00;
    header_pic[1] = 0x00;
    header_pic[2] = 0x01;
    header_pic[3] = 0x00;

    header_pic[4] = 0x00;
    header_pic[5] = 0x0F;
    header_pic[6] = 0xFF;
    header_pic[7] = 0xF8;
    fwrite(header_pic, 1, sizeof(header_pic), file_mlv);
}

// Parameter:
// index: the vertical position of slice (only 4 bit is allowed)
// scal: quatization scale 0-51
void writeSliceHeader(FILE* file_mlv, uint8_t index, uint8_t scal) {
    unsigned char header_sli[5];
    header_sli[0] = 0x00;
    header_sli[1] = 0x00;
    header_sli[2] = 0x01;
    header_sli[3] = index;

    header_sli[4] = (scal << 3) | 0x03;
    fwrite(header_sli, 1, sizeof(header_sli), file_mlv);
}

FILE* createMLV(char* filename_p, ImageInfo imageinfo) {
    FILE* file_mlv = fopen(filename_p, "wb");
    writeSequenceHeader(file_mlv, imageinfo.width, imageinfo.height);
    writeGOPHeader(file_mlv);
    writePictureHeader(file_mlv);
    return file_mlv;
}