#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// Write the pack header
void writePackHeader(FILE *file) {
    uint8_t pack_header[14] = {
        0x00, 0x00, 0x01, 0xBA,  // 起始码
        0x44, 0x00, 0x04, 0x00,  // 系统时钟参考（模拟值）
        0x04, 0x01, 0x1F, 0xFF,  // SCR 扩展（模拟值）和比特率
        0xF8                       // 结束标志
    };
    fwrite(pack_header, 1, sizeof(pack_header), file);
}

// Write the system header
void writeSystemHeader(FILE *file) {
    uint8_t system_header[18] = {
        0x00, 0x00, 0x01, 0xBB,  // 起始码
        0x00, 0x0C,              // 长度
        0x80, 0xC4, 0x0D, 0x00,  // 标志位和固定值
        0x00, 0x00, 0x01, 0xE0,  // 视频流起始码
        0x00, 0x0F,              // 缓冲区大小（模拟值）
        0xE0, 0x00               // 结束标志
    };
    fwrite(system_header, 1, sizeof(system_header), file);
}

// Write the empty video packet.
void writeEmptyVideoPacket(FILE *file) {
    uint8_t video_packet[6] = {
        0x00, 0x00, 0x01, 0xE0,  // 视频流起始码
        0x00, 0x00               // 包长度为0
    };
    fwrite(video_packet, 1, sizeof(video_packet), file);
}

int main() {
    FILE* file = fopen("empty.mpg", "wb");
    if(!file) {
        perror("Failed to open file");
        return EXIT_FAILURE;
    }

    writePackHeader(file);       // Write the pack header.
    writeSystemHeader(file);     // 写入 System Header
    // writeEmptyVideoPacket(file); // 写入空视频包

    fclose(file);
    printf("Empty MPEG-1 file 'empty.mpg' generated successfully.\n");
    return 0;
}
