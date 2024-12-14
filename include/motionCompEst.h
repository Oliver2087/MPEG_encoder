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
void performMotionCompensation(unsigned char* reference, unsigned char* predicted, int width, int height, MotionVector* motion_vectors, int block_size);
void performMotionEstimation(unsigned char* current_frame, unsigned char* reference_frame, int width, int height, int block_size, int search_range, MotionVector* motion_vectors);

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
);
void performBidirectionalCompensation(
    unsigned char* previous_frame,
    unsigned char* next_frame,
    unsigned char* predicted_frame,
    int width,
    int height,
    int block_size,
    MotionVector* mv_backward_array,
    MotionVector* mv_forward_array
);

#endif