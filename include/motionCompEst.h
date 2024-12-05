#ifndef MOTION_COMP_EST_H_
#define MOTION_COMP_EST_H_

typedef struct {
    int dx; // Horizontal displacement
    int dy; // Vertical displacement
} MotionVector;

MotionVector findMotionVector(unsigned char* current, unsigned char* reference, int width, int height, int block_x, int block_y, int block_size, int search_range);
void motionCompensation(unsigned char* reference, unsigned char* predicted, int width, int height, MotionVector* motion_vectors, int block_size);
#endif