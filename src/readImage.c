#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "ycbcr_to_yuv420.h"

#include <jpeglib.h>

#include <huffmancoding.h>

#include "countDir.h"

#define FILENAME_OUTPUT "output.mpg"
#define PI 3.1415927
#define NUMOFLINESREADINONETIME 16
#define FRAMERATE 30
#define SCALE_QUANT 23
#define GOP_SIZE 8
#define MB_SIZE 16
#define BITRATE 1152000
#define BLOCK_SIZE 8

typedef enum { 
    I_FRAME, 
    P_FRAME, 
    B_FRAME 
} FrameType;

const unsigned char quantization_table_y[BLOCK_SIZE * BLOCK_SIZE] = {
    16, 11, 10, 16, 24, 40, 51, 61,
    12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56,
    14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77,
    24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103, 99
};

const unsigned char quantization_table_c[BLOCK_SIZE * BLOCK_SIZE] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 13, 21, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99
};

int decompressJPEG(struct jpeg_decompress_struct* cinfo_p, unsigned char** bmp_buffer, char* filename) {
    FILE* infile = fopen(filename, "rb");
    if(!infile) {
        fprintf(stderr, "Error opening JPEG file %s!\n", filename);
        exit(EXIT_FAILURE);
    }
    
    struct jpeg_error_mgr jerr;
    
    (*cinfo_p).err = jpeg_std_error(&jerr);
    jpeg_create_decompress(cinfo_p);
    jpeg_stdio_src(cinfo_p, infile);   // Set cinfo.src
    jpeg_read_header(cinfo_p, TRUE);
    cinfo_p->out_color_space = JCS_YCbCr;
    jpeg_start_decompress(cinfo_p);

    int width = (*cinfo_p).output_width;
    int height = (*cinfo_p).output_height;
    int pixel_size = (*cinfo_p).output_components;
    unsigned long bmp_size = width * height * pixel_size;
    *bmp_buffer = (unsigned char*)malloc(bmp_size);
    int batch_size = NUMOFLINESREADINONETIME; // The number of lines the algorithm is going to read in one time
    unsigned char* rowptr[batch_size];
    while((*cinfo_p).output_scanline < height) {
        int lines_to_read = (*cinfo_p).output_scanline +
            batch_size > height ? height - (*cinfo_p).output_scanline :
            batch_size;
        // Set row pointers for the batch
        for (int i = 0; i < lines_to_read; i++) {
            rowptr[i] = *bmp_buffer + ((*cinfo_p).output_scanline + i) * width * pixel_size;
        }
        jpeg_read_scanlines(cinfo_p, rowptr, lines_to_read);
    }
    jpeg_finish_decompress(cinfo_p);
    fclose(infile);
    return 0;
}

void performFastDCT(double block[BLOCK_SIZE*BLOCK_SIZE]) {
    double temp[BLOCK_SIZE*BLOCK_SIZE];
    double cu, cv;

    // 1D DCT on rows
    for(int u = 0; u < BLOCK_SIZE; u++) {
        for(int x = 0; x < BLOCK_SIZE; x++) {
            temp[u*BLOCK_SIZE+x] = 0;
            cu = (u == 0) ? 1.0 / sqrt(2.0) : 1.0;
            for(int i = 0; i < BLOCK_SIZE; i++) {
                temp[u*BLOCK_SIZE+x] += block[u*BLOCK_SIZE+i] * cos((2.0 * i + 1.0) * x * PI / (2.0 * BLOCK_SIZE));
            }
            temp[u*BLOCK_SIZE+x] *= 0.5 * cu;
        }
    }

    // 1D DCT on columns
    for(int v = 0; v < BLOCK_SIZE; v++) {
        for(int y = 0; y < BLOCK_SIZE; y++) {
            block[y*BLOCK_SIZE+v] = 0;
            cv = (v == 0) ? 1.0 / sqrt(2.0) : 1.0;
            for(int j = 0; j < BLOCK_SIZE; j++) {
                block[y*BLOCK_SIZE+v] += temp[j*BLOCK_SIZE+v] * cos((2.0 * j + 1.0) * y * PI / (2.0 * BLOCK_SIZE));
            }
            block[y*BLOCK_SIZE+v] *= 0.5 * cv;
        }
    }
}

