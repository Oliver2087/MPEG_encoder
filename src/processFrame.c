#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <jpeglib.h>
#include <limits.h>
#include <string.h>
#include <stdint.h>
#include "processFrame.h"
#include "motionCompEst.h"
#include "huffmancoding.h"

#define GOP_SIZE 8       // Frames per GOP
#define PI 3.1415927
#define B_FRAME_SIZE 2       // Number of B-frames between I and P-frames
#define MIN_BLOCK_SIZE 4

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

// Function to initialize a single frame
void initializeFrame(Frame* frame, FrameType type, int frame_number, Frame* previous_frame, Frame* future_frame, int width, int height) {
    frame->type = type;                     // Set the frame type
    frame->frame_number = frame_number;     // Set the frame number
    frame->previous_frame = previous_frame; // Link to the previous frame
    frame->future_frame = future_frame;     // Link to the future frame
    frame->width = width;
    frame->height = height;

    // Allocate and initialize block data (example size)
    size_t block_size = 1024; // Example block size
    frame->block_data = (unsigned char*)malloc(block_size);
    if (!frame->block_data) {
        fprintf(stderr, "Memory allocation failed for block data\n");
        exit(EXIT_FAILURE);
    }

    // Optionally initialize block_data to zeros
    for (size_t i = 0; i < block_size; i++) {
        frame->block_data[i] = 0;
    }
}

void assignEncodingFrameTypes(GopStruct* current_GOP, int gop_size, int b_frames) {
    for (int i = 0; i < gop_size; i++) {
        if (i == 0) {
            current_GOP->encoding_frame_types[i] = I_FRAME; // I-frame at the start of each GOP
        } else if ((i % gop_size - 1) % (b_frames + 1) == 0) {
            current_GOP->encoding_frame_types[i] = P_FRAME; // P-frame after B-frames
        } else {
            current_GOP->encoding_frame_types[i] = B_FRAME; // B-frame between I- and P-frames
        }
    }
}

