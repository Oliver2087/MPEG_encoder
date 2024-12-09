#include "seperateMatrix.h"
#include "readImage.h"

#define FILENAME "../../data/images/output.jpg"

int main() {
    ImageInfo imageinfo;
    char filename[30] = FILENAME;
    readImage(&imageinfo, filename);
    uint8_t y_block[4][BLOCKSIZE * BLOCKSIZE];
    seperateMatrix(y_block, imageinfo.buf_p);
    for(int i = 0; i < 8; i++) {
        for(int j = 0; j < 8; j++) {
            printf("%d, ", y_block[0][i * 8+j]);
        }
        printf("\n");
    }
    return 0;
}