// Function to apply 2D DCT on an 8x8 block
void performDCT(double block[BLOCK_SIZE][BLOCK_SIZE]) {
    double temp[BLOCK_SIZE][BLOCK_SIZE]; // Temporary array to store DCT results
    double cu, cv, sum;

    // Perform the 2D DCT
    for(int u = 0; u < BLOCK_SIZE; u++) {
        for(int v = 0; v < BLOCK_SIZE; v++) {
            sum = 0.0; // Initialize sum for each (u, v)
            
            // Calculate normalization factors
            cu = (u == 0) ? 1.0 / sqrt(2.0) : 1.0;
            cv = (v == 0) ? 1.0 / sqrt(2.0) : 1.0;

            // Perform the summation over x and y
            for(int x = 0; x < BLOCK_SIZE; x++) {
                for(int y = 0; y < BLOCK_SIZE; y++) {
                    sum += block[x][y] * 
                        cos((2.0 * x + 1.0) * u * PI / (2.0 * BLOCK_SIZE)) *
                        cos((2.0 * y + 1.0) * v * PI / (2.0 * BLOCK_SIZE));
                }
            }

            // Scale the result and store it in the temp array
            temp[u][v] = 0.25 * cu * cv * sum;
        }
    }

    // Copy the result back into the original block
    for (int u = 0; u < BLOCK_SIZE; u++) {
        for (int v = 0; v < BLOCK_SIZE; v++) {
            block[u][v] = temp[u][v];
        }
    }
}

// scale from 0 to 51
void quantizeBlock(double block[BLOCK_SIZE*BLOCK_SIZE], const unsigned char* quantization_table, uint8_t scale) {
    for(int i = 0; i < BLOCK_SIZE; i++) {
        for(int j = 0; j < BLOCK_SIZE; j++) {
            block[i*BLOCK_SIZE+j] = round(block[i*BLOCK_SIZE+j] / (scale * quantization_table[i * BLOCK_SIZE + j]));
        }
    }
}

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

void writeSystemHeader(FILE* file_mpeg) {
    uint16_t len_header = 7; // 7 bytes for rate bound, reserved bits, and 1 video stream
    unsigned char system_header[12];

    // 1. System Header Start Code
    system_header[0] = 0x00;
    system_header[1] = 0x00;
    system_header[2] = 0x01;
    system_header[3] = 0xBB; // System Header Start Code (0x000001BB)

    // 2. Header Length
    system_header[4] = (len_header >> 8) & 0xFF; // High byte
    system_header[5] = len_header & 0xFF;        // Low byte

    // 3. Rate Bound (22 bits, example: 1152 kbps / 50 = 23040)
    uint32_t rate_bound = 1152000 / 50;
    system_header[6] = (rate_bound >> 14) & 0xFF;
    system_header[7] = (rate_bound >> 6) & 0xFF;
    system_header[8] = ((rate_bound & 0x3F) << 2) | 0x01; // Marker bit

    // 4. Reserved Bits
    system_header[9] = 0x80; // Reserved bits, set to '10000000'

    // 5. Video Stream Descriptor
    system_header[10] = 0xE0; // Video Stream ID (0xE0 for primary video)
    system_header[11] = 0x07; // Buffer size (example value)

    // Write the System Header to the file
    fwrite(system_header, sizeof(unsigned char), sizeof(system_header), file_mpeg);
}

