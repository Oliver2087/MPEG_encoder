#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <jpeglib.h>
#include <limits.h>
#include <string.h>
#include "processFrame.h"
#include "motionCompEst.h"

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

void assignFrameTypes(FrameType* frame_types, int total_frames, int gop_size, int b_frames) {
    for (int i = 0; i < total_frames; i++) {
        if (i % gop_size == 0) {
            frame_types[i] = I_FRAME; // I-frame at the start of each GOP
        } else if ((i % gop_size - 1) % (b_frames + 1) == 0) {
            frame_types[i] = P_FRAME; // P-frame after B-frames
        } else {
            frame_types[i] = B_FRAME; // B-frame between I- and P-frames
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

void performIntraframCompression(int width, int height, unsigned char* bmp_buffer, int num_blocks, CompressedData* compressed_data) {
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

void processIFrame(struct jpeg_decompress_struct cinfo, unsigned char* bmp_buffer, int frame_index) {
    int width = cinfo.output_width;
    int height = cinfo.output_height;
    int pixel_size = cinfo.output_components;
    int num_blocks_x = width / BLOCK_SIZE;
    int num_blocks_y = height / BLOCK_SIZE;
    int num_blocks = num_blocks_x * num_blocks_y;

    CompressedData compressed_frame;
    performIntraframCompression(width, height, bmp_buffer, num_blocks, &compressed_frame);

    // Generate dynamic filename for output
    char output_filename[256];
    snprintf(output_filename, sizeof(output_filename), "../include/Image%d.bin", frame_index);

    FILE* outfile = fopen(output_filename, "wb");
    if (!outfile) {
        perror("Error: Failed to open the output file.");
        exit(EXIT_FAILURE);
    }

    fwrite(bmp_buffer, sizeof(unsigned char), width * height * pixel_size, outfile);
    fclose(outfile);

    // Free compressed data
    free(compressed_frame.data);

    printf("Processed I-frame %d successfully.\n", frame_index);
}

void processPFrame(struct jpeg_decompress_struct cinfo, unsigned char* current_frame, unsigned char* reference_frame, int frame_index) {
    int width = cinfo.output_width;
    int height = cinfo.output_height;
    int block_size = 16; // Macroblock size
    int search_range = 16; // Search range for motion estimation

    int num_blocks_x = width / block_size;
    int num_blocks_y = height / block_size;
    int total_blocks = num_blocks_x * num_blocks_y;

    // Allocate memory for motion vectors
    MotionVector* motion_vectors = (MotionVector*)malloc(total_blocks * sizeof(MotionVector));
    if (!motion_vectors) {
        fprintf(stderr, "Failed to allocate memory for motion vectors.\n");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for the predicted frame
    unsigned char* predicted_frame = (unsigned char*)malloc(width * height);
    if (!predicted_frame) {
        fprintf(stderr, "Failed to allocate memory for predicted frame.\n");
        free(motion_vectors);
        exit(EXIT_FAILURE);
    }

    // Parallel motion estimation using OpenMP
    #pragma omp parallel for collapse(2)
    for (int block_y = 0; block_y < height; block_y += block_size) {
        for (int block_x = 0; block_x < width; block_x += block_size) {
            int block_index = (block_y / block_size) * num_blocks_x + (block_x / block_size);
            motion_vectors[block_index] = findMotionVector(
                current_frame, reference_frame, width, height, block_x, block_y, block_size, search_range
            );
        }
    }

    // Perform motion compensation to generate the predicted frame
    motionCompensation(reference_frame, predicted_frame, width, height, motion_vectors, block_size);

    // Compute the residual (current_frame - predicted_frame) and process it
    #pragma omp parallel for collapse(2)
    for (int block_y = 0; block_y < height; block_y += block_size) {
        for (int block_x = 0; block_x < width; block_x += block_size) {
            double block[16][16];
            for (int i = 0; i < block_size; i++) {
                for (int j = 0; j < block_size; j++) {
                    int current_pixel = current_frame[(block_y + i) * width + (block_x + j)];
                    int predicted_pixel = predicted_frame[(block_y + i) * width + (block_x + j)];
                    block[i][j] = current_pixel - predicted_pixel;
                }
            }
            performFastDCT(block);
            quantizeBlock(block, quantization_table);
        }
    }

    // Clean up
    free(motion_vectors);
    free(predicted_frame);

    printf("Processed P-frame %d successfully.\n", frame_index);
}

void computeBlockMotion(
    unsigned char* current, 
    unsigned char* reference, 
    MotionVector* motion_vectors, 
    int width, 
    int height, 
    Block block, 
    int search_range) {

    MotionVector best_vector = {0, 0};
    int min_sad = INT_MAX;

    for (int dy = -search_range; dy <= search_range; dy++) {
        for (int dx = -search_range; dx <= search_range; dx++) {
            int ref_x = block.x + dx;
            int ref_y = block.y + dy;

            if (ref_x < 0 || ref_y < 0 || ref_x + block.width > width || ref_y + block.height > height) {
                continue;
            }

            int sad = 0;
            for (int y = 0; y < block.height; y++) {
                for (int x = 0; x < block.width; x++) {
                    int current_pixel = current[(block.y + y) * width + (block.x + x)];
                    int reference_pixel = reference[(ref_y + y) * width + (ref_x + x)];
                    sad += abs(current_pixel - reference_pixel);
                }
            }

            if (sad < min_sad) {
                min_sad = sad;
                best_vector.dx = dx;
                best_vector.dy = dy;
            }
        }
    }

    int block_index = (block.y / block.height) * (width / block.width) + (block.x / block.width);
    motion_vectors[block_index] = best_vector;
}

void processBFrame(
    struct jpeg_decompress_struct cinfo,
    unsigned char* current_frame,
    unsigned char* reference_frame,
    unsigned char* future_frame,
    int frame_index) {

    int width = cinfo.output_width;
    int height = cinfo.output_height;
    int block_size = 16; // Macroblock size
    int search_range = 16; // Search range for motion estimation

    int num_blocks_x = width / block_size;
    int num_blocks_y = height / block_size;
    int total_blocks = num_blocks_x * num_blocks_y;

    // Allocate memory for forward and backward motion vectors
    MotionVector* forward_motion_vectors = (MotionVector*)malloc(total_blocks * sizeof(MotionVector));
    MotionVector* backward_motion_vectors = (MotionVector*)malloc(total_blocks * sizeof(MotionVector));
    if (!forward_motion_vectors || !backward_motion_vectors) {
        fprintf(stderr, "Failed to allocate memory for motion vectors.\n");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for forward and backward predicted frames
    unsigned char* forward_predicted_frame = (unsigned char*)malloc(width * height);
    unsigned char* backward_predicted_frame = (unsigned char*)malloc(width * height);
    if (!forward_predicted_frame || !backward_predicted_frame) {
        fprintf(stderr, "Failed to allocate memory for predicted frames.\n");
        free(forward_motion_vectors);
        free(backward_motion_vectors);
        exit(EXIT_FAILURE);
    }

    // Forward motion estimation and compensation (current -> reference)
    #pragma omp parallel for collapse(2)
    for (int block_y = 0; block_y < height; block_y += block_size) {
        for (int block_x = 0; block_x < width; block_x += block_size) {
            int block_index = (block_y / block_size) * num_blocks_x + (block_x / block_size);
            forward_motion_vectors[block_index] = findMotionVector(
                current_frame, reference_frame, width, height, block_x, block_y, block_size, search_range
            );
        }
    }
    motionCompensation(reference_frame, forward_predicted_frame, width, height, forward_motion_vectors, block_size);

    // Backward motion estimation and compensation (current -> future)
    #pragma omp parallel for collapse(2)
    for (int block_y = 0; block_y < height; block_y += block_size) {
        for (int block_x = 0; block_x < width; block_x += block_size) {
            int block_index = (block_y / block_size) * num_blocks_x + (block_x / block_size);
            backward_motion_vectors[block_index] = findMotionVector(
                current_frame, future_frame, width, height, block_x, block_y, block_size, search_range
            );
        }
    }
    motionCompensation(future_frame, backward_predicted_frame, width, height, backward_motion_vectors, block_size);

    // Combine forward and backward predictions to create the B-frame
    unsigned char* bidirectional_predicted_frame = (unsigned char*)malloc(width * height);
    if (!bidirectional_predicted_frame) {
        fprintf(stderr, "Failed to allocate memory for bidirectional prediction.\n");
        free(forward_motion_vectors);
        free(backward_motion_vectors);
        free(forward_predicted_frame);
        free(backward_predicted_frame);
        exit(EXIT_FAILURE);
    }

    #pragma omp parallel for collapse(2)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            bidirectional_predicted_frame[y * width + x] = 
                (forward_predicted_frame[y * width + x] + backward_predicted_frame[y * width + x]) / 2;
        }
    }

    // Optionally calculate residual (current_frame - bidirectional_predicted_frame)
    unsigned char* residual_frame = (unsigned char*)malloc(width * height);
    if (residual_frame) {
        #pragma omp parallel for
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                residual_frame[y * width + x] =
                    current_frame[y * width + x] - bidirectional_predicted_frame[y * width + x];
            }
        }

        // Process residual (DCT and Quantization)
        #pragma omp parallel for collapse(2)
        for (int block_y = 0; block_y < height; block_y += block_size) {
            for (int block_x = 0; block_x < width; block_x += block_size) {
                double block[16][16];
                for (int i = 0; i < block_size; i++) {
                    for (int j = 0; j < block_size; j++) {
                        block[i][j] = residual_frame[(block_y + i) * width + (block_x + j)];
                    }
                }
                performFastDCT(block);
                quantizeBlock(block, quantization_table);
            }
        }

        free(residual_frame);
    }

    // Clean up
    free(forward_motion_vectors);
    free(backward_motion_vectors);
    free(forward_predicted_frame);
    free(backward_predicted_frame);
    free(bidirectional_predicted_frame);

    printf("Processed B-frame %d successfully.\n", frame_index);
}



void processFrames(FrameType frame_type, struct jpeg_decompress_struct cinfo, unsigned char* current, unsigned char* reference, unsigned char* future, int frame_index){
    if (frame_type == I_FRAME) {
        processIFrame(cinfo, current, frame_index);
    } else if (frame_type == P_FRAME) {
        processPFrame(cinfo, current, reference, frame_index);
    } else if (frame_type == B_FRAME) {
        processBFrame(cinfo, current, reference, future, frame_index);
    }
}
