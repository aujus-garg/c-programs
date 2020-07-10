#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include "Stack.h"

#define SEGMENT_DIMENSION (16)

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

int mosaicGCF(int value1, int value2) {
  if (value1 == 0 || value2 == 0) {
    return 0;
  }

  int temp = value1;
  value1 = (value1 > value2) ? value1 : value2;
  value2 = (temp != value1) ? temp : value2;

  while (value1 % value2 != 0) {
    temp = value1 % value2;
    value1 = value2;
    value2 = temp;
  }

  return value2;
}

void recon_sub(unsigned char* scanline,
               int scanlinePixelCount,
               int pixelByteCount,
               int print,
               struct avgCol* total) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  if (scanline[0] != 1) {
    return;
  }

  for (int i = 1 + pixelByteCount; i < scanlineLen; i++) {
    if (print == 2) {
      printf("i %d pixelByteCount %d buffer[x] %d buffer[a] %d\n", i,
             pixelByteCount, scanline[i], scanline[i - pixelByteCount]);
    }
    scanline[i] += scanline[i - pixelByteCount];
    colAdd(total, scanline[i], (i - 1) % pixelByteCount);
  }
}

void recon_up(unsigned char* curScanline,
              unsigned char* prevScanline,
              int scanlinePixelCount,
              int pixelByteCount,
              int print,
              struct avgCol* total) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  if (curScanline[0] != 2) {
    return;
  }

  for (int i = 1; i < scanlineLen; i++) {
    if (print == 2) {
      printf("i %d pixelByteCount %d buffer[x] %d buffer[a] %d\n", i,
             pixelByteCount, curScanline[i], prevScanline[i]);
    }
    curScanline[i] += prevScanline[i];
    colAdd(total, curScanline[i], (i - 1) % pixelByteCount);
  }
}

void recon_avg(unsigned char* curScanline,
               unsigned char* prevScanline,
               int scanlinePixelCount,
               int pixelByteCount,
               int print,
               struct avgCol* total) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  if (curScanline[0] != 3) {
    return;
  }

  for (int i = 0; i < pixelByteCount; i++) {
    curScanline[1 + i] += prevScanline[1 + i] / 2;
    colAdd(total, curScanline[1 + i], i);
  }
  for (int i = 1 + pixelByteCount; i < scanlineLen; i++) {
    if (print == 2) {
      printf("i %d pixelByteCount %d buffer[x] %d buffer[a] %d\n", i,
             pixelByteCount, curScanline[i - pixelByteCount], prevScanline[i]);
    }
    int x = ((int)curScanline[i - pixelByteCount] + (int)prevScanline[i]) / 2;
    curScanline[i] += x;
    colAdd(total, curScanline[i], (i - 1) % pixelByteCount);
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

void recon_paeth(unsigned char* curScanline,
                 unsigned char* prevScanline,
                 int scanlinePixelCount,
                 int pixelByteCount,
                 int print,
                 struct avgCol* total) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  if (curScanline[0] != 4) {
    return;
  }
  for (int i = 0; i < pixelByteCount; i++) {
    unsigned char b = prevScanline[1 + i];
    unsigned char x = curScanline[1 + i] + paeth_predict(0, b, 0);
    curScanline[1 + i] = x;
    colAdd(total, curScanline[1 + i], i);
  }
  for (int i = 1 + pixelByteCount; i < scanlineLen; i++) {
    if (print == 2) {
      printf("i %d pixelByteCount %d buffer[x] %d buffer[a] %d\n", i,
             pixelByteCount, curScanline[i], curScanline[i - pixelByteCount]);
    }
    unsigned char a = curScanline[i - pixelByteCount];
    unsigned char b = prevScanline[i];
    unsigned char c = prevScanline[i - pixelByteCount];
    unsigned char x = curScanline[i] + paeth_predict(a, b, c);
    curScanline[i] = x;
    colAdd(total, curScanline[i], (i - 1) % pixelByteCount);
  }
}

