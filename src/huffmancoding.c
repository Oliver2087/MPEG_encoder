// gcc -o readImage readImage.c -ljpeg
// ./readImage

#include <stdio.h>
#include <stdlib.h>
#include <jpeglib.h>
#include <string.h>

#define INPUTFILENAME "../../data/Video1/Images/image0005.jpeg"
#define OUTPUTFILENAME "Image5.dat"
#define BLOCK_SIZE 16

// Structure for Huffman nodes
typedef struct HuffmanNode {
    int value;
    unsigned freq;
    struct HuffmanNode *left, *right;
} HuffmanNode;

// Structure for Huffman code
typedef struct HuffmanCode {
    unsigned char bitstring[32]; // Maximum 256 bits
    int bitlength;
} HuffmanCode;

// Function to decompress JPEG and get raw pixel data
int decompressJPEG(struct jpeg_decompress_struct *cinfo_p, unsigned char **bmp_buffer, FILE *infile) {
    struct jpeg_error_mgr jerr;

    cinfo_p->err = jpeg_std_error(&jerr);
    jpeg_create_decompress(cinfo_p);
    jpeg_stdio_src(cinfo_p, infile);   // Set cinfo.src
    jpeg_read_header(cinfo_p, TRUE);
    jpeg_start_decompress(cinfo_p);

    int width = cinfo_p->output_width;
    int height = cinfo_p->output_height;
    int pixel_size = cinfo_p->output_components;
    unsigned long size_bmp = width * height * pixel_size;

    *bmp_buffer = (unsigned char *)malloc(size_bmp);
    unsigned char *rowptr[1];
    while (cinfo_p->output_scanline < height) {
        rowptr[0] = *bmp_buffer + cinfo_p->output_scanline * width * pixel_size;
        jpeg_read_scanlines(cinfo_p, rowptr, 1);
    }
    jpeg_finish_decompress(cinfo_p);
    return 0;
}

// Build Huffman Tree
HuffmanNode* createHuffmanTree(int freq_table[256]) {
    HuffmanNode* nodes[256];
    int node_count = 0;

    // Initialize nodes
    for(int i = 0; i < 256; i++) {
        if(freq_table[i] > 0) {
            nodes[node_count] = (HuffmanNode*)malloc(sizeof(HuffmanNode));
            nodes[node_count]->value = i;
            nodes[node_count]->freq = freq_table[i];
            nodes[node_count]->left = nodes[node_count]->right = NULL;
            node_count++;
        }
    }

    // Build the tree (always look for the 2 smallest value and put then into a tree)
    while(node_count > 1) {
        // Find two nodes with the smallest frequencies
        int min1 = 0, min2 = 1;
        if(nodes[min2]->freq < nodes[min1]->freq) {
            min1 = 1;
            min2 = 0;
        }

        for(int i = 2; i < node_count; i++) {
            if(nodes[i]->freq < nodes[min1]->freq) {
                min2 = min1;
                min1 = i;
            } else if (nodes[i]->freq < nodes[min2]->freq) {
                min2 = i;
            }
        }

        // Merge nodes[min1] and nodes[min2]
        HuffmanNode* merged = (HuffmanNode *)malloc(sizeof(HuffmanNode));
        merged->freq = nodes[min1]->freq + nodes[min2]->freq;
        merged->value = -1;
        merged->left = nodes[min1];
        merged->right = nodes[min2];

        // Replace the two nodes with the merged node
        nodes[min1] = merged;
        nodes[min2] = nodes[node_count - 1]; // Move the last node to min2
        node_count--;
    }
    return nodes[0];
}

// Generate Huffman codes
void generateHuffmanCodes(HuffmanCode huffman_table[], HuffmanNode* root, unsigned char *code, int length) {
    if(!root->left && !root->right) {
        huffman_table[root->value].bitlength = length;
        memcpy(huffman_table[root->value].bitstring, code, (length + 7) / 8);
        return;
    }
    if(root->left) {
        code[length / 8] &= ~(1 << (7 - (length % 8))); // Set current bit to 0
        generateHuffmanCodes(huffman_table, root->left, code, length + 1);
    }
    if(root->right) {
        code[length / 8] |= (1 << (7 - (length % 8))); // Set current bit to 1
        generateHuffmanCodes(huffman_table, root->right, code, length + 1);
    }
}

// Encode data using Huffman codes
void huffmanEncode(HuffmanCode huffman_table[], char *data, size_t size, FILE *outfile) {
    for(size_t i = 0; i < size; i++) {
        unsigned char symbol = data[i];
        fwrite(huffman_table[symbol].bitstring, 1, (huffman_table[symbol].bitlength + 7) / 8, outfile);
    }
}

int main() {
    // Read file
    FILE* infile = fopen(INPUTFILENAME, "rb");
    if(!infile) {
        fprintf(stderr, "Error opening JPEG file %s!\n", INPUTFILENAME);
        return EXIT_FAILURE;
    }

    // Decompress JPEG to raw pixel data
    struct jpeg_decompress_struct cinfo;
    unsigned char *bmp_buffer = NULL;
    decompressJPEG(&cinfo, &bmp_buffer, infile);
    fclose(infile);

    // Calculate frequency table
    size_t size_bmp = cinfo.output_width * cinfo.output_height * cinfo.output_components;
    int freq_table[256] = {0};
    for(size_t i = 0; i < size_bmp; i++) {
        freq_table[bmp_buffer[i]]++; // Everytime finding an element add one to the corresponding element in the frequency table.
    }

    // Build Huffman Tree and generate codes
    HuffmanNode* root = createHuffmanTree(freq_table); // Dynamic
    unsigned char code[32] = {0}; // Temporary buffer for bitstrings
    HuffmanCode huffman_table[256]; // Variable for storing Huffman codes
    generateHuffmanCodes(huffman_table, root, code, 0);

    // Encode and save to file
    FILE* outfile = fopen(OUTPUTFILENAME, "wb");
    if(!outfile) {
        fprintf(stderr, "Error opening output file %s!\n", OUTPUTFILENAME);
        return EXIT_FAILURE;
    }
    huffmanEncode(huffman_table, bmp_buffer, size_bmp, outfile);
    fclose(outfile);
    free(bmp_buffer);

    printf("Huffman encoding completed.\n");
    return 0;
}
