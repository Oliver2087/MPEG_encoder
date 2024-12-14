#ifndef CNT_DIR_H_
#define CNT_DIR_H_

#define MAX_FILES 100  // Adjust this based on the number of files you expect
#define MAX_FILENAME_LEN 256 // Maximum filename length
#define JPEG_DIRECTORY "/home/wsoxl/Encoder/inputFiles"

int load_filenames(const char *directory, unsigned char filenames[MAX_FILES][MAX_FILENAME_LEN]);

#endif