void assignDisplayFrameTypes(GopStruct* current_GOP, int gop_size) {
    for (int i = 0; i < gop_size; i++) {
        if (i == 0) {
            current_GOP->display_frame_types[i] = I_FRAME; // I-frame at the start of each GOP
        } else if (i <= 3) {
            current_GOP->display_frame_types[i] = P_FRAME; // P-frames follow the I-frame
        } else {
            current_GOP->display_frame_types[i] = B_FRAME; // B-frames follow the P-frames
        }
    }
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

static void processBlock(
    unsigned char* current_frame,
    unsigned char* predicted_frame,
    int width,
    int height,
    int block_x,
    int block_y,
    unsigned char* bitstream_buffer,
    int* bitstream_index,
    double* previous_dc_coeffi,
    FILE* outfile
) {
    double block[BLOCK_SIZE][BLOCK_SIZE];

    // Compute residual
    for (int i = 0; i < BLOCK_SIZE; i++) {
        for (int j = 0; j < BLOCK_SIZE; j++) {
            int current_pixel = current_frame[(block_y + i) * width + (block_x + j)];
            int predicted_pixel = predicted_frame[(block_y + i) * width + (block_x + j)];
            block[i][j] = (double)(current_pixel - predicted_pixel);
        }
    }

    // Print all block values for debugging
    printf("Block values:\n");
    for (int i = 0; i < 64; i++) {
        printf("%f ", block[i]);
        if ((i + 1) % 8 == 0) {
            printf("\n"); // Print a newline every 8 values for better readability
        }
    }

    performFastDCT(block);
    quantizeBlock(block, quantization_table);

    // Huffman encode this block
    performHuffmanCoding(bitstream_buffer, (double*)block, *previous_dc_coeffi);

    // Write the Huffman-coded bitstream to the file
    int bytes_to_write = (*bitstream_index + 7) / 8;
    fwrite(bitstream_buffer, 1, bytes_to_write, outfile);

    // Prepare for the next block
    memset(bitstream_buffer, 0, bytes_to_write);
    *bitstream_index = 0;
    *previous_dc_coeffi = round(block[0][0]); // Update DC predictor
}

void encodeResiduals(
    unsigned char* current_frame,
    unsigned char* predicted_frame,
    int width,
    int height,
    FILE* outfile
) {
    unsigned char bitstream_buffer[MAX_BITSTREAM_SIZE];
    memset(bitstream_buffer, 0, sizeof(bitstream_buffer));
    bitstream_index = 0;
    double previous_dc_coeffi = 0.0; // DC predictor initialization

    for (int block_y = 0; block_y < height; block_y += BLOCK_SIZE) {
        for (int block_x = 0; block_x < width; block_x += BLOCK_SIZE) {
            processBlock(current_frame, predicted_frame, width, height, block_x, block_y, bitstream_buffer, &bitstream_index, &previous_dc_coeffi, outfile);
        }
    }
}

void performIntraframeCompression(int width, int height, unsigned char* bmp_buffer, int num_blocks, CompressedData* compressed_data) {
    int i_CurrentBlock = 0;
    printf("Processing blocks: 0/%d", num_blocks);
    fflush(stdout);

    int x_block, y_block;

    // Allocate memory for compressed data
    int blocks_per_frame = (width / BLOCK_SIZE) * (height / BLOCK_SIZE);
    compressed_data->data = (double*)malloc(blocks_per_frame * BLOCK_SIZE * BLOCK_SIZE * 3 * sizeof(double));
    if (!compressed_data->data) {
        fprintf(stderr, "Failed to allocate memory for compressed data.\n");
        exit(EXIT_FAILURE);
    }

    double* compressed_ptr = compressed_data->data; // Pointer to track where to write compressed data

    #pragma omp parallel for collapse(2)
    for (y_block = 0; y_block < height / BLOCK_SIZE; y_block++) {
        for (x_block = 0; x_block < width / BLOCK_SIZE; x_block++) {
            i_CurrentBlock++;
            printf("\rProcessing blocks: %d/%d", i_CurrentBlock, num_blocks);
            fflush(stdout);

            double ym[BLOCK_SIZE][BLOCK_SIZE];
            double cbm[BLOCK_SIZE][BLOCK_SIZE];
            double crm[BLOCK_SIZE][BLOCK_SIZE];

            // Extract 8x8 blocks
            for (int y = 0; y < BLOCK_SIZE; y++) {
                for (int x = 0; x < BLOCK_SIZE; x++) {
                    ym[y][x] = bmp_buffer[(y_block * BLOCK_SIZE + y) * width + (x_block * BLOCK_SIZE + x)];
                    cbm[y][x] = bmp_buffer[width * height + (y_block * BLOCK_SIZE + y) * (width / 2) + (x_block * BLOCK_SIZE + x) / 2];
                    crm[y][x] = bmp_buffer[width * height * 5 / 4 + (y_block * BLOCK_SIZE + y) * (width / 2) + (x_block * BLOCK_SIZE + x) / 2];
                }
            }

            // Apply DCT and quantization
            performFastDCT(ym);
            quantizeBlock(ym, quantization_table);

            performFastDCT(cbm);
            quantizeBlock(cbm, quantization_table);

            performFastDCT(crm);
            quantizeBlock(crm, quantization_table);

            // Store compressed data
            #pragma omp critical
            {
                memcpy(compressed_ptr, ym, BLOCK_SIZE * BLOCK_SIZE * sizeof(double));
                compressed_ptr += BLOCK_SIZE * BLOCK_SIZE;
                memcpy(compressed_ptr, cbm, BLOCK_SIZE * BLOCK_SIZE * sizeof(double));
                compressed_ptr += BLOCK_SIZE * BLOCK_SIZE;
                memcpy(compressed_ptr, crm, BLOCK_SIZE * BLOCK_SIZE * sizeof(double));
                compressed_ptr += BLOCK_SIZE * BLOCK_SIZE;
            }
        }
    }
    printf("\n");
}

void processIFrame(Frame *curr_frame) {
    if (curr_frame == NULL) return;

    // Assuming the JPEG struct has been initialized and ready to use
    int width = curr_frame->width;
    int height = curr_frame->height;
    int num_blocks_x = width / BLOCK_SIZE;
    int num_blocks_y = height / BLOCK_SIZE;
    int num_blocks = num_blocks_x * num_blocks_y;

    CompressedData compressed_frame;
    compressed_frame.data = malloc(sizeof(double) * num_blocks * BLOCK_SIZE * BLOCK_SIZE);
    if (compressed_frame.data == NULL) {
        fprintf(stderr, "Memory allocation failed for compressed frame data.\n");
        return;
    }

    performIntraframeCompression(width, height, curr_frame->block_data, num_blocks, &compressed_frame);

    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), OUTPUT_PATH, curr_frame->frame_number);

    FILE *outfile = fopen(output_filename, "wb");
    if (outfile == NULL) {
        perror("Error: Failed to open the output file.");
        free(compressed_frame.data);
        return;
    }

    fwrite(&width, sizeof(int), 1, outfile);
    fwrite(&height, sizeof(int), 1, outfile);
    fwrite(&(int){BLOCK_SIZE}, sizeof(int), 1, outfile);

    // Writing compressed data directly as an example
    fwrite(compressed_frame.data, sizeof(double), num_blocks * BLOCK_SIZE * BLOCK_SIZE, outfile);

    fclose(outfile);
    free(compressed_frame.data);
    printf("Processed I-frame %d successfully and stored to %s.\n", curr_frame->frame_number, output_filename);
}

