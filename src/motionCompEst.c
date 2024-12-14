#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "motionCompEst.h"

void getBlock(unsigned char* frame, int width, int height, int x, int y, int block_size, unsigned char* block) {
    // Ensure we don't go out of bounds
    if (x < 0 || y < 0 || x + block_size > width || y + block_size > height) {
        fprintf(stderr, "getBlock: Requested block is out of frame bounds.\n");
        return;
    }

    for (int i = 0; i < block_size; i++) {
        memcpy(block + i * block_size, frame + (y + i) * width + x, block_size);
    }
}

void averageBlocks(unsigned char* blockA, unsigned char* blockB, unsigned char* output, int block_size) {
    for (int y = 0; y < block_size; y++) {
        for (int x = 0; x < block_size; x++) {
            int index = y * block_size + x;
            output[index] = (blockA[index] + blockB[index]) / 2;
        }
    }
}

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

void performMotionCompensation(
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

    // #pragma omp parallel for collapse(2)
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

    // #pragma omp parallel for collapse(2)
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

void performBidirectionalEstimation(
    unsigned char* current_frame,
    unsigned char* previous_frame,
    unsigned char* next_frame,
    int width,
    int height,
    int block_size,
    int search_range,
    MotionVector* mv_backward_array, // output: motion vectors referencing previous_frame
    MotionVector* mv_forward_array   // output: motion vectors referencing next_frame
) {
    int num_blocks_x = width / block_size;
    int num_blocks_y = height / block_size;

    // Parallelization optional, depends on your setup
    // #pragma omp parallel for collapse(2)
    for (int by = 0; by < num_blocks_y; by++) {
        for (int bx = 0; bx < num_blocks_x; bx++) {
            int block_x = bx * block_size;
            int block_y = by * block_size;
            int block_index = by * num_blocks_x + bx;

            // Find backward motion vector (current block vs previous_frame)
            MotionVector mv_backward = findMotionVector(
                current_frame,
                previous_frame,
                width,
                height,
                block_x,
                block_y,
                block_size,
                search_range
            );

            // Find forward motion vector (current block vs next_frame)
            MotionVector mv_forward = findMotionVector(
                current_frame,
                next_frame,
                width,
                height,
                block_x,
                block_y,
                block_size,
                search_range
            );

            mv_backward_array[block_index] = mv_backward;
            mv_forward_array[block_index] = mv_forward;
        }
    }
}

void performBidirectionalCompensation(
    unsigned char* previous_frame,
    unsigned char* next_frame,
    unsigned char* predicted_frame,
    int width,
    int height,
    int block_size,
    MotionVector* mv_backward_array,
    MotionVector* mv_forward_array
) {
    int num_blocks_x = width / block_size;
    int num_blocks_y = height / block_size;

    unsigned char* block_past   = (unsigned char*)malloc(block_size * block_size);
    unsigned char* block_future = (unsigned char*)malloc(block_size * block_size);
    unsigned char* block_avg    = (unsigned char*)malloc(block_size * block_size);

    if (!block_past || !block_future || !block_avg) {
        fprintf(stderr, "Memory allocation failed in performBidirectionalCompensation.\n");
        free(block_past);
        free(block_future);
        free(block_avg);
        return;
    }

    for (int by = 0; by < num_blocks_y; by++) {
        for (int bx = 0; bx < num_blocks_x; bx++) {
            int block_x = bx * block_size;
            int block_y = by * block_size;
            int block_index = by * num_blocks_x + bx;

            MotionVector mv_backward = mv_backward_array[block_index];
            MotionVector mv_forward  = mv_forward_array[block_index];

            int ref_past_x  = block_x + mv_backward.dx;
            int ref_past_y  = block_y + mv_backward.dy;
            int ref_future_x = block_x + mv_forward.dx;
            int ref_future_y = block_y + mv_forward.dy;

            // Boundary check: ensure we don't read outside the frame
            // In a real encoder, you'd have more robust boundary handling (maybe use a padded frame)
            if (ref_past_x < 0) ref_past_x = 0;
            if (ref_past_y < 0) ref_past_y = 0;
            if (ref_past_x + block_size > width) ref_past_x = width - block_size;
            if (ref_past_y + block_size > height) ref_past_y = height - block_size;

            if (ref_future_x < 0) ref_future_x = 0;
            if (ref_future_y < 0) ref_future_y = 0;
            if (ref_future_x + block_size > width) ref_future_x = width - block_size;
            if (ref_future_y + block_size > height) ref_future_y = height - block_size;

            // Extract blocks from previous and next frames
            getBlock(previous_frame, width, height, ref_past_x, ref_past_y, block_size, block_past);
            getBlock(next_frame,     width, height, ref_future_x, ref_future_y, block_size, block_future);

            // Average the two blocks to get the predicted block
            averageBlocks(block_past, block_future, block_avg, block_size);

            // Copy block_avg into predicted_frame
            for (int i = 0; i < block_size; i++) {
                memcpy(&predicted_frame[(block_y + i)*width + block_x], &block_avg[i*block_size], block_size);
            }
        }
    }

    free(block_past);
    free(block_future);
    free(block_avg);
}