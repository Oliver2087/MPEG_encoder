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

#define BLOCK_SIZE 16
#define NUMOFLINESREADINONETIME 16

#define GOP_SIZE 8       // Frames per GOP
#define B_FRAME_SIZE 2       // Number of B-frames between I and P-frames

int decompressJPEG(struct jpeg_decompress_struct* cinfo_p, unsigned char** bmp_buffer, FILE* infile) {
    struct jpeg_error_mgr jerr;
    
    (*cinfo_p).err = jpeg_std_error(&jerr);
    jpeg_create_decompress(cinfo_p);
    jpeg_stdio_src(cinfo_p, infile);   // Set cinfo.src
    jpeg_read_header(cinfo_p, TRUE);
    jpeg_start_decompress(cinfo_p);

    int width = (*cinfo_p).output_width;
    int height = (*cinfo_p).output_height;
    int pixel_size = (*cinfo_p).output_components;
    unsigned long bmp_size = width * height * pixel_size;
    *bmp_buffer = (unsigned char*)malloc(bmp_size);
    int batch_size = NUMOFLINESREADINONETIME; // The number of lines the algorithm is going to read in one time
    unsigned char* rowptr[batch_size];
    while((*cinfo_p).output_scanline < height) {
        int lines_to_read = (*cinfo_p).output_scanline +
            batch_size > height ? height - (*cinfo_p).output_scanline :
            batch_size;
        // Set row pointers for the batch
        for (int i = 0; i < lines_to_read; i++) {
            rowptr[i] = *bmp_buffer + ((*cinfo_p).output_scanline + i) * width * pixel_size;
        }
        jpeg_read_scanlines(cinfo_p, rowptr, lines_to_read);
    }
    jpeg_finish_decompress(cinfo_p);
    return 0;
}

struct jpeg_decompress_struct readJPEGFile(unsigned char *filename, unsigned char** bmp_buffer) {
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

void updateFrames(unsigned char* previous_frame, unsigned char* current_frame, unsigned char* future_frame, 
                  unsigned char* bmp_buffer, int frame_size, int frame_index, int total_frames, unsigned char filenames[][MAX_FILENAME_LEN]) {
    // Copy current frame to the previous frame
    memcpy(previous_frame, current_frame, frame_size);

    // Copy the decompressed current buffer to current frame
    memcpy(current_frame, bmp_buffer, frame_size);

    // Load the next frame into future_frame if it's not the last frame
    if (frame_index + 1 < total_frames) {
        unsigned char* temp_buffer = NULL;
        struct jpeg_decompress_struct futureCInfo = readJPEGFile(filenames[frame_index + 1], &temp_buffer);
        memcpy(future_frame, temp_buffer, frame_size);
        free(temp_buffer); // Free temporary buffer allocated by readJPEGFile
    } else {
        // If this is the last frame, clear the future frame
        memset(future_frame, 0, frame_size);
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
    for (int i = 0; i < count; i++)
    {
        printf("Filename %d: %s\n", i + 1, filenames[i]);
    }

    // Allocate array for frame types
    FrameType frame_types[count];
    assignFrameTypes(frame_types, count, GOP_SIZE, B_FRAME_SIZE);

    // Read the first frame to determine dimensions
    struct jpeg_decompress_struct cinfo = readJPEGFile(filenames[1], &bmp_buffer);
    int width = cinfo.output_width;
    int height = cinfo.output_height;
    int pixel_size = cinfo.output_components; // e.g., 3 for YCbCr

    int frame_size = width * height * pixel_size;

    // Allocate memory for frame buffers
    unsigned char* previous_frame = (unsigned char*)malloc(frame_size);
    unsigned char* future_frame = (unsigned char*)malloc(frame_size);
    unsigned char* current_frame = (unsigned char*)malloc(frame_size);

    if (!previous_frame || !future_frame || !current_frame) {
        fprintf(stderr, "Memory allocation failed for frame buffers.\n");
        exit(EXIT_FAILURE);
    }

    // Update frames
    updateFrames(previous_frame, current_frame, future_frame, bmp_buffer, frame_size, 0, count, filenames);

    // Process the current frame based on its type
    processFrames(frame_types[0], cinfo, current_frame, previous_frame, future_frame, 0);

    //Initialize the first frame as the current frame
    memcpy(current_frame, bmp_buffer, frame_size);

    for (int i = 1; i < count; i++) {
        // Read the current JPEG file
        struct jpeg_decompress_struct cinfo = readJPEGFile(filenames[i], &bmp_buffer);

        // Update frames
        updateFrames(previous_frame, current_frame, future_frame, bmp_buffer, frame_size, i, count, filenames);

        // Process the current frame based on its type
        processFrames(frame_types[i], cinfo, current_frame, previous_frame, future_frame, i);

        // Free the compressed data (if any) after processing
        // For example, if compressed data was used:
        // freeCompressedData(compressed_data);
    }

    // Cleanup
    free(previous_frame);
    free(future_frame);
    free(current_frame);
    free(bmp_buffer);

    gettimeofday(&end, NULL);
    double total_time = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
    printf("Total execution time: %.2f seconds\n", total_time);

    return 0;
}
