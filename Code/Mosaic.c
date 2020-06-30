#include<stdio.h>
#include<stdlib.h>
#include<dirent.h>
#include<string.h>
#include"Stack.h"
#include<zlib.h>

void print_buffer(const unsigned char * buf, size_t len)
{
    char output[50];
    size_t offset = 0;
    for (unsigned int i = 0; i < len; i++)
    {
        if (i % 8 == 0 && offset > 0)
        {
            printf("%s\n", output);
            offset = 0;
        }
        offset += snprintf(&output[offset], sizeof(output) - offset, "0x%02x, ", (unsigned char) buf[i]);
    }
    if (offset > 0)
    {
        printf("%s\n", output);
    }
}

void filter_sub(unsigned char *buffer, int scanlineIndex, int scanlinePixelCount, int pixelByteCount, int print) {
    int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
    int filterStart = scanlineIndex * scanlineLen;
    if(buffer[filterStart] != 1) {
        return;
    }
    for(int i = filterStart + 1 + pixelByteCount; i < filterStart + scanlineLen; i++) {
        if(print == 2) {
            printf("i %d filterStart %d pixelByteCount %d buffer[x] %d buffer[a] %d\n",
            i, filterStart, pixelByteCount, buffer[i], buffer[i - pixelByteCount]);
        }
        buffer[i] += buffer[i - pixelByteCount];
    }
}

void filter_up(unsigned char *buffer, int scanlineIndex, int scanlinePixelCount, int pixelByteCount, int print) {
    int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
    int filterStart = scanlineIndex * scanlineLen;
    if(buffer[filterStart] != 2) {
        return;
    }
    
    for(int i = filterStart + 1; i < filterStart + scanlineLen; i++) {
        if(print == 2) {
            printf("i %d filterStart %d pixelByteCount %d buffer[x] %d buffer[a] %d\n",
            i, filterStart, pixelByteCount, buffer[i], buffer[i - pixelByteCount]);
        }
        buffer[i] += buffer[i - scanlineLen];
    }
}

void filter_avg(unsigned char *buffer, int scanlineIndex, int scanlinePixelCount, int pixelByteCount, int print) {
    int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
    int filterStart = scanlineIndex * scanlineLen;
    if(buffer[filterStart] != 3) {
        return;
    }
    for (int i = 0; i < pixelByteCount; i++) {
        buffer[filterStart + 1 + i] += buffer[filterStart + 1 + i - scanlineLen] / 2;
    }
    for(int i = filterStart + 1 + pixelByteCount; i < filterStart + scanlineLen; i++) {
        if(print == 2) {
            printf("i %d filterStart %d pixelByteCount %d buffer[x] %d buffer[a] %d\n",
            i, filterStart, pixelByteCount, buffer[i], buffer[i - pixelByteCount]);
        }
        int x = ((int)buffer[i - pixelByteCount] + (int)buffer[i - scanlineLen]) / 2;
        buffer[i] += x;
    }
}

unsigned char paeth_predict(unsigned char a, unsigned char b, unsigned char c) {
    int p = a + b - c;
    unsigned int pa = abs(p - a);
    unsigned int pb = abs(p - b);
    unsigned int pc = abs(p - c);
    if((pa <= pb) && (pa <= pc)) {
        return a;
    } else if(pb <= pc) {
        return b;
    }
    return c;
}

void filter_paeth(unsigned char *buffer, int scanlineIndex, int scanlinePixelCount, int pixelByteCount, int print) {
    int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
    int filterStart = scanlineIndex * scanlineLen;
    if(buffer[filterStart] != 4) {
        return;
    }
    for (int i = 0; i < pixelByteCount; i++) {
        unsigned char b = buffer[filterStart + 1 + i - scanlineLen];
        unsigned char x = buffer[filterStart + 1 + i] + paeth_predict(0, b, 0);
        buffer[filterStart + 1 + i] = x;
    }
    for(int i = filterStart + 1 + pixelByteCount; i < filterStart + scanlineLen; i++) {
        if(print == 2) {
            printf("i %d filterStart %d pixelByteCount %d buffer[x] %d buffer[a] %d\n",
            i, filterStart, pixelByteCount, buffer[i], buffer[i - pixelByteCount]);
        }
        unsigned char a = buffer[i - pixelByteCount];
        unsigned char b = buffer[i - scanlineLen];
        unsigned char c = buffer[i - pixelByteCount - scanlineLen];
        unsigned char x = buffer[i] + paeth_predict(a, b, c);
        buffer[i] = x;
    }
}

void test() {
    unsigned char result[] = {0, 5, 2, 4, 3, 4, 3, 2, 5, 6, 4, 2, 3, 4, 5};
    const unsigned char expected[] = {0, 5, 2, 4, 3, 4, 3, 4, 9, 15, 4, 2, 3, 4, 5};
    filter_paeth(result, 1, 4, 1, 1);
    filter_avg(result, 2, 4, 1, 1);
    printf("Result:\n");
    print_buffer(result, 15);
    printf("Expected:\n");
    print_buffer(expected, 15);
}

