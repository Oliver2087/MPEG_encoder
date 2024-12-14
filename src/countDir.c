#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <dirent.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "countDir.h"

// Filter function to include only files ending with .jpeg
int filter_jpeg(const struct dirent *entry) {
    const char *filename = entry->d_name;
    const char *extension = ".jpeg";
    size_t len_filename = strlen(filename);
    size_t len_extension = strlen(extension);

    // Check if the filename ends with .jpeg
    if (len_filename >= len_extension &&
        strcmp(filename + len_filename - len_extension, extension) == 0) {
        return 1; // Include this file
    }
    return 0; // Exclude this file
}

int my_alphasort(const struct dirent **a, const struct dirent **b) {
    return strcmp((*a)->d_name, (*b)->d_name);
}

// Function to load all filenames from a directory with .jpeg extension
int load_filenames(const char *directory, unsigned char filenames[MAX_FILES][MAX_FILENAME_LEN]) {
    struct dirent **namelist;
    int n, count = 0;
    char path[MAX_FILENAME_LEN];
    struct stat file_stat;

    // Use scandir with the filter function
    n = scandir(directory, &namelist, filter_jpeg, my_alphasort);
    if (n < 0) {
        perror("scandir");
        return -1;
    }

    // Iterate over the filtered directory entries
    for (int i = 0; i < n && count < MAX_FILES; i++) {
        // Construct the full path
        if (snprintf(path, sizeof(path), "%s/%s", directory, namelist[i]->d_name) >= sizeof(path)) {
            fprintf(stderr, "Warning: Path truncated for %s/%s\n", directory, namelist[i]->d_name);
            continue;
        }

        // Use stat to check if it is a regular file
        if (stat(path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
            // Copy the full path into the filenames array
            strncpy((char *)filenames[count], path, MAX_FILENAME_LEN - 1);
            filenames[count][MAX_FILENAME_LEN - 1] = '\0'; // Ensure null termination
            count++;
        }

        free(namelist[i]); // Free the dirent struct allocated by scandir
    }
    free(namelist); // Free the array of pointers allocated by scandir

    return count;
}