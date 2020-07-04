#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "Stack.h"

void print_buffer(const unsigned char* buf, size_t len) {
  char output[50];
  size_t offset = 0;
  for (unsigned int i = 0; i < len; i++) {
    if (i % 8 == 0 && offset > 0) {
      printf("[%08x] %s\n", i / 8 * 8, output);
      offset = 0;
    }
    offset += snprintf(&output[offset], sizeof(output) - offset, "0x%02x, ",
                       (unsigned char)buf[i]);
  }
  if (offset > 0) {
    printf("[%08lx] %s\n", len / 8 * 8, output);
  }
}

struct avgCol {
  int r;
  int g;
  int b;
  int a;
};

void colAdd(struct avgCol* total, unsigned char bufferCol, int rgbaSelect) {
  if (rgbaSelect == 0) {
    total->r += bufferCol;
  } else if (rgbaSelect == 1) {
    total->g += bufferCol;
  } else if (rgbaSelect == 2) {
    total->b += bufferCol;
  } else if (rgbaSelect == 3) {
    total->a += bufferCol;
  }
}

void recon_sub(unsigned char* buffer,
               int scanlineIndex,
               int scanlinePixelCount,
               int pixelByteCount,
               int print,
               struct avgCol* total) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  int scanlineStart = scanlineIndex * scanlineLen;
  if (buffer[scanlineStart] != 1) {
    return;
  }

  for (int i = scanlineStart + 1 + pixelByteCount;
       i < scanlineStart + scanlineLen; i++) {
    if (print == 2) {
      printf(
          "i %d scanlineStart %d pixelByteCount %d buffer[x] %d buffer[a] %d\n",
          i, scanlineStart, pixelByteCount, buffer[i],
          buffer[i - pixelByteCount]);
    }
    buffer[i] += buffer[i - pixelByteCount];
    colAdd(total, buffer[i], (i - scanlineStart - 1) % pixelByteCount);
  }
}

void recon_up(unsigned char* buffer,
              int scanlineIndex,
              int scanlinePixelCount,
              int pixelByteCount,
              int print,
              struct avgCol* total) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  int scanlineStart = scanlineIndex * scanlineLen;
  if (buffer[scanlineStart] != 2) {
    return;
  }

  for (int i = scanlineStart + 1; i < scanlineStart + scanlineLen; i++) {
    if (print == 2) {
      printf(
          "i %d scanlineStart %d pixelByteCount %d buffer[x] %d buffer[a] %d\n",
          i, scanlineStart, pixelByteCount, buffer[i],
          buffer[i - pixelByteCount]);
    }
    buffer[i] += buffer[i - scanlineLen];
    colAdd(total, buffer[i], (i - scanlineStart - 1) % pixelByteCount);
  }
}

void recon_avg(unsigned char* buffer,
               int scanlineIndex,
               int scanlinePixelCount,
               int pixelByteCount,
               int print,
               struct avgCol* total) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  int scanlineStart = scanlineIndex * scanlineLen;
  if (buffer[scanlineStart] != 3) {
    return;
  }

  for (int i = 0; i < pixelByteCount; i++) {
    buffer[scanlineStart + 1 + i] +=
        buffer[scanlineStart + 1 + i - scanlineLen] / 2;
    colAdd(total, buffer[scanlineStart + 1 + i], i);
  }
  for (int i = scanlineStart + 1 + pixelByteCount;
       i < scanlineStart + scanlineLen; i++) {
    if (print == 2) {
      printf(
          "i %d scanlineStart %d pixelByteCount %d buffer[x] %d buffer[a] %d\n",
          i, scanlineStart, pixelByteCount, buffer[i],
          buffer[i - pixelByteCount]);
    }
    int x =
        ((int)buffer[i - pixelByteCount] + (int)buffer[i - scanlineLen]) / 2;
    buffer[i] += x;
    colAdd(total, buffer[i], (i - scanlineStart - 1) % pixelByteCount);
  }
}

unsigned char paeth_predict(unsigned char a, unsigned char b, unsigned char c) {
  int p = a + b - c;
  unsigned int pa = abs(p - a);
  unsigned int pb = abs(p - b);
  unsigned int pc = abs(p - c);
  if ((pa <= pb) && (pa <= pc)) {
    return a;
  } else if (pb <= pc) {
    return b;
  }
  return c;
}

