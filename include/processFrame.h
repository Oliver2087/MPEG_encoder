#ifndef PROCESS_FRAME_H_
#define PROCESS_FRAME_H_

#include <jpeglib.h>

#define BLOCK_SIZE 16

#define GOP_SIZE 8       // Frames per GOP
#define B_FRAME_SIZE 2       // Number of B-frames between I and P-frames

#define OUTPUT_PATH "/home/wsoxl/Encoder/MPEG_encoder/include/ImageOutput/Image%d.bin"

typedef enum { 
    I_FRAME, 
    P_FRAME, 
    B_FRAME 
} FrameType;

typedef struct Frame {
    FrameType type;                   // Type of the frame (I, P, or B)
    int frame_number;                 // Frame index
    struct Frame *previous_frame;     // Pointer to the previous frame
    struct Frame *future_frame;       // Pointer to the future frame
    unsigned char* block_data;        // Pointer to the frame's block data
    int width;
    int height;
} Frame;

typedef struct{
    FrameType* display_frame_types;
    FrameType* encoding_frame_types;
    Frame* frames;
    int gop_index;
} GopStruct;

typedef struct {
    int width;
    int height;
    int block_size;
    double* data; // Pointer to store compressed DCT coefficients
} CompressedData;

extern const unsigned char quantization_table[BLOCK_SIZE][BLOCK_SIZE];

// Function prototypes for frame management
void initializeFrame(Frame* frame, FrameType type, int frame_number, Frame* previous_frame, Frame* future_frame, int width, int height);
void assignEncodingFrameTypes(GopStruct* current_GOP, int gop_size, int b_frames);
void assignDisplayFrameTypes(GopStruct* current_GOP, int gop_size);
void processGOP(GopStruct* curr_GOP);

#endif