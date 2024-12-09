#include "createMLV.h"

#define FILENAME "output.mlv"

int main() {
    ImageInfo imageinfo;
    imageinfo.width = 16;
    imageinfo.height = 16;
    char filenmae[15] = FILENAME;
    createMLV(filenmae, imageinfo);
    return 0;
}