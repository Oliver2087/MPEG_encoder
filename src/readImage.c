// gcc -o readImage readImage.c -ljpeg
// ./readImage

#include <math.h>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <jpeglib.h>
#include "countDir.h"
#include "processFrame.h"
#include "encoder.h"

#define NUMOFLINESREADINONETIME 16

// int decompressJPEG(struct jpeg_decompress_struct* cinfo_p, unsigned char** bmp_buffer, FILE* infile) {
//     struct jpeg_error_mgr jerr;
    
//     (*cinfo_p).err = jpeg_std_error(&jerr);
//     jpeg_create_decompress(cinfo_p);
//     jpeg_stdio_src(cinfo_p, infile);   // Set cinfo.src
//     jpeg_read_header(cinfo_p, TRUE);
//     jpeg_start_decompress(cinfo_p);

//     int width = (*cinfo_p).output_width;
//     int height = (*cinfo_p).output_height;
//     int pixel_size = (*cinfo_p).output_components;
//     unsigned long bmp_size = width * height * pixel_size;
//     *bmp_buffer = (unsigned char*)malloc(bmp_size);
//     int batch_size = NUMOFLINESREADINONETIME; // The number of lines the algorithm is going to read in one time
//     unsigned char* rowptr[batch_size];
//     while((*cinfo_p).output_scanline < height) {
//         int lines_to_read = (*cinfo_p).output_scanline +
//             batch_size > height ? height - (*cinfo_p).output_scanline :
//             batch_size;
//         // Set row pointers for the batch
//         for (int i = 0; i < lines_to_read; i++) {
//             rowptr[i] = *bmp_buffer + ((*cinfo_p).output_scanline + i) * width * pixel_size;
//         }
//         jpeg_read_scanlines(cinfo_p, rowptr, lines_to_read);
//     }
//     jpeg_finish_decompress(cinfo_p);
//     return 0;
// }

struct jpeg_decompress_struct readJPEGFile(unsigned char* filename, unsigned char** bmp_buffer) {
    FILE* infile = fopen(filename, "rb");
    if (!infile) {
        fprintf(stderr, "Error: Unable to open JPEG file %s\n", filename);
        return (struct jpeg_decompress_struct){0}; // Return an empty struct on error
    }

    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr); // Attach the error handler
    jpeg_create_decompress(&cinfo);

    jpeg_stdio_src(&cinfo, infile);

    // Read the JPEG header
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        fprintf(stderr, "Error: Failed to read JPEG header for file %s\n", filename);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return (struct jpeg_decompress_struct){0}; // Return an empty struct
    }

    // Start decompression
    if (!jpeg_start_decompress(&cinfo)) {
        fprintf(stderr, "Error: Failed to start decompression for file %s\n", filename);
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return (struct jpeg_decompress_struct){0}; // Return an empty struct
    }

    // Allocate memory for decompressed data
    int width = cinfo.output_width;
    int height = cinfo.output_height;
    int pixel_size = cinfo.output_components;
    unsigned long bmp_size = width * height * pixel_size;

    *bmp_buffer = (unsigned char*)malloc(bmp_size);
    if (!*bmp_buffer) {
        fprintf(stderr, "Error: Failed to allocate memory for BMP buffer.\n");
        jpeg_destroy_decompress(&cinfo);
        fclose(infile);
        return (struct jpeg_decompress_struct){0}; // Return an empty struct
    }

    // Read the decompressed image data
    int row_stride = width * pixel_size;
    unsigned char* row_ptr[1];
    while (cinfo.output_scanline < height) {
        row_ptr[0] = *bmp_buffer + cinfo.output_scanline * row_stride;
        jpeg_read_scanlines(&cinfo, row_ptr, 1);
    }

    // Finish decompression
    jpeg_finish_decompress(&cinfo);
    fclose(infile);

    // Check the color space
    if (cinfo.jpeg_color_space == JCS_YCbCr) {
        printf("The image is in YUV (YCbCr) format.\n");
    } else if (cinfo.jpeg_color_space == JCS_RGB) {
        printf("The image is in RGB format.\n");
    } else {
        fprintf(stderr, "Error: Unsupported color space in file %s\n", filename);
        free(*bmp_buffer);
        jpeg_destroy_decompress(&cinfo);
        return (struct jpeg_decompress_struct){0}; // Return an empty struct
    }

    return cinfo; // Successfully return the decompression structure
}