void processPFrame(Frame *curr_frame) {
    if (curr_frame == NULL || curr_frame->previous_frame == NULL) {
        fprintf(stderr, "Invalid frame data.\n");
        return;
    }

    int width = curr_frame->width;
    int height = curr_frame->height;
    int search_range = 16; // Search range for motion estimation

    int num_blocks_x = width / BLOCK_SIZE;
    int num_blocks_y = height / BLOCK_SIZE;
    int total_blocks = num_blocks_x * num_blocks_y;

    // Allocate memory for motion vectors
    MotionVector *motion_vectors = (MotionVector *)malloc(total_blocks * sizeof(MotionVector));
    if (motion_vectors == NULL) {
        fprintf(stderr, "Failed to allocate memory for motion vectors.\n");
        return;
    }

    // Allocate memory for the predicted frame
    unsigned char *predicted_frame = (unsigned char *)malloc(width * height);
    if (predicted_frame == NULL) {
        fprintf(stderr, "Failed to allocate memory for predicted frame.\n");
        free(motion_vectors);
        return;
    }

    // Perform motion estimation and compensation
    performMotionEstimation(curr_frame->block_data, curr_frame->previous_frame->block_data, width, height, BLOCK_SIZE, search_range, motion_vectors);
    performMotionCompensation(curr_frame->previous_frame->block_data, predicted_frame, width, height, motion_vectors, BLOCK_SIZE);

    // Open output file
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), OUTPUT_PATH, curr_frame->frame_number);
    FILE *outfile = fopen(output_filename, "wb");
    if (outfile == NULL) {
        fprintf(stderr, "Error: Failed to open file %s for writing.\n", output_filename);
        free(motion_vectors);
        free(predicted_frame);
        return;
    }

    // Write frame metadata and motion vectors
    fwrite(&width, sizeof(int), 1, outfile);
    fwrite(&height, sizeof(int), 1, outfile);
    fwrite(&(int){BLOCK_SIZE}, sizeof(int), 1, outfile);
    fwrite(motion_vectors, sizeof(MotionVector), total_blocks, outfile);

    // Encode and write residuals
    encodeResiduals(curr_frame->block_data, predicted_frame, width, height, outfile);

    // Close the file and clean up
    fclose(outfile);
    free(motion_vectors);
    free(predicted_frame);

    printf("Processed and saved P-frame %d successfully.\n", curr_frame->frame_number);
}


// Function prototype (placeholders)
extern void performBidirectionalEstimation(
    unsigned char* current_frame,
    unsigned char* previous_frame,
    unsigned char* next_frame,
    int width,
    int height,
    int block_size,
    int search_range,
    MotionVector* mv_backward_array,
    MotionVector* mv_forward_array
);

extern void performBidirectionalCompensation(
    unsigned char* previous_frame,
    unsigned char* next_frame,
    unsigned char* predicted_frame,
    int width,
    int height,
    int block_size,
    MotionVector* mv_backward_array,
    MotionVector* mv_forward_array
);