void writeEndOfFile(FILE* file_mpeg) {
    // End of file marker for MPEG-1
    unsigned char eof_marker[4] = {0x00, 0x00, 0x01, 0xB9}; // Start code for EOF
    fwrite(eof_marker, 1, sizeof(eof_marker), file_mpeg);
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
    sequence_header[5] = ((width & 0xF) << 4) | (height >> 8);
    sequence_header[6] = height & 0xFF;

    sequence_header[7] = (1 << 4) | 3; // Aspect ratio: 1 (1:1), Frame rate: 3 (29.97 fps)
    sequence_header[8] = 0x2C;         // Bitrate high byte (1150000 / 400 = 2875)
    sequence_header[9] = 0x3B;         // Bitrate low byte
    sequence_header[10] = 0x20;        // VBV buffer size high byte
    sequence_header[11] = 0x00;        // VBV buffer size low byte
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

void writePictureHeader(FILE* file_mlv, FrameType frameType, uint16_t temporalReference) {
    unsigned char header_pic[8];
    header_pic[0] = 0x00;
    header_pic[1] = 0x00;
    header_pic[2] = 0x01;
    header_pic[3] = 0x00;

    // Temporal Reference (10 bits)
    header_pic[4] = (temporalReference >> 2) & 0xFF;            // Upper 8 bits of temporal reference
    header_pic[5] = (temporalReference & 0x03) << 6;            // Lower 2 bits of temporal reference

    // Picture Coding Type (3 bits)
    switch (frameType) {
        case I_FRAME:
            header_pic[5] |= (1 << 3); // Coding type: 1 (I-frame)
            break;
        case P_FRAME:
            header_pic[5] |= (2 << 3); // Coding type: 2 (P-frame)
            break;
        case B_FRAME:
            header_pic[5] |= (3 << 3); // Coding type: 3 (B-frame)
            break;
        default:
            fprintf(stderr, "Error: Invalid frame type.\n");
            return;
    }
    // Write Picture Header to the file
    fwrite(header_pic, 1, sizeof(header_pic), file_mlv);

    // Debug output
    const char* frameTypeStr = (frameType == I_FRAME) ? "I-frame" :
                               (frameType == P_FRAME) ? "P-frame" :
                               "B-frame";
    printf("Picture Header Written: Temporal Reference = %d, Picture Type = %s\n", temporalReference, frameTypeStr);
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

void writeSlice(FILE* file_mlv, int sliceNumber, int quantizationScale,
                unsigned char* Y, unsigned char* U, unsigned char* V,
                int width, int height) {
    int i_CurrentBlock = 0;
    printf("Processing slice: %d\n", sliceNumber);

    // Write Slice Header
    writeSliceHeader(file_mlv, sliceNumber, quantizationScale);

    // DC Coefficients for Differential Encoding
    double prev_dc_coeffi_y = 0.0;
    double prev_dc_coeffi_cb = 0.0;
    double prev_dc_coeffi_cr = 0.0;

    // Allocate Bitstream Buffer
    uint8_t* bitstream_buffer = (uint8_t*)malloc(MAX_BITSTREAM_SIZE * sizeof(uint8_t));
    if (!bitstream_buffer) {
        fprintf(stderr, "Error: Failed to allocate memory for bitstream buffer.\n");
        exit(1);
    }

    int sliceStartY = sliceNumber * BLOCK_SIZE;
    int sliceEndY = sliceStartY + BLOCK_SIZE;
    if (sliceEndY > height) {
        sliceEndY = height; // Handle edge case for partial slices
    }

    // Process Each 8x8 Block (Macroblock) in the Slice
    for (int x_block = 0; x_block < width / BLOCK_SIZE; x_block++) {
        i_CurrentBlock++;
        printf("\rProcessing macroblock: %d", i_CurrentBlock);
        fflush(stdout);

        // Y, Cb, Cr Blocks (8x8 each)
        double ym[BLOCK_SIZE * BLOCK_SIZE];
        double cbm[BLOCK_SIZE * BLOCK_SIZE];
        double crm[BLOCK_SIZE * BLOCK_SIZE];

        // Copy data from Y, U, V into 8x8 blocks
        for (int y = 0; y < BLOCK_SIZE && (sliceStartY + y) < height; y++) {
            for (int x = 0; x < BLOCK_SIZE && (x_block * BLOCK_SIZE + x) < width; x++) {
                // Y plane
                ym[y * BLOCK_SIZE + x] = Y[(sliceStartY + y) * width + (x_block * BLOCK_SIZE + x)];

                // Chroma indexing with 4:2:0 subsampling
                int uy = (sliceStartY + y) / 2;
                int ux = (x_block * BLOCK_SIZE + x) / 2;

                if (uy < (height / 2) && ux < (width / 2)) {
                    cbm[y * BLOCK_SIZE + x] = U[uy * (width / 2) + ux];
                    crm[y * BLOCK_SIZE + x] = V[uy * (width / 2) + ux];
                } else {
                    // Edge cases if dimensions are not multiples of BLOCK_SIZE
                    cbm[y * BLOCK_SIZE + x] = 0.0;
                    crm[y * BLOCK_SIZE + x] = 0.0;
                }
            }
        }
        // Process Y Block
        int bitstream_index = 0; // Reset for each block
        performFastDCT(ym);
        quantizeBlock(ym, quantization_table_y, quantizationScale);
        bitstream_index = encode_mpeg1_y((uint8_t*)ym, bitstream_buffer);
        //performHuffmanCoding(bitstream_buffer, &bitstream_index, ym, prev_dc_coeffi_y);

        if (fwrite(bitstream_buffer, sizeof(uint8_t), bitstream_index, file_mlv) != (size_t)bitstream_index) {
            fprintf(stderr, "Error: Failed to write Y block.\n");
            free(bitstream_buffer);
            exit(1);
        }
        prev_dc_coeffi_y = round(ym[0]); // Update DC for next block

        // Process Cb Block
        bitstream_index = 0;
        performFastDCT(cbm);
        quantizeBlock(cbm, quantization_table_c, quantizationScale);
        bitstream_index = encode_mpeg1_c((uint8_t*)cbm, bitstream_buffer);
        //performHuffmanCoding(bitstream_buffer, &bitstream_index, cbm, prev_dc_coeffi_cb);
        
        if (fwrite(bitstream_buffer, sizeof(uint8_t), bitstream_index, file_mlv) != (size_t)bitstream_index) {
            fprintf(stderr, "Error: Failed to write Cb block.\n");
            free(bitstream_buffer);
            exit(1);
        }
        prev_dc_coeffi_cb = round(cbm[0]); // Update DC for next block

        // Process Cr Block
        bitstream_index = 0;
        performFastDCT(crm);
        quantizeBlock(crm, quantization_table_c, quantizationScale);
        bitstream_index = encode_mpeg1_c((uint8_t*)crm, bitstream_buffer);
        //performHuffmanCoding(bitstream_buffer, &bitstream_index, crm, prev_dc_coeffi_cr);

        if (fwrite(bitstream_buffer, sizeof(uint8_t), bitstream_index, file_mlv) != (size_t)bitstream_index) {
            fprintf(stderr, "Error: Failed to write Cr block.\n");
            free(bitstream_buffer);
            exit(1);
        }
        prev_dc_coeffi_cr = round(crm[0]); // Update DC for next block
    }

    free(bitstream_buffer);
    printf("\nFinished processing slice: %d\n", sliceNumber);
}


struct jpeg_decompress_struct readImage(unsigned char *filename_i, unsigned char **Y, unsigned char **U, unsigned char **V) {
    struct jpeg_decompress_struct cinfo;
    unsigned char *bmp_buffer = NULL;
    decompressJPEG(&cinfo, &bmp_buffer, filename_i);

    // Now convert to YUV420
    convertYCbCrToYUV420(bmp_buffer, Y, U, V, cinfo.output_width, cinfo.output_height);
    free(bmp_buffer);

    printf("Number of components: %d\n", cinfo.num_components);
    if (cinfo.jpeg_color_space == JCS_YCbCr) {
        printf("The image is in YUV (YCbCr) format.\n");
    } else if (cinfo.jpeg_color_space == JCS_RGB) {
        perror("The image is in RGB format.\n");
        exit(EXIT_FAILURE);
    } else {
        perror("The image is in an unsupported color space.\n");
        exit(EXIT_FAILURE);
    }
    return cinfo;
}

void writeHeaders(FILE* file_mlv, struct jpeg_decompress_struct cinfo) {
    writePackHeader(file_mlv, cinfo.output_width, cinfo.output_height, FRAMERATE, BITRATE);
    writeSystemHeader(file_mlv);
}

void doIntraframeCompression(FILE* file_mlv, struct jpeg_decompress_struct cinfo,
                             unsigned char filenames[MAX_FILES][MAX_FILENAME_LEN], int count) {
    uint16_t width = cinfo.output_width;
    uint16_t height = cinfo.output_height;
    int pixel_size = cinfo.output_components;

    printf("Image width: %d, height: %d, pixel size: %d\n", width, height, pixel_size);

    // int bit_rate = ... // Calculate if needed

    // Write sequence header once
    writeSequenceHeader(file_mlv, width, height);

    unsigned char *Y = NULL, *U = NULL, *V = NULL;

    for (int frame_cnt = 0; frame_cnt < count; frame_cnt++) {
        // Reuse Y, U, V for each frame
        if (Y) { free(Y); Y = NULL; }
        if (U) { free(U); U = NULL; }
        if (V) { free(V); V = NULL; }

        // Read and convert image
        struct jpeg_decompress_struct cinfo_frame = readImage(filenames[frame_cnt], &Y, &U, &V);

        if (frame_cnt % GOP_SIZE == 0) {
            writeGOPHeader(file_mlv);
        }

        // Always I-frame for simplicity
        writePictureHeader(file_mlv, I_FRAME, frame_cnt);

        int num_slices = height / BLOCK_SIZE;
        for (int y_block = 0; y_block < num_slices; y_block++) {
            writeSlice(file_mlv, y_block + 1, SCALE_QUANT, Y, U, V, width, height);
        }

        jpeg_destroy_decompress(&cinfo_frame);
    }

    // Free at the end if still allocated
    if (Y) free(Y);
    if (U) free(U);
    if (V) free(V);

    printf("Intraframe compression completed for %d frames.\n", count);
}

int main() {
    struct timeval start, end;
    int count;
    char directory[256];
    int ImgCount;

    // Prompt the user to input the directory path
    printf("Enter the directory path: ");
    fgets(directory, sizeof(directory), stdin);

     // Prompt the user to input the number of images
    printf("Enter the number of images: ");
    if (scanf("%d", &ImgCount) != 1) {
        printf("Invalid input. Please enter a number.\n");
        return 1; // Exit with an error code
    }

    if(ImgCount > 100){
        printf("Too many images");
        return 1;
    }

    // Remove the newline character from fgets input
    directory[strcspn(directory, "\n")] = '\0';

    unsigned char filenames[ImgCount][MAX_FILENAME_LEN];
    
    gettimeofday(&start, NULL);

    count = load_filenames(directory, filenames); 

    FILE* file_mlv = fopen(FILENAME_OUTPUT, "wb");
    unsigned char *Y = NULL, *U = NULL, *V = NULL;
    struct jpeg_decompress_struct cinfo = readImage(filenames[0], &Y, &U, &V);

    // Write headers
    //writeHeaders(file_mlv, cinfo);

    // Pass Y, U, V to doIntraframeCompression
    doIntraframeCompression(file_mlv, cinfo, filenames, count);

    writeEndOfFile(file_mlv);

    // After finishing, free them if allocated
    if (Y) free(Y);
    if (U) free(U);
    if (V) free(V);

    gettimeofday(&end, NULL);
    
    double total_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    printf("Total execution time: %.2f seconds\n", total_time);

    fclose(file_mlv);

    return 0;
}
