#include "readImage.h"

#define FILENAME "../../data/Video1/Images/image0005.jpeg"
#define FILENAME_OUTPUT "output.raw"

int main() {
    ImageInfo imageinfo;
    char filename[50] = FILENAME;
    if(readImage(&imageinfo, filename) != 0) {
        fprintf(stderr, "Failed to read the image.\n");
        return EXIT_FAILURE;
    }

    char filename_o[15] = FILENAME_OUTPUT;
    FILE* outfile = fopen(filename_o, "wb");
    if(!outfile) {
        fprintf(stderr, "Error creating output file %s!\n", filename_o);
        free(imageinfo.buf_p);  // 释放分配的内存
        return EXIT_FAILURE;
    }

    size_t bytes_written = fwrite(imageinfo.buf_p, 1, imageinfo.buf_size, outfile);
    if(bytes_written != imageinfo.buf_size) {
        fprintf(stderr, "Error writing data to output file %s!\n", filename_o);
        fclose(outfile);
        free(imageinfo.buf_p);  // 释放分配的内存
        return EXIT_FAILURE;
    }

    printf("Image saved to %s\n", filename_o);
    fclose(outfile);
    free(imageinfo.buf_p);  // 释放分配的内存

    return 0;
}