void recon_paeth(unsigned char* buffer,
                 int scanlineIndex,
                 int scanlinePixelCount,
                 int pixelByteCount,
                 int print,
                 struct avgCol* total) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  int scanlineStart = scanlineIndex * scanlineLen;
  if (buffer[scanlineStart] != 4) {
    return;
  }
  for (int i = 0; i < pixelByteCount; i++) {
    unsigned char b = buffer[scanlineStart + 1 + i - scanlineLen];
    unsigned char x = buffer[scanlineStart + 1 + i] + paeth_predict(0, b, 0);
    buffer[scanlineStart + 1 + i] = x;
    colAdd(total, buffer[scanlineStart + 1 + i], i);
  }
  for (int i = scanlineStart + 1 + pixelByteCount;
       i < scanlineStart + scanlineLen; i++) {
    if (print == 2) {
      printf(
          "i %d scanlineStart %d pixelByteCount %d buffer[x] %d buffer[a] %d\n",
          i, scanlineStart, pixelByteCount, buffer[i],
          buffer[i - pixelByteCount]);
    }
    unsigned char a = buffer[i - pixelByteCount];
    unsigned char b = buffer[i - scanlineLen];
    unsigned char c = buffer[i - pixelByteCount - scanlineLen];
    unsigned char x = buffer[i] + paeth_predict(a, b, c);
    buffer[i] = x;
    colAdd(total, buffer[i], (i - scanlineStart - 1) % pixelByteCount);
  }
}

void recon_image(unsigned char* inflatedBuffer,
                 int pixelPerScanline,
                 int scanlineCount,
                 int bytePerPixel,
                 int print,
                 FILE* inflatedMosaic,
                 struct avgCol* total) {
  int scanlineLen = pixelPerScanline * bytePerPixel + 1;
  for (int scanlineIndex = 0; scanlineIndex < scanlineCount; scanlineIndex++) {
    int scanlineStart = scanlineLen * scanlineIndex;
    unsigned char filter_type = inflatedBuffer[scanlineStart];
    if (print >= 2) {
      printf("[%d]Filter type is %d\n", scanlineIndex, filter_type);
    }
    if (filter_type == 1) {
      recon_sub(inflatedBuffer, scanlineIndex, pixelPerScanline, bytePerPixel,
                print, total);
    } else if (filter_type == 2) {
      recon_up(inflatedBuffer, scanlineIndex, pixelPerScanline, bytePerPixel,
               print, total);
    } else if (filter_type == 3) {
      recon_avg(inflatedBuffer, scanlineIndex, pixelPerScanline, bytePerPixel,
                print, total);
    } else if (filter_type == 4) {
      recon_paeth(inflatedBuffer, scanlineIndex, pixelPerScanline, bytePerPixel,
                  print, total);
    } else if (filter_type != 0) {
      printf("Unknown filter type: %d\n", filter_type);
    }
    size_t written = fwrite(&inflatedBuffer[scanlineStart + 1], 1,
                            scanlineLen - 1, inflatedMosaic);
    if (print >= 1) {
      printf("Filter type %d, wrote %zu bytes\n", filter_type, written);
    }
  }
}

/*
void test() {
  unsigned char result[] = {0, 5, 2, 4, 3, 4, 3, 2, 5, 6, 4, 2, 3, 4, 5};
  const unsigned char expected[] = {0, 5,  2, 4, 3, 4, 3, 4,
                                    9, 15, 4, 2, 3, 4, 5};
  recon_paeth(result, 1, 4, 1, 1);
  recon_avg(result, 2, 4, 1, 1);
  printf("Result:\n");
  print_buffer(result, 15);
  printf("Expected:\n");
  print_buffer(expected, 15);
}
*/

