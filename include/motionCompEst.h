#ifndef MOTION_COMP_EST_H_
#define MOTION_COMP_EST_H_

typedef struct {
    int dx; // Horizontal displacement
    int dy; // Vertical displacement
} MotionVector;

typedef struct {
    int x;
    int y;
    int width;
    int height;
} Block;

int computeSAD(unsigned char* current, unsigned char* reference, int width, int block_x, int block_y, int ref_x, int ref_y, int block_size);
MotionVector findMotionVector(unsigned char* current, unsigned char* reference, int width, int height, int block_x, int block_y, int block_size, int search_range);
void motionCompensation(unsigned char* reference, unsigned char* predicted, int width, int height, MotionVector* motion_vectors, int block_size);
void performMotionEstimation(unsigned char* current_frame, unsigned char* reference_frame, int width, int height, int block_size, int search_range, MotionVector* motion_vectors);
#endif