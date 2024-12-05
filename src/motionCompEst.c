#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "motionCompEst.h"

// Perform Sum of Absolute Differences (SAD) for block matching
int computeSAD(unsigned char* current, unsigned char* reference, int width, int block_x, int block_y, int ref_x, int ref_y, int block_size) {
    int sad = 0;
    for (int y = 0; y < block_size; y++) {
        for (int x = 0; x < block_size; x++) {
            int current_pixel = current[(block_y + y) * width + (block_x + x)];
            int reference_pixel = reference[(ref_y + y) * width + (ref_x + x)];
            sad += abs(current_pixel - reference_pixel);
        }
    }
    return sad;
}

// Check if a block is static using a low threshold SAD
bool isStaticBlock(unsigned char* current, unsigned char* reference, int width, int block_x, int block_y, int block_size, int threshold) {
    int sad = computeSAD(current, reference, width, block_x, block_y, block_x, block_y, block_size);
    return sad < threshold;
}

// Motion estimation for P-frames
MotionVector findMotionVector(unsigned char* current, unsigned char* reference, int width, int height, int block_x, int block_y, int block_size, int search_range) {
    MotionVector best_vector = {0, 0};
    int min_sad = INT_MAX;

    // Early termination for static blocks
    if (isStaticBlock(current, reference, width, block_x, block_y, block_size, 5)) {
        return best_vector; // No motion for static blocks
    }

    // Perform full search for motion estimation
    for (int dy = -search_range; dy <= search_range; dy++) {
        for (int dx = -search_range; dx <= search_range; dx++) {
            int ref_x = block_x + dx;
            int ref_y = block_y + dy;

            // Check bounds
            if (ref_x < 0 || ref_y < 0 || ref_x + block_size > width || ref_y + block_size > height) {
                continue;
            }

            int sad = computeSAD(current, reference, width, block_x, block_y, ref_x, ref_y, block_size);
            if (sad < min_sad) {
                min_sad = sad;
                best_vector.dx = dx;
                best_vector.dy = dy;
            }
        }
    }

    return best_vector;
}

void motionCompensation(
    unsigned char* reference,
    unsigned char* predicted,
    int width,
    int height,
    MotionVector* motion_vectors,
    int block_size) {
    
    if (!reference || !predicted || !motion_vectors) {
        fprintf(stderr, "Error: Null pointer passed to motionCompensation.\n");
        exit(EXIT_FAILURE);
    }

    int num_blocks_x = width / block_size;
    int num_blocks_y = height / block_size;

    #pragma omp parallel for collapse(2)
    for (int block_y = 0; block_y < num_blocks_y; block_y++) {
        for (int block_x = 0; block_x < num_blocks_x; block_x++) {
            int block_index = block_y * num_blocks_x + block_x;
            MotionVector mv = motion_vectors[block_index];

            // Iterate over each pixel in the block
            for (int y = 0; y < block_size; y++) {
                for (int x = 0; x < block_size; x++) {
                    int ref_x = block_x * block_size + mv.dx + x;
                    int ref_y = block_y * block_size + mv.dy + y;

                    // Bounds checking for the reference frame
                    if (ref_x < 0 || ref_y < 0 || ref_x >= width || ref_y >= height) {
                        fprintf(stderr, "Error: Motion vector out of bounds at block (%d, %d).\n", block_x, block_y);
                        continue;
                    }

                    int ref_idx = ref_y * width + ref_x;
                    int pred_idx = (block_y * block_size + y) * width + (block_x * block_size + x);

                    predicted[pred_idx] = reference[ref_idx];
                }
            }
        }
    }
}

void performMotionEstimation(
    unsigned char* current_frame,
    unsigned char* reference_frame,
    int width,
    int height,
    int block_size,
    int search_range,
    MotionVector* motion_vectors) {

    if (!current_frame || !reference_frame || !motion_vectors) {
        fprintf(stderr, "Error: Null pointer passed to performMotionEstimation.\n");
        exit(EXIT_FAILURE);
    }

    int num_blocks_x = width / block_size;
    int num_blocks_y = height / block_size;

    #pragma omp parallel for collapse(2)
    for (int block_y = 0; block_y < num_blocks_y; block_y++) {
        for (int block_x = 0; block_x < num_blocks_x; block_x++) {
            int block_index = block_y * num_blocks_x + block_x;

            // Find the motion vector for the current block
            motion_vectors[block_index] = findMotionVector(
                current_frame,
                reference_frame,
                width,
                height,
                block_x * block_size, // x-coordinate of the block
                block_y * block_size, // y-coordinate of the block
                block_size,
                search_range
            );
        }
    }
}