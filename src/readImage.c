#include "readImage.h"

int readImage(ImageInfo* imageinfo, char* filename) {
    FILE* infile = fopen(filename, "rb");
    if(!infile) {
        fprintf(stderr, "Error opening JPEG file %s!\n", filename);
        exit(EXIT_FAILURE);
    }
    
    struct jpeg_error_mgr jerr;
    struct jpeg_decompress_struct cinfo;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);   // Set cinfo.src
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    imageinfo->width = cinfo.output_width;
    imageinfo->height = cinfo.output_height;
    int pixel_size = cinfo.output_components;
    imageinfo->buf_size = imageinfo->width * imageinfo->height * pixel_size;
    imageinfo->buf_p = (unsigned char*)malloc(imageinfo->buf_size);
    int batch_size = NUMOFLINESREADINONETIME; // The number of lines the algorithm is going to read in one time
    unsigned char* rowptr[batch_size];
    while(cinfo.output_scanline < imageinfo->height) {
        int lines_to_read = cinfo.output_scanline +
            batch_size > imageinfo->height ? imageinfo->height - cinfo.output_scanline : batch_size;
        // Set row pointers for the batch
        for(int i = 0; i < lines_to_read; i++) {
            rowptr[i] = imageinfo->buf_p + (cinfo.output_scanline + i) * imageinfo->width * pixel_size;
        }
        jpeg_read_scanlines(&cinfo, rowptr, lines_to_read);
    }
    jpeg_finish_decompress(&cinfo);
    fclose(infile);

    // Check whether the data is in yuv
    printf("Number of components: %d\n", cinfo.num_components); // Check the number of th components.
    if(cinfo.jpeg_color_space == JCS_YCbCr) {
        printf("The image is in YUV (YCbCr) format.\n");
    }else if(cinfo.jpeg_color_space == JCS_RGB) {
        perror("The image is in RGB format.\n");
        exit(EXIT_FAILURE);
    }else {
        perror("The image is in an unsupported color space.\n");
        exit(EXIT_FAILURE);
    }

    // 图片数据已在 bmp_buffer 中，可进一步处理
    printf("Image width: %d, height: %d, pixel size: %d\n", imageinfo->width, imageinfo->height, pixel_size);
    jpeg_destroy_decompress(&cinfo);
    return 0;
}