GopStruct* initializeGopStruct() {
    // Allocate memory for GopStruct
    GopStruct* gop_struct = (GopStruct*)malloc(sizeof(GopStruct));
    if (!gop_struct) {
        fprintf(stderr, "Memory allocation failed for GopStruct\n");
        exit(EXIT_FAILURE);
    }

    // Allocate memory for display_frame_types and encoding_frame_types
    gop_struct->display_frame_types = (FrameType*)malloc(GOP_SIZE * sizeof(FrameType));
    gop_struct->encoding_frame_types = (FrameType*)malloc(GOP_SIZE * sizeof(FrameType));
    gop_struct->frames = (Frame*)malloc(GOP_SIZE * sizeof(Frame)); // Array of frames

    if (!gop_struct->display_frame_types || !gop_struct->encoding_frame_types || !gop_struct->frames) {
        fprintf(stderr, "Memory allocation failed for GopStruct fields\n");
        free(gop_struct);
        exit(EXIT_FAILURE);
    }

    // Initialize frame types
    for (int i = 0; i < GOP_SIZE; i++) {
        gop_struct->display_frame_types[i] = (i == 0) ? I_FRAME : ((i - 1) % 3 == 0 ? P_FRAME : B_FRAME);
        gop_struct->encoding_frame_types[i] = gop_struct->display_frame_types[i];
    }

    // Initialize frames and link them
    for (int i = 0; i < GOP_SIZE; i++) {
        Frame* previous_frame = (i > 0) ? &gop_struct->frames[i - 1] : NULL;
        Frame* future_frame = (i < GOP_SIZE - 1) ? &gop_struct->frames[i + 1] : NULL;
        initializeFrame(&gop_struct->frames[i], gop_struct->encoding_frame_types[i], i, previous_frame, future_frame, 0, 0);
    }

    // Initialize gop_index
    gop_struct->gop_index = 0;

    return gop_struct;
}

// Function to free memory allocated for GopStruct
void freeGopStruct(GopStruct* gop_struct) {
    if (!gop_struct) return;

    // Free each frame's resources
    for (int i = 0; i < GOP_SIZE; i++) {
        free(gop_struct->frames[i].block_data);
    }

    // Free arrays in GopStruct
    free(gop_struct->display_frame_types);
    free(gop_struct->encoding_frame_types);
    free(gop_struct->frames);

    // Free the GopStruct itself
    free(gop_struct);
}

void updateGOP(GopStruct* curr_GOP, int gop_index, int width, int height) {
    curr_GOP->gop_index = gop_index;
    // Assign frame numbers and types
    for (int i = 0; i < GOP_SIZE; i++) {
        curr_GOP->frames[i].frame_number = i;
        FrameType type = curr_GOP->encoding_frame_types[i]; 
        curr_GOP->frames[i].type = type;
        curr_GOP->frames[i].width = width;
        curr_GOP->frames[i].height = height;             
    }

    // Assign pointers to previous and future frames
    for (int i = 0; i < GOP_SIZE; i++) {
        unsigned char* temp_buffer = NULL;
        curr_GOP->frames[i].previous_frame = NULL;  // Initialize previous_frame pointer
        curr_GOP->frames[i].future_frame = NULL;    // Initialize future_frame pointer
        curr_GOP->frames[i].block_data = temp_buffer;

        // Assign previous_frame for P and B frames
        if (curr_GOP->frames[i].type == P_FRAME || curr_GOP->frames[i].type == B_FRAME) {
            // Look backwards for the nearest I or P frame
            for (int j = i - 1; j >= 0; j--) {
                if (curr_GOP->frames[j].type == I_FRAME || curr_GOP->frames[j].type == P_FRAME) {
                    curr_GOP->frames[i].previous_frame = &curr_GOP->frames[j];
                    break;
                }
            }
        }

        // Assign future_frame for B frames
        if (curr_GOP->frames[i].type == B_FRAME) {
            // Look forwards for the nearest P frame
            for (int j = i + 1; j < GOP_SIZE; j++) {
                if (curr_GOP->frames[j].type == P_FRAME) {
                    curr_GOP->frames[i].future_frame = &curr_GOP->frames[j];
                    break;
                }
            }
        }
    }
}

int main() {
    struct timeval start, end;
    unsigned char filenames[MAX_FILES][MAX_FILENAME_LEN];
    int count;
    unsigned char* bmp_buffer = NULL;

    gettimeofday(&start, NULL);

    // Load JPEG filenames
    count = load_filenames(JPEG_DIRECTORY, filenames);

    // Allocate array for frame types
    FrameType encoding_frame_types[count];
    FrameType display_frame_types[count];
    GopStruct *curr_GOP = initializeGopStruct();

    assignEncodingFrameTypes(curr_GOP, GOP_SIZE, B_FRAME_SIZE);
    assignDisplayFrameTypes(curr_GOP, GOP_SIZE);

    // Read the first frame to determine dimensions
    struct jpeg_decompress_struct cinfo = readJPEGFile(filenames[1], &bmp_buffer);
    int width = cinfo.output_width;
    int height = cinfo.output_height;
    int pixel_size = cinfo.output_components; // e.g., 3 for YCbCr

    for (int i = 0; i < (count / GOP_SIZE); i++) {
        // Update frames
        updateGOP(curr_GOP, (i / GOP_SIZE), width, height);
        for (int j = 0; j < GOP_SIZE; j++) {
            struct jpeg_decompress_struct cinfo = readJPEGFile(filenames[(i * GOP_SIZE) + j], &(curr_GOP->frames[j].block_data));
        }
        // Process the current frame based on its type
        processGOP(curr_GOP);
    }

    // Cleanup
    free(bmp_buffer);
    freeGopStruct(curr_GOP);

    gettimeofday(&end, NULL);
    double total_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    printf("Total execution time: %.2f seconds\n", total_time);

    return 0;
}
