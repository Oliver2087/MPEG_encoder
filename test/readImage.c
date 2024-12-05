#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <jpeglib.h>

#include <huffmancoding.h>

#define INPUTFILENAME "../../data/Video1/Images/image0005.jpeg"
#define FILENAME_OUTPUT "output.mpeg"
#define BLOCK_SIZE 8
#define PI 3.1415927
#define NUMOFLINESREADINONETIME 16

const unsigned char quantization_table[BLOCK_SIZE][BLOCK_SIZE] = {
    {16, 11, 10, 16, 24, 40, 51, 61},
    {12, 12, 14, 19, 26, 58, 60, 55},
    {14, 13, 16, 24, 40, 57, 69, 56},
    {14, 17, 22, 29, 51, 87, 80, 62},
    {18, 22, 37, 56, 68, 109, 103, 77},
    {24, 35, 55, 64, 81, 104, 113, 92},
    {49, 64, 78, 87, 103, 121, 120, 101},
    {72, 92, 95, 98, 112, 100, 103, 99}
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

void quantizeBlock(double block[BLOCK_SIZE*BLOCK_SIZE]) {
    for(int i = 0; i < BLOCK_SIZE; i++) {
        for(int j = 0; j < BLOCK_SIZE; j++) {
            block[i*BLOCK_SIZE+j] = round(block[i*BLOCK_SIZE+j] / quantization_table[i][j]);
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

    // Write Sequence Header to file
    fwrite(sequence_header, 1, sizeof(sequence_header), file_mpeg);
}

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

    int width = cinfo.output_width;
    int height = cinfo.output_height;
    int pixel_size = cinfo.output_components;
    // 图片数据已在 bmp_buffer 中，可进一步处理
    printf("Image width: %d, height: %d, pixel size: %d\n", width, height, pixel_size);
    jpeg_destroy_decompress(&cinfo);

    FILE* file_mpeg = fopen(filename_o, "wb");
    writeSequenceHeader(file_mpeg, width, height, 5, calculateBitRate(width, height, 30));
    // Write picture header
    fwrite(picture_header, 1, sizeof(picture_header), file_mpeg);

    // Establish the dynamic display
    // int i_CurrentBlock = 0;
    // printf("Processing blocks: 0/%d", num_blocks);
    // fflush(stdout); // Ensure the output is flushed to the terminal

    // Process each 8x8 block in Y, Cb, and Cr
    double prev_dc_coeffi_y = 0.0;
    double prev_dc_coeffi_cb = 0.0;
    double prev_dc_coeffi_cr = 0.0;
    #pragma omp parallel for collapse(2) // Parallelize two nested loops
    for(int y_block = 0; y_block < height / BLOCK_SIZE; y_block++) {
        for(int x_block = 0; x_block < width / BLOCK_SIZE; x_block++) {
            // i_CurrentBlock++;
            // Dynamic progress update
            // printf("\rProcessing blocks: %d/%d", i_CurrentBlock, num_blocks);
            // fflush(stdout);

            // Use 1D arrays instead of 2D arrays
            double ym[BLOCK_SIZE * BLOCK_SIZE];  // Y matrix for this block
            double cbm[BLOCK_SIZE * BLOCK_SIZE]; // Cb matrix for this block
            double crm[BLOCK_SIZE * BLOCK_SIZE]; // Cr matrix for this block

            // Copy Y, Cb, Cr values into 1D arrays (this assumes YCbCr format)
            for(int y = 0; y < BLOCK_SIZE; y++) {
                for(int x = 0; x < BLOCK_SIZE; x++) {
                    ym[y * BLOCK_SIZE + x] = bmp_buffer[ 
                        (y_block * BLOCK_SIZE + y) * width + 
                        (x_block * BLOCK_SIZE + x)
                    ];

                    cbm[y * BLOCK_SIZE + x] = bmp_buffer[ 
                        width * height + 
                        (y_block * BLOCK_SIZE + y) * (width / 2) + 
                        (x_block * BLOCK_SIZE + x) / 2
                    ];

                    crm[y * BLOCK_SIZE + x] = bmp_buffer[ 
                        width * height * 5 / 4 + 
                        (y_block * BLOCK_SIZE + y) * (width / 2) + 
                        (x_block * BLOCK_SIZE + x) / 2
                    ];
                }
            }

            // Apply DCT, quantization and Huffman encoding to the blocks
            performFastDCT(ym);
            quantizeBlock(ym);
            performFastDCT(cbm);
            quantizeBlock(cbm);
            performFastDCT(crm);
            quantizeBlock(crm);

            prev_dc_coeffi_y = ym[0]; // First element (DC coefficient for Y)
            prev_dc_coeffi_cb = cbm[0]; // First element (DC coefficient for Cb)
            prev_dc_coeffi_cr = crm[0]; // First element (DC coefficient for Cr)
        }
    }
    fclose(file_mpeg);
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