int main(int argc, char *argv[]) {
 /* printf("%d\n", argc);
    for(int i = 0; i < argc; i++) {
        printf("%s\n", argv[i]);        
    } */
    if (argc < 3) {
        printf("Not enough arguments\n");
        return 1;
    }
    int print = 0;
    if(argc >= 4) {
        printf("Value of 4th arg is %s\n", argv[3]);
        if(strcmp(argv[3], "debug1") == 0) {
            print = 1;
        } else if(strcmp(argv[3], "debug2") == 0) {
            print = 2;
        }
    }
    FILE* mosaicOriginal = fopen(argv[1], "rb");
    FILE* uncompressedMosaic = fopen(argv[2], "wb");
    if(mosaicOriginal != NULL) {
        fseek(mosaicOriginal, 0L, SEEK_END);
        int fileLen = ftell(mosaicOriginal);
        rewind(mosaicOriginal);
        unsigned char buffer[fileLen];
        size_t read = fread(buffer, fileLen, fileLen, mosaicOriginal);
        int chunkCount = 0;
        int chunkDataLength;
        int bytePerPixel = 0;
        int pixelPerScanline = 0;
        int scanlineCount = 0;
        for(int i = 8; i < fileLen; i += 12) {
            chunkDataLength = buffer[i + 3] + (buffer[i + 2] * 256) + (buffer[i + 1] * 65536) + (buffer[i] * 16777216);
            chunkCount++;
            if(print >= 1) {
                printf("Theres is %d byte(s) in chunk #%d\n", chunkDataLength, chunkCount);
                printf("%c %c %c %c\n", buffer[i + 4], buffer[i + 5], buffer[i + 6], buffer[i + 7]);
            }
            if(memcmp(&buffer[i + 4], "IHDR", 4) == 0) {
                pixelPerScanline = buffer[i + 11] + (buffer[i + 10] * 256) + (buffer[i + 9] * 65536) + (buffer[i + 8] * 16777216);
                scanlineCount = buffer[i + 15] + (buffer[i + 14] * 256) + (buffer[i + 13] * 65536) + (buffer[i + 12] * 16777216);
                
                if(buffer[i + 17] == 2) {
                    if(buffer[i + 16] == 8) {
                        bytePerPixel = 3;
                    } else {
                        bytePerPixel = 6;
                    }
                } else if(buffer[i + 17] == 4) {
                    if(buffer[i + 16] == 8) {
                        bytePerPixel = 2;
                    } else {
                        bytePerPixel = 4;
                    }
                } else if(buffer[i + 17] == 6) {
                    if(buffer[i + 16] == 8) {
                        bytePerPixel = 4;
                    } else {
                        bytePerPixel = 8;
                    }
                } else {
                    printf("Unable to process image");
                    return 1;
                }
                if(print >= 1) {
                    printf("PNG dimensions: %d * %d\n", pixelPerScanline, scanlineCount);
                    printf("Color Type: %d\n", buffer[i + 17]);
                    printf("Bit Depth: %d\n", buffer[i + 16]);    
                }
            } else if(memcmp(&buffer[i + 4], "IDAT", 4) == 0) {
                unsigned long uncompressedBufLen = 1000000;
                unsigned long *lenPtr = &uncompressedBufLen;
                unsigned char uncompressedBuffer[1000000];
                int status = uncompress(uncompressedBuffer, lenPtr, &buffer[8 + i], chunkDataLength);
                if(print == 1) {
                    printf("The status is: %d, %lu\n", status, uncompressedBufLen);
                    print_buffer(&buffer[8 + i], 16);
                }
                if (status != Z_OK) {
                    printf("Failed to decompress the buffer. Exiting\n");
                    return 1;
                }
                int scanlineLen = pixelPerScanline * bytePerPixel + 1;
                for(int scanlineIndex = 0; scanlineIndex < scanlineCount; scanlineIndex++) {
                    int scanlineStart = scanlineLen * scanlineIndex;
                    unsigned char filter_type = uncompressedBuffer[scanlineStart];
                    if(print >= 1) {
                        printf("[%d]Filter type is %d\n", scanlineIndex, filter_type);
                    }
                    if(filter_type == 1) {
                        filter_sub(uncompressedBuffer, scanlineIndex, pixelPerScanline, bytePerPixel, print);
                    } else if(filter_type == 2) {
                        filter_up(uncompressedBuffer, scanlineIndex, pixelPerScanline, bytePerPixel, print);
                    } else if(filter_type == 3) {
                        filter_avg(uncompressedBuffer, scanlineIndex, pixelPerScanline, bytePerPixel, print);
                    } else if(filter_type == 4) {
                        filter_paeth(uncompressedBuffer, scanlineIndex, pixelPerScanline, bytePerPixel, print);
                    } else if(filter_type != 0) {
                        printf("Unknown filter type: %d\n", filter_type);
                    } 
                    size_t written = fwrite(&uncompressedBuffer[scanlineStart + 1], 1, scanlineLen - 1, uncompressedMosaic);
                    if(print >= 1) {
                        printf("Wrote %zu bytes\n", written);
                    }
                }
            } 
            i += chunkDataLength;
        }
        if(print >= 1) {
            printf("The chunk count is: %d\n", chunkCount);
        }
        fclose(mosaicOriginal);
        fclose(uncompressedMosaic);
        return 0;
    } else {
        printf("File not found\n");
        return 1;
    }
}