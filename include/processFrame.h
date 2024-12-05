#ifndef PROCESS_FRAME_H_
#define PROCESS_FRAME_H_

#define BLOCK_SIZE 16

typedef enum { 
    I_FRAME, 
    P_FRAME, 
    B_FRAME 
} FrameType;

typedef struct {
    int width;
    int height;
    int block_size;
    double* data; // Pointer to store compressed DCT coefficients
} CompressedData;

extern const unsigned char quantization_table[BLOCK_SIZE][BLOCK_SIZE];

void assignFrameTypes(FrameType* frame_types, int total_frames, int gop_size, int b_frames);
void processFrames(FrameType frame_type, struct jpeg_decompress_struct cinfo, unsigned char* current, unsigned char* reference, unsigned char* future, int frame_index);

#endif