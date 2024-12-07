#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <jpeglib.h>

#include <huffmancoding.h>

<<<<<<< HEAD
#define INPUTFILENAME "../../data/images/input.jpg"
=======
#define INPUTFILENAME "../../data/Video1/Images/image0005.jpeg"
>>>>>>> refs/remotes/origin/main
#define FILENAME_OUTPUT "output.mpeg"
#define BLOCK_SIZE 8
#define PI 3.1415927
#define NUMOFLINESREADINONETIME 16
#define FRAMERATE 10
#define SCALE_QUANT 23

<<<<<<< HEAD
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
=======
const unsigned char quantization_table[BLOCK_SIZE][BLOCK_SIZE] = {
    {16, 11, 10, 16, 24, 40, 51, 61},
    {12, 12, 14, 19, 26, 58, 60, 55},
    {14, 13, 16, 24, 40, 57, 69, 56},
    {14, 17, 22, 29, 51, 87, 80, 62},
    {18, 22, 37, 56, 68, 109, 103, 77},
    {24, 35, 55, 64, 81, 104, 113, 92},
    {49, 64, 78, 87, 103, 121, 120, 101},
    {72, 92, 95, 98, 112, 100, 103, 99}
>>>>>>> refs/remotes/origin/main
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

<<<<<<< HEAD
// scale from 0 to 51
void quantizeBlock(double block[BLOCK_SIZE*BLOCK_SIZE], const unsigned char* quantization_table, uint8_t scale) {
    for(int i = 0; i < BLOCK_SIZE; i++) {
        for(int j = 0; j < BLOCK_SIZE; j++) {
            block[i*BLOCK_SIZE+j] = round(block[i*BLOCK_SIZE+j] / (scale * quantization_table[i * BLOCK_SIZE + j]));
=======
void quantizeBlock(double block[BLOCK_SIZE*BLOCK_SIZE]) {
    for(int i = 0; i < BLOCK_SIZE; i++) {
        for(int j = 0; j < BLOCK_SIZE; j++) {
            block[i*BLOCK_SIZE+j] = round(block[i*BLOCK_SIZE+j] / quantization_table[i][j]);
>>>>>>> refs/remotes/origin/main
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
<<<<<<< HEAD
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

=======
void writeSequenceHeader(FILE* file_mpeg, int width, int height, int frame_rate_code, int bit_rate) {
    unsigned char sequence_header[12];

    // Start code for Sequence Header
    sequence_header[0] = 0x00;
    sequence_header[1] = 0x00;
    sequence_header[2] = 0x01;
    sequence_header[3] = 0xB3;

    // Resolution (12 bits for horizontal, 12 bits for vertical)
    sequence_header[4] = (width >> 4) & 0xFF;                   // Higher 8 bits of horizontal size
    sequence_header[5] = ((width & 0x0F) << 4) | ((height >> 8) & 0x0F); // Lower 4 bits of horizontal and higher 4 bits of vertical
    sequence_header[6] = height & 0xFF;                         // Lower 8 bits of vertical size

    // Frame rate and bit rate
    sequence_header[7] = (bit_rate >> 10) & 0xFF;               // Higher 8 bits of bit rate
    sequence_header[8] = (bit_rate >> 2) & 0xFF;                // Middle 8 bits of bit rate
    sequence_header[9] = ((bit_rate & 0x03) << 6) | 0x3F;       // Lower 2 bits of bit rate and VBV buffer size

    // Frame rate code and marker bits
    sequence_header[10] = (frame_rate_code & 0x0F) << 4;        // Frame rate code (4 bits)
    sequence_header[11] = 0xFF;                                 // Marker bits

>>>>>>> refs/remotes/origin/main
    // Write Sequence Header to file
    fwrite(sequence_header, 1, sizeof(sequence_header), file_mpeg);
}

<<<<<<< HEAD
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

    sequence_header[4] = width >> 1;
    sequence_header[5] = ((width & 0xF) << 1) | (height >> 2);
    sequence_header[6] = height & 0xFF;

    sequence_header[7] = 0x15;
    sequence_header[8] = 0xFF;
    sequence_header[9] = 0xFF;
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
=======
// Calculate the bitrate based on an empirical formula (unit: 400Kbps)
int calculateBitRate(int width, int height, int frame_rate) {
    // rate_factor is an empirical parameter, typically between 0.1 and 0.3
    double rate_factor = 0.2; // Adjust this parameter according to the desired quality
    int bit_rate = (int)(width * height * frame_rate * rate_factor / 1000 / 400);
    return bit_rate; // Return the bitrate in Kbps
}

unsigned char picture_header[] = {
    0x00, 0x00, 0x01, 0x00,  // Start code for picture header
    0x00, 0x01,              // Temporal reference
    0x80                     // Picture type (I-frame)
};
>>>>>>> refs/remotes/origin/main

void doIntraframeCompression(char* filename_o, char* filename_i) {
    // decompress the image and get the information
    struct jpeg_decompress_struct cinfo;
    unsigned char* bmp_buffer = NULL;
    decompressJPEG(&cinfo, &bmp_buffer, filename_i);

    // Check whether the data is in yuv
    printf("Number of components: %d\n", cinfo.num_components); // Check the number of th components.
    if(cinfo.jpeg_color_space == JCS_YCbCr) {
        printf("The image is in YUV (YCbCr) format.\n");
    }else if(cinfo.jpeg_color_space == JCS_RGB) {
        perror("The image is in RGB format.\n");
        exit(EXIT_FAILURE);
    }else {
        perror("The image is in an unsupported color space.\n");
        exit(EXIT_FAILURE);
    }

    uint16_t width = cinfo.output_width;
    uint16_t height = cinfo.output_height;
    int pixel_size = cinfo.output_components;
    // 图片数据已在 bmp_buffer 中，可进一步处理
    printf("Image width: %d, height: %d, pixel size: %d\n", width, height, pixel_size);
    jpeg_destroy_decompress(&cinfo);

<<<<<<< HEAD
    int bit_rate = width * height * FRAMERATE * 0.2;

    FILE* file_mlv = fopen(filename_o, "wb");
    writeSequenceHeader(file_mlv, width, height);
    writeGOPHeader(file_mlv);
    writePictureHeader(file_mlv);

    // writePackHeader(file_mpeg, width, height, 10, (uint32_t)(bit_rate/400));
    

    // Establish the dynamic display
    int i_CurrentBlock = 0;
    printf("Processing blocks: 0/%d", 10000);
    fflush(stdout); // Ensure the output is flushed to the terminal
=======
    FILE* file_mpeg = fopen(filename_o, "wb");
    writeSequenceHeader(file_mpeg, width, height, 5, calculateBitRate(width, height, 30));
    // Write picture header
    fwrite(picture_header, 1, sizeof(picture_header), file_mpeg);

    // Establish the dynamic display
    // int i_CurrentBlock = 0;
    // printf("Processing blocks: 0/%d", num_blocks);
    // fflush(stdout); // Ensure the output is flushed to the terminal
>>>>>>> refs/remotes/origin/main

    // Process each 8x8 block in Y, Cb, and Cr
    double prev_dc_coeffi_y = 0.0;
    double prev_dc_coeffi_cb = 0.0;
    double prev_dc_coeffi_cr = 0.0;
<<<<<<< HEAD
    uint8_t* bitstream_buffer = (uint8_t*)malloc(MAX_BITSTREAM_SIZE * 3 * sizeof(uint8_t));
    int bitstream_index = 0;
    size_t length_buffer = 0;
    //#pragma omp parallel for collapse(2) // Parallelize two nested loops
    for(int y_block = 0; y_block < height / BLOCK_SIZE; y_block++) { //height / BLOCK_SIZE
        writeSliceHeader(file_mlv, y_block, SCALE_QUANT);
        for(int x_block = 0; x_block < width / BLOCK_SIZE; x_block++) { // width / BLOCK_SIZE
            i_CurrentBlock++;
            // Dynamic progress update
            printf("\rProcessing blocks: %d/%d", i_CurrentBlock, 10000);
            fflush(stdout);

            // Use 1D arrays instead of 2D arrays
            double ym[4][BLOCK_SIZE * BLOCK_SIZE];  // Y matrix for this block
=======
    #pragma omp parallel for collapse(2) // Parallelize two nested loops
    for(int y_block = 0; y_block < height / BLOCK_SIZE; y_block++) {
        for(int x_block = 0; x_block < width / BLOCK_SIZE; x_block++) {
            // i_CurrentBlock++;
            // Dynamic progress update
            // printf("\rProcessing blocks: %d/%d", i_CurrentBlock, num_blocks);
            // fflush(stdout);

            // Use 1D arrays instead of 2D arrays
            double ym[BLOCK_SIZE * BLOCK_SIZE];  // Y matrix for this block
>>>>>>> refs/remotes/origin/main
            double cbm[BLOCK_SIZE * BLOCK_SIZE]; // Cb matrix for this block
            double crm[BLOCK_SIZE * BLOCK_SIZE]; // Cr matrix for this block

            // Copy Y, Cb, Cr values into 1D arrays (this assumes YCbCr format)
            for(int y = 0; y < BLOCK_SIZE; y++) {
                for(int x = 0; x < BLOCK_SIZE; x++) {
<<<<<<< HEAD
                    ym[0][y * BLOCK_SIZE + x] = bmp_buffer[ 
                        (y_block * BLOCK_SIZE * 2 + y) * width + 
                        (x_block * BLOCK_SIZE * 2 + x)
                    ];

                    ym[1][y * BLOCK_SIZE + x] = bmp_buffer[ 
                        (y_block * BLOCK_SIZE * 2 + y) * width + 
                        (x_block * BLOCK_SIZE * 2 + BLOCK_SIZE + x)
                    ];

                    ym[2][y * BLOCK_SIZE + x] = bmp_buffer[ 
                        (y_block * BLOCK_SIZE * 2 + BLOCK_SIZE + y) * width + 
                        (x_block * BLOCK_SIZE * 2 + x)
                    ];

                    ym[3][y * BLOCK_SIZE + x] = bmp_buffer[ 
                        (y_block * BLOCK_SIZE * 2 + BLOCK_SIZE + y) * width + 
                        (x_block * BLOCK_SIZE * 2 + BLOCK_SIZE + x)
=======
                    ym[y * BLOCK_SIZE + x] = bmp_buffer[ 
                        (y_block * BLOCK_SIZE + y) * width + 
                        (x_block * BLOCK_SIZE + x)
>>>>>>> refs/remotes/origin/main
                    ];

                    cbm[y * BLOCK_SIZE + x] = bmp_buffer[ 
                        width * height + 
<<<<<<< HEAD
                        (y_block * BLOCK_SIZE + y) * width + 
                        (x_block * BLOCK_SIZE + x)
=======
                        (y_block * BLOCK_SIZE + y) * (width / 2) + 
                        (x_block * BLOCK_SIZE + x) / 2
>>>>>>> refs/remotes/origin/main
                    ];

                    crm[y * BLOCK_SIZE + x] = bmp_buffer[ 
                        width * height * 5 / 4 + 
<<<<<<< HEAD
                        (y_block * BLOCK_SIZE + y) * width + 
                        (x_block * BLOCK_SIZE + x)
=======
                        (y_block * BLOCK_SIZE + y) * (width / 2) + 
                        (x_block * BLOCK_SIZE + x) / 2
>>>>>>> refs/remotes/origin/main
                    ];
                }
            }

            // Apply DCT, quantization and Huffman encoding to the blocks
<<<<<<< HEAD
            for(int i = 0; i < 4; i++) {
                performFastDCT(ym[i]);
                quantizeBlock(ym[i], quantization_table_y, SCALE_QUANT);
                performHuffmanCoding(bitstream_buffer, &bitstream_index, ym[i], prev_dc_coeffi_y);
                fwrite(bitstream_buffer, sizeof(uint8_t), bitstream_index, file_mlv);
                prev_dc_coeffi_y = ym[i][0]; // First element (DC coefficient for Y)
            }
            performFastDCT(cbm);
            quantizeBlock(cbm, quantization_table_c, SCALE_QUANT);
            performHuffmanCoding(bitstream_buffer, &bitstream_index, cbm, prev_dc_coeffi_cb);
            fwrite(bitstream_buffer, sizeof(uint8_t), bitstream_index, file_mlv);
            performFastDCT(crm);
            quantizeBlock(crm, quantization_table_c, SCALE_QUANT);
            performHuffmanCoding(bitstream_buffer, &bitstream_index, crm, prev_dc_coeffi_cr);
            fwrite(bitstream_buffer, sizeof(uint8_t), bitstream_index, file_mlv);
=======
            performFastDCT(ym);
            quantizeBlock(ym);
            performFastDCT(cbm);
            quantizeBlock(cbm);
            performFastDCT(crm);
            quantizeBlock(crm);

            prev_dc_coeffi_y = ym[0]; // First element (DC coefficient for Y)
>>>>>>> refs/remotes/origin/main
            prev_dc_coeffi_cb = cbm[0]; // First element (DC coefficient for Cb)
            prev_dc_coeffi_cr = crm[0]; // First element (DC coefficient for Cr)
        }
    }
<<<<<<< HEAD
    fclose(file_mlv);
=======
    fclose(file_mpeg);
>>>>>>> refs/remotes/origin/main
}

int main() {
    struct timeval start, end;
    
    gettimeofday(&start, NULL);

    doIntraframeCompression(FILENAME_OUTPUT, INPUTFILENAME);

    gettimeofday(&end, NULL);
    
    double total_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    printf("Total execution time: %.2f seconds\n", total_time);

    return 0;
}
