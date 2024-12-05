#include <libavformat/avformat.h>
#include <stdio.h>

#define FILENAME "/home/qiangch2001/projects/MPEG_encoder/test/empty.mpg"

int main() {
    int ex = 1;
    AVFormatContext* ctx_fmt = NULL;
    ex = avformat_open_input(&ctx_fmt, FILENAME, NULL, NULL);
    if(ex) {
        char err_buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ex, err_buf, sizeof(err_buf));
        fprintf(stderr, "Error: %s\n", err_buf);
        return EXIT_FAILURE;
    }
    return 0;
}