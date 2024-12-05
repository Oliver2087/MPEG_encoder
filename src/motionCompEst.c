#include <limits.h>
#include <math.h>
#include <stdlib.h>
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

// Motion estimation for P-frames
MotionVector findMotionVector(unsigned char* current, unsigned char* reference, int width, int height, int block_x, int block_y, int block_size, int search_range) {
    MotionVector best_vector = {0, 0};
    int min_sad = INT_MAX;

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

// Motion compensation for P-frames
void motionCompensation(unsigned char* reference, unsigned char* predicted, int width, int height, MotionVector* motion_vectors, int block_size) {
    for (int block_y = 0; block_y < height; block_y += block_size) {
        for (int block_x = 0; block_x < width; block_x += block_size) {
            int block_index = (block_y / block_size) * (width / block_size) + (block_x / block_size);
            MotionVector mv = motion_vectors[block_index];

            for (int y = 0; y < block_size; y++) {
                for (int x = 0; x < block_size; x++) {
                    int ref_x = block_x + mv.dx + x;
                    int ref_y = block_y + mv.dy + y;
                    predicted[(block_y + y) * width + (block_x + x)] = reference[ref_y * width + ref_x];
                }
            }
        }
    }
}