int main(int argc, char* argv[]) {
  if (argc < 3) {
    printf("Not enough arguments\n");
    return 1;
  }

  int print = 0;
  if (argc >= 4) {
    if (strcmp(argv[3], "debug1") == 0) {
      print = 1;
    } else if (strcmp(argv[3], "debug2") == 0) {
      print = 2;
    }
  }

  FILE* mosaicOriginal = fopen(argv[1], "rb");
  if (mosaicOriginal == NULL) {
    printf("File %s could not be opened\n", argv[1]);
    return 1;
  }

  FILE* inflatedMosaic = fopen(argv[2], "wb");
  if (inflatedMosaic == NULL) {
    printf("File %s could not be opened\n", argv[2]);
    return 1;
  }

  fseek(mosaicOriginal, 0L, SEEK_END);
  int fileLen = ftell(mosaicOriginal);
  rewind(mosaicOriginal);

  unsigned char buffer[fileLen];
  unsigned char* inflatedBuffer = NULL;
  size_t read = fread(buffer, fileLen, fileLen, mosaicOriginal);

  int chunkCount = 0;
  int chunkDataLength;
  int bytePerPixel = 0;
  int pixelPerScanline = 0;
  int scanlineCount = 0;
  int bytesPerScanline = 0;
  int ret;

  struct avgCol total;
  total.r = 0;
  total.g = 0;
  total.b = 0;
  total.a = 0;

  int processed_out = 0;

  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;
  strm.total_out = 0;
  strm.total_in = 0;

  ret = inflateInit(&strm);
  if (ret != Z_OK) {
    printf("%d\n", ret);
    return ret;
  }

  for (int i = 8; i < fileLen; i += 12) {
    chunkDataLength = buffer[i + 3] + (buffer[i + 2] * 256) +
                      (buffer[i + 1] * 65536) + (buffer[i] * 16777216);
    chunkCount++;

    if (print >= 1) {
      printf("[%d]Theres is %d byte(s) in chunk #%d\n", i, chunkDataLength,
             chunkCount);
      printf("%c %c %c %c\n", buffer[i + 4], buffer[i + 5], buffer[i + 6],
             buffer[i + 7]);
    }

    if (memcmp(&buffer[i + 4], "IHDR", 4) == 0) {
      pixelPerScanline = buffer[i + 11] + (buffer[i + 10] * 256) +
                         (buffer[i + 9] * 65536) + (buffer[i + 8] * 16777216);
      scanlineCount = buffer[i + 15] + (buffer[i + 14] * 256) +
                      (buffer[i + 13] * 65536) + (buffer[i + 12] * 16777216);
      if (buffer[i + 17] == 2) {
        if (buffer[i + 16] == 8) {
          bytePerPixel = 3;
        } else {
          bytePerPixel = 6;
        }
      } else if (buffer[i + 17] == 4) {
        if (buffer[i + 16] == 8) {
          bytePerPixel = 2;
        } else {
          bytePerPixel = 4;
        }
      } else if (buffer[i + 17] == 6) {
        if (buffer[i + 16] == 8) {
          bytePerPixel = 4;
        } else {
          bytePerPixel = 8;
        }
      } else {
        printf("Unable to process image");
        return 1;
      }
      if (print >= 1) {
        printf("PNG dimensions: %d * %d\n", pixelPerScanline, scanlineCount);
        printf("Color Type: %d\n", buffer[i + 17]);
        printf("Bit Depth: %d\n", buffer[i + 16]);
        printf("Bytes per pixel: %d\n", bytePerPixel);
      }
      bytesPerScanline = pixelPerScanline * bytePerPixel + 1;
      inflatedBuffer = (unsigned char*)malloc(bytesPerScanline * scanlineCount);
    } else if (memcmp(&buffer[i + 4], "IDAT", 4) == 0) {
      if (inflatedBuffer == NULL) {
        printf("No header chunk\n");
        return 1;
      }

      strm.next_in = &buffer[i + 8];
      strm.avail_in = chunkDataLength;
      strm.next_out = &inflatedBuffer[strm.total_out];
      strm.avail_out =
          bytesPerScanline * scanlineCount + scanlineCount - strm.total_out;

      ret = inflate(&strm, Z_NO_FLUSH);
      if (strm.total_out - processed_out > bytesPerScanline) {
        int numLines = (strm.total_out - processed_out) / bytesPerScanline;
        recon_image(&inflatedBuffer[processed_out], pixelPerScanline, numLines,
                    bytePerPixel, print, inflatedMosaic, &total);
        processed_out += numLines * bytesPerScanline;
      }

      if (ret == Z_STREAM_END) {
        inflateEnd(&strm);
      } else if (ret != Z_OK) {
        inflateEnd(&strm);
        printf("Failed to decompress the buffer. Exiting... ERRNO %d\n", ret);
        return 1;
      }
      int pixelCount = pixelPerScanline * scanlineCount;
      total.r = total.r / pixelCount;
      total.g = total.g / pixelCount;
      total.b = total.b / pixelCount;
      total.a = total.a / pixelCount;
      printf("The RGB(A) is %d %d %d (%d)\n", total.r, total.g, total.b,
             total.a);
    }
    i += chunkDataLength;
  }

  if (print >= 1) {
    printf("The chunk count is: %d\n", chunkCount);
  }

  fclose(mosaicOriginal);
  fclose(inflatedMosaic);
  free(inflatedBuffer);

  return 0;
}