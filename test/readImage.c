// gcc -o readImage readImage.c -ljpeg
// ./readImage

#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <jpeglib.h>

#define INPUTFILENAME "../../data/Video1/Images/image0005.jpeg"
#define OUTPUTFILENAME "../../data/Image5.bin"
#define BLOCK_SIZE 16
#define PI 3.1415927
#define NUMOFLINESREADINONETIME 16

int decompressJPEG(struct jpeg_decompress_struct* cinfo_p, unsigned char** bmp_buffer, FILE* infile) {
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
    return 0;
}

void performFastDCT(double block[BLOCK_SIZE][BLOCK_SIZE]) {
    double temp[BLOCK_SIZE][BLOCK_SIZE];
    double cu, cv;

    // 1D DCT on rows
    for(int u = 0; u < BLOCK_SIZE; u++) {
        for(int x = 0; x < BLOCK_SIZE; x++) {
            temp[u][x] = 0;
            cu = (u == 0) ? 1.0 / sqrt(2.0) : 1.0;
            for(int i = 0; i < BLOCK_SIZE; i++) {
                temp[u][x] += block[u][i] * cos((2.0 * i + 1.0) * x * PI / (2.0 * BLOCK_SIZE));
            }
            temp[u][x] *= 0.5 * cu;
        }
    }

    // 1D DCT on columns
    for(int v = 0; v < BLOCK_SIZE; v++) {
        for(int y = 0; y < BLOCK_SIZE; y++) {
            block[y][v] = 0;
            cv = (v == 0) ? 1.0 / sqrt(2.0) : 1.0;
            for(int j = 0; j < BLOCK_SIZE; j++) {
                block[y][v] += temp[j][v] * cos((2.0 * j + 1.0) * y * PI / (2.0 * BLOCK_SIZE));
            }
            block[y][v] *= 0.5 * cv;
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

void quantizeBlock(double block[BLOCK_SIZE][BLOCK_SIZE], const unsigned char quant_table[BLOCK_SIZE][BLOCK_SIZE]) {
    for(int i = 0; i < BLOCK_SIZE; i++) {
        for(int j = 0; j < BLOCK_SIZE; j++) {
            block[i][j] = round(block[i][j] / quant_table[i][j]);
        }
    }
}

int performIntraframCompression(int width, int height, unsigned char* bmp_buffer, int num_blocks) {
    // Establish the dynamic display
    int i_CurrentBlock = 0;
    printf("Processing blocks: 0/%d", num_blocks);
    fflush(stdout); // Ensure the output is flushed to the terminal

    // Process each 8x8 block in Y, Cb, and Cr
    int x_block, y_block; // the index of blocks

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

    #pragma omp parallel for collapse(2) // Parallelize two nested loops

    for(y_block = 0; y_block < height / BLOCK_SIZE; y_block++) {
        for(x_block = 0; x_block < width / BLOCK_SIZE; x_block++) {
            i_CurrentBlock++;
            // Dynamic progress update
            printf("\rProcessing blocks: %d/%d", i_CurrentBlock, num_blocks);
            fflush(stdout);
            // Extract 8x8 block for Y, Cb, and Cr
            double ym[BLOCK_SIZE][BLOCK_SIZE]; // the Y matrix for this block
            double cbm[BLOCK_SIZE][BLOCK_SIZE]; // the CB matrix for this block
            double crm[BLOCK_SIZE][BLOCK_SIZE]; // the CR matrix for this block
            // Copy Y, Cb, Cr values into blocks (this assumes YCbCr format)
            for(int y = 0; y < BLOCK_SIZE; y++) {
                for(int x = 0; x < BLOCK_SIZE; x++) {
                    ym[y][x] = bmp_buffer[
                        (y_block * BLOCK_SIZE + y) * width + 
                        (x_block * BLOCK_SIZE + x)
                    ];
                    cbm[y][x] = bmp_buffer[
                        width * height + 
                        (y_block * BLOCK_SIZE + y) * (width / 2) + 
                        (x_block * BLOCK_SIZE + x) / 2
                    ];         // CB and CR matrix is after the Y matrix, 
                    crm[y][x] = bmp_buffer[
                        width * height * 5 / 4 + 
                        (y_block * BLOCK_SIZE + y) * (width / 2) + 
                        (x_block * BLOCK_SIZE + x) / 2
                    ]; // in YUV420 the C matrices has half height and width as Y matrix
                }
            }

            // Apply DCT, quatization and Huffman encoding to the blocks
            performFastDCT(ym);
            quantizeBlock(ym, quantization_table);
            performFastDCT(cbm);
            quantizeBlock(cbm, quantization_table);
            performFastDCT(crm);
            quantizeBlock(crm, quantization_table);
        }
    }
    printf("\n");
}

void readJPEGFile(const char *filename) {
    FILE* infile = fopen(filename, "rb");
    if(!infile) {
        fprintf(stderr, "Error opening JPEG file %s!\n", filename);
        exit(EXIT_FAILURE);
    }

    // decompress the image and get the information
    struct jpeg_decompress_struct cinfo;
    unsigned char* bmp_buffer = NULL;
    decompressJPEG(&cinfo, &bmp_buffer, infile);

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
    int num_blocks_x = width / BLOCK_SIZE;    // Number of blocks in X direction
    int num_blocks_y = height / BLOCK_SIZE;   // Number of blocks in Y direction
    int num_blocks = num_blocks_x * num_blocks_y;
    // 图片数据已在 bmp_buffer 中，可进一步处理
    printf("Image width: %d, height: %d, pixel size: %d\n", width, height, pixel_size);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    performIntraframCompression(width, height, bmp_buffer, num_blocks);

    FILE* outfile = fopen(OUTPUTFILENAME, "wb");
    if(!outfile) {
        perror("Error: Failed to open the output file.");
        exit(EXIT_FAILURE);
    }

    fwrite(bmp_buffer, sizeof(unsigned char), width * height * pixel_size, outfile);
    fclose(outfile);

    free(bmp_buffer); // 处理完后释放内存
}

int main() {
    struct timeval start, end;
    
    gettimeofday(&start, NULL);

    readJPEGFile(INPUTFILENAME);

    gettimeofday(&end, NULL);
    
    double total_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    printf("Total execution time: %.2f seconds\n", total_time);

    return 0;
}