void processBFrame(Frame *curr_frame) {
    int width = curr_frame->width;
    int height = curr_frame->height;
    int search_range = 16; // Search range for motion estimation

    int num_blocks_x = width / BLOCK_SIZE;
    int num_blocks_y = height / BLOCK_SIZE;
    int total_blocks = num_blocks_x * num_blocks_y;

    // Allocate memory for forward and backward motion vectors
    MotionVector* mv_backward_array = (MotionVector*)malloc(total_blocks * sizeof(MotionVector));
    if (!mv_backward_array) {
        fprintf(stderr, "Failed to allocate memory for backward motion vectors.\n");
        exit(EXIT_FAILURE);
    }

    MotionVector* mv_forward_array = (MotionVector*)malloc(total_blocks * sizeof(MotionVector));
    if (!mv_forward_array) {
        fprintf(stderr, "Failed to allocate memory for forward motion vectors.\n");
        free(mv_backward_array);
        exit(EXIT_FAILURE);
    }

    // Allocate memory for the predicted frame
    unsigned char* predicted_frame = (unsigned char*)malloc(width * height);
    if (!predicted_frame) {
        fprintf(stderr, "Failed to allocate memory for predicted frame.\n");
        free(mv_backward_array);
        free(mv_forward_array);
        exit(EXIT_FAILURE);
    }

    // Perform bidirectional motion estimation
    // This finds mv_backward (relative to previous_frame) and mv_forward (relative to next_frame)
    performBidirectionalEstimation(
        curr_frame->block_data,
        curr_frame->previous_frame->block_data,
        curr_frame->future_frame->block_data,
        width,
        height,
        BLOCK_SIZE,
        search_range,
        mv_backward_array,
        mv_forward_array
    );

    // Perform bidirectional motion compensation
    // This uses both sets of vectors to form a predicted B-frame by averaging predictions
    performBidirectionalCompensation(
        curr_frame->previous_frame->block_data,
        curr_frame->future_frame->block_data,
        predicted_frame,
        width,
        height,
        BLOCK_SIZE,
        mv_backward_array,
        mv_forward_array
    );

    // Open output file
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), OUTPUT_PATH, curr_frame->frame_number);
    FILE* outfile = fopen(output_filename, "wb");
    if (!outfile) {
        fprintf(stderr, "Error: Failed to open file %s for writing.\n", output_filename);
        free(mv_backward_array);
        free(mv_forward_array);
        free(predicted_frame);
        exit(EXIT_FAILURE);
    }

    // Write frame metadata (width, height, block size)
    fwrite(&width, sizeof(int), 1, outfile);
    fwrite(&height, sizeof(int), 1, outfile);
    fwrite(&(int){BLOCK_SIZE}, sizeof(int), 1, outfile);

    // Write motion vectors
    // First, write backward motion vectors
    fwrite(mv_backward_array, sizeof(MotionVector), total_blocks, outfile);
    // Then, write forward motion vectors
    fwrite(mv_forward_array, sizeof(MotionVector), total_blocks, outfile);

    // Encode residuals (current_frame - predicted_frame)
    // The encodeResiduals() function should handle DCT, quantization, Huffman coding, etc.
    // Just like in the P-frame code, we do:
    encodeResiduals(curr_frame->block_data, predicted_frame, width, height, outfile);

    // Close the file
    fclose(outfile);

    // Clean up
    free(mv_backward_array);
    free(mv_forward_array);
    free(predicted_frame);

    printf("Processed and saved B-frame %d successfully.\n", curr_frame->frame_number);
}

void processGOP(GopStruct* curr_GOP) {
    if (curr_GOP == NULL) {
        fprintf(stderr, "Invalid GOP structure.\n");
        return;
    }

    // Loop through each frame in the GOP up to GOP_SIZE - 1
    for (int i = 0; i < GOP_SIZE; i++) {
        if (curr_GOP->frames[i].type == I_FRAME) {
            processIFrame(&curr_GOP->frames[i]);  // Pass pointer to the frame
        } else if (curr_GOP->frames[i].type == P_FRAME) {
            processPFrame(&curr_GOP->frames[i]);  // Pass pointer to the frame
        } else if (curr_GOP->frames[i].type == B_FRAME) {
            processBFrame(&curr_GOP->frames[i]);  // Pass pointer to the frame
        }
    }
}