int recon_image(unsigned char* newScanline,
                unsigned char* prevScanline,
                int pixelPerScanline,
                int bytePerPixel,
                int print,
                FILE* inflatedMosaic,
                struct avgCol* total,
                struct avgCol* segments,
                int lineNum,
                int arrayXVal) {
  int scanlineLen = pixelPerScanline * bytePerPixel + 1;
  unsigned char filter_type = newScanline[0];
  if (print >= 2) {
    printf("Filter type is %d\n", filter_type);
  }
  if (filter_type == 1) {
    recon_sub(newScanline, pixelPerScanline, bytePerPixel, print, total);
  } else if (filter_type == 2) {
    if (prevScanline == NULL) {
      printf("No previous scanline detected\n");
      return 1;
    }
    recon_up(newScanline, prevScanline, pixelPerScanline, bytePerPixel, print,
             total);
  } else if (filter_type == 3) {
    if (prevScanline == NULL) {
      printf("No previous scanline detected\n");
      return 1;
    }
    recon_avg(newScanline, prevScanline, pixelPerScanline, bytePerPixel, print,
              total);
  } else if (filter_type == 4) {
    if (prevScanline == NULL) {
      printf("No previous scanline detected\n");
      return 1;
    }
    recon_paeth(newScanline, prevScanline, pixelPerScanline, bytePerPixel,
                print, total);
  } else if (filter_type != 0) {
    printf("Unknown filter type: %d\n", filter_type);
  }
  size_t written =
      fwrite(&newScanline[0 + 1], 1, scanlineLen - 1, inflatedMosaic);
  for (int linePos = 0; linePos < scanlineLen - 1; linePos++) {
    colAdd(&segments[(linePos / bytePerPixel) / SEGMENT_DIMENSION +
                     (lineNum / SEGMENT_DIMENSION) * arrayXVal],
           newScanline[linePos + 1], linePos % bytePerPixel);
  }
  if (print >= 2) {
    printf("Filter type %d, wrote %zu bytes\n", filter_type, written);
  }
  return 0;
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    printf("Not enough arguments\n");
    return 1;
  }

  int print = 0;
  if (argc >= 3) {
    if (strcmp(argv[2], "debug1") == 0) {
      print = 1;
    } else if (strcmp(argv[2], "debug2") == 0) {
      print = 2;
    }
  }

  FILE* mosaicOriginal = fopen(argv[1], "rb");
  if (mosaicOriginal == NULL) {
    printf("File %s could not be opened\n", argv[1]);
    return 1;
  }

  FILE* inflatedMosaic =
      fopen("/Users/aujus.garg/github/testing_files/test.raw", "wb");
  if (inflatedMosaic == NULL) {
    printf(
        "File /Users/aujus.garg/github/testing_files/test.raw could not be "
        "opened\n");
    return 1;
  }

  FILE* pixelizedImage =
      fopen("/Users/aujus.garg/github/testing_files/pixelimage.raw", "wb");
  if (inflatedMosaic == NULL)
    printf("File /Users/aujus.garg/github/testing_files/pixelimage.raw");

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
  int lineNum = 0;
  int arrayXVal = 0;
  int arrayYVal = 0;
  int ret;

  struct avgCol* segments;

  struct avgCol total;
  total.r = 0;
  total.g = 0;
  total.b = 0;
  total.a = 0;

  int scanlineFilled = 0;

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
      printf("%c %c %c %c\n\n", buffer[i + 4], buffer[i + 5], buffer[i + 6],
             buffer[i + 7]);
    }

    if (memcmp(&buffer[i + 4], "IHDR", 4) == 0) {
      pixelPerScanline = buffer[i + 11] + (buffer[i + 10] * 256) +
                         (buffer[i + 9] * 65536) + (buffer[i + 8] * 16777216);
      scanlineCount = buffer[i + 15] + (buffer[i + 14] * 256) +
                      (buffer[i + 13] * 65536) + (buffer[i + 12] * 16777216);
      arrayXVal = pixelPerScanline / SEGMENT_DIMENSION;
      arrayYVal = scanlineCount / SEGMENT_DIMENSION;
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
        printf("Unable to process bit depth and color type\n");
        fclose(mosaicOriginal);
        fclose(inflatedMosaic);
        return 1;
      }
      if (print >= 1) {
        printf("\nPNG dimensions: %d * %d\n", pixelPerScanline, scanlineCount);
        printf("Color Type: %d\n", buffer[i + 17]);
        printf("Bit Depth: %d\n", buffer[i + 16]);
        printf("Bytes per pixel: %d\n\n", bytePerPixel);
        printf(
            "The amount of segments width-wise is %d \nand the amount of "
            "segments length-wise is %d\n\n",
            arrayXVal, arrayYVal);
      }
      bytesPerScanline = pixelPerScanline * bytePerPixel + 1;
      inflatedBuffer = (unsigned char*)malloc(bytesPerScanline * 2);
      segments =
          (struct avgCol*)malloc(sizeof(struct avgCol) * arrayXVal * arrayYVal);
      for (int counter = 0; counter < arrayXVal * arrayYVal; counter++) {
        segments[counter].r = 0;
        segments[counter].g = 0;
        segments[counter].b = 0;
        segments[counter].a = 0;
      }
    } else if (memcmp(&buffer[i + 4], "IDAT", 4) == 0) {
      if (inflatedBuffer == NULL) {
        printf("No header chunk\n");
        return 1;
      }

      strm.next_in = &buffer[i + 8];
      strm.avail_in = chunkDataLength;

      while (lineNum < scanlineCount) {
        unsigned char* curScanline =
            &inflatedBuffer[(lineNum % 2) * bytesPerScanline];
        unsigned char* prevScanline =
            (lineNum == 0)
                ? NULL
                : &inflatedBuffer[((lineNum - 1) % 2) * bytesPerScanline];

        strm.next_out = &curScanline[scanlineFilled];
        strm.avail_out = bytesPerScanline - scanlineFilled;
        ret = inflate(&strm, Z_NO_FLUSH);
        scanlineFilled = strm.total_out - lineNum * bytesPerScanline;
        if (scanlineFilled >= bytesPerScanline) {
          recon_image(curScanline, prevScanline, pixelPerScanline, bytePerPixel,
                      print, inflatedMosaic, &total, segments, lineNum,
                      arrayXVal);
          lineNum++;
          scanlineFilled -= bytesPerScanline;
        } else {
          printf("Failed to inflate a full scanline\n");
          break;
        }

        if (ret == Z_STREAM_END) {
          inflateEnd(&strm);
        } else if (ret != Z_OK) {
          inflateEnd(&strm);
          printf("Failed to decompress the buffer. Exiting... ERRNO %d\n", ret);
          fclose(mosaicOriginal);
          fclose(inflatedMosaic);
          free(inflatedBuffer);
          return 1;
        }
      }
    }
    i += chunkDataLength;
  }

  if (print >= 1) {
    printf("The chunk count is: %d\n\n", chunkCount);
  }
  int pixelCount = pixelPerScanline * scanlineCount;
  total.r = total.r / pixelCount;
  total.g = total.g / pixelCount;
  total.b = total.b / pixelCount;
  total.a = total.a / pixelCount;
  if (print >= 1) {
    printf("The RGB(A) is %d %d %d (%d)\n", total.r, total.g, total.b, total.a);
  }
  for (int i = 0; i < arrayXVal * arrayYVal; i++) {
    segments[i].r = segments[i].r / 256;
    segments[i].g = segments[i].g / 256;
    segments[i].b = segments[i].b / 256;
    segments[i].a = segments[i].a / 256;
    if (print >= 2) {
      printf("%d %d %d (%d)\n", segments[i].r, segments[i].g, segments[i].b,
             segments[i].a);
    }
    for (int j = 0; j < bytePerPixel; j++) {
      if (j == 0) {
        fwrite(&segments[i].r, 1, 1, pixelizedImage);
      } else if (j == 1) {
        fwrite(&segments[i].g, 1, 1, pixelizedImage);
      } else if (j == 2) {
        fwrite(&segments[i].b, 1, 1, pixelizedImage);
      } else if (j == 3) {
        fwrite(&segments[i].a, 1, 1, pixelizedImage);
      }
    }
  }
  fclose(mosaicOriginal);
  fclose(inflatedMosaic);
  fclose(pixelizedImage);
  free(inflatedBuffer);
  free(segments);

  return 0;
}