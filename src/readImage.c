#include "readImage.h"

#define FPS 30
#define BITRATEPAR 100

void transferrRgb2Yuv420(unsigned char *yuv,unsigned char *rgb, int width, int height) {
    int frame_size = width * height + (width / 2) * (height / 2) * 2;
    int i, j;
    unsigned char *y = yuv;
    unsigned char *u = yuv + width * height;
    unsigned char *v = u + (width / 2) * (height / 2);

    for (i = 0; i < height; i++) {
        for (j = 0; j < width; j++) {
            int r = rgb[(i * width + j) * 3];
            int g = rgb[(i * width + j) * 3 + 1];
            int b = rgb[(i * width + j) * 3 + 2];
            
            int y_val = (0.299 * r + 0.587 * g + 0.114 * b);
            int u_val = (-0.147 * r - 0.289 * g + 0.436 * b);
            int v_val = (0.615 * r - 0.515 * g - 0.100 * b);

            y[i * width + j] = y_val;
            
            if (i % 2 == 0 && j % 2 == 0) {
                u[(i / 2) * (width / 2) + (j / 2)] = u_val + 128;
                v[(i / 2) * (width / 2) + (j / 2)] = v_val + 128;
            }
        }
    }
}


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
    unsigned char* buf_rgb = (unsigned char*)malloc(imageinfo->buf_size);
    int batch_size = NUMOFLINESREADINONETIME; // The number of lines the algorithm is going to read in one time
    unsigned char* rowptr[batch_size];
    while(cinfo.output_scanline < imageinfo->height) {
        int lines_to_read = cinfo.output_scanline +
            batch_size > imageinfo->height ? imageinfo->height - cinfo.output_scanline : batch_size;
        // Set row pointers for the batch
        for(int i = 0; i < lines_to_read; i++) {
            rowptr[i] = buf_rgb + (cinfo.output_scanline + i) * imageinfo->width * pixel_size;
        }
        jpeg_read_scanlines(&cinfo, rowptr, lines_to_read);
    }
    jpeg_finish_decompress(&cinfo);
    fclose(infile);

    imageinfo->buf_size = 1.5 * imageinfo->width * imageinfo->height;
    imageinfo->buf_p = (uint8_t*)malloc(imageinfo->buf_size);
    transferrRgb2Yuv420(imageinfo->buf_p, buf_rgb, imageinfo->width, imageinfo->height);
    free(buf_rgb);

    imageinfo->fps = FPS;
    imageinfo->bitrate = imageinfo->width * imageinfo->height * imageinfo->fps * BITRATEPAR;

    // 图片数据已在 bmp_buffer 中，可进一步处理
    printf("Image width: %d, height: %d, pixel size: %d\n", imageinfo->width, imageinfo->height, pixel_size);
    return 0;
}