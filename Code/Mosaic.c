#include <arpa/inet.h>
#include <dirent.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define SEGMENT_DIMENSION (8)

int debugMosaic = 0;

void printBuffer(const unsigned char* buf, size_t len) {
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

uint32_t rc_crc32(uint32_t crc, unsigned char* buf, size_t len) {
  static uint32_t table[256];
  static int have_table = 0;
  uint32_t rem;
  uint8_t octet;
  int i, j;
  unsigned char *p, *q;

  /* This check is not thread safe; there is no mutex. */
  if (have_table == 0) {
    /* Calculate CRC table. */
    for (i = 0; i < 256; i++) {
      rem = i; /* remainder from polynomial division */
      for (j = 0; j < 8; j++) {
        if (rem & 1) {
          rem >>= 1;
          rem ^= 0xedb88320;
        } else
          rem >>= 1;
      }
      table[i] = rem;
    }
    have_table = 1;
  }

  crc = ~crc;
  q = buf + len;
  for (p = buf; p < q; p++) {
    octet = *p; /* Cast to unsigned octet. */
    crc = (crc >> 8) ^ table[(crc & 0xff) ^ octet];
  }
  return ~crc;
}

struct AverageColor {
  int r;
  int g;
  int b;
  int a;
};

struct IHDRChunk {
  unsigned char width[4];
  unsigned char height[4];
  unsigned char bitDepth;
  unsigned char colorType;
  unsigned char remainingVals[3];
};

void aggregateColors(struct AverageColor* color,
                     unsigned char bufferCol,
                     int byteLoc,
                     int pixelByteCount) {
  int rgbaSelect = byteLoc % pixelByteCount;
  switch (rgbaSelect) {
    case (0):
      color->r += bufferCol;
      break;
    case (1):
      color->g += bufferCol;
      break;
    case (2):
      color->b += bufferCol;
      break;
    case (3):
      color->a += bufferCol;
      break;
  }
}

struct node;

struct node {
  struct AverageColor sourceTotal;
  struct node* next;
};

struct node* head = NULL;

void insertNode(struct AverageColor sourceTotal) {
  struct node* new_node = (struct node*)malloc(sizeof(struct node));
  new_node->sourceTotal.r = sourceTotal.r;
  new_node->sourceTotal.g = sourceTotal.g;
  new_node->sourceTotal.b = sourceTotal.b;
  new_node->sourceTotal.a = 0;
  new_node->next = head;
  head = new_node;
}

char* pngPath(char* directoryPath, char* pngName) {
  int pathSize = strlen(directoryPath) + strlen(pngName) + 2;
  char* imagePath = malloc(pathSize);
  snprintf(imagePath, pathSize, "%s/%s", directoryPath, pngName);
  return imagePath;
}

char* pixelizedPath(char* directoryPath, struct AverageColor pixelizedTotal) {
  int pixelizedPathLen = strlen(directoryPath) + 21;
  char* pixelizedImagePath = malloc(pixelizedPathLen);
  snprintf(pixelizedImagePath, pixelizedPathLen, "%s/%d_%d_%d_%d.raw",
           directoryPath, pixelizedTotal.r, pixelizedTotal.g, pixelizedTotal.b,
           0);
  return pixelizedImagePath;
}

int rgbaDistance(struct AverageColor sourceRGBA,
                 struct AverageColor targetRGBA) {
  return sqrt(pow(targetRGBA.r - sourceRGBA.r, 2) +
              pow(targetRGBA.g - sourceRGBA.g, 2) +
              pow(targetRGBA.b - sourceRGBA.b, 2));
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

void newDimensions(int x,
                   int y,
                   int* segCountX,
                   int* segCountY,
                   int* segPixelsX,
                   int* segPixelsY) {
  int targetGCF = mosaicGCF(x, y);
  *segPixelsX = x / targetGCF;
  *segPixelsY = y / targetGCF;
  int tempSegPixelsX = *segPixelsX;
  int tempSegPixelsY = *segPixelsY;
  while (*segPixelsX < SEGMENT_DIMENSION && *segPixelsY < SEGMENT_DIMENSION) {
    *segPixelsX += tempSegPixelsX;
    *segPixelsY += tempSegPixelsY;
  }
  *segCountX = x / *segPixelsX;
  *segCountY = y / *segPixelsY;
}

int roundUp(int dividend, int divisor) {
  return dividend / divisor + !!(dividend % divisor);
}

int calculateSegmentIndex(int bytePosition,
                          int bytesPerPixel,
                          int segmentX,
                          int segmentY,
                          int scanline,
                          int pixelsPerScanline) {
  int index = (bytePosition / bytesPerPixel) / segmentX +
              (scanline / segmentY) * (pixelsPerScanline / segmentX);
  if (debugMosaic >= 3) {
    printf("[%d][%d] = [%d]\n", bytePosition, scanline, index);
  }
  return index;
}

void reconSub(unsigned char* scanline,
              int scanlinePixelCount,
              int pixelByteCount,
              struct AverageColor* segments,
              int lineNum,
              int segPixelsX,
              int segPixelsY) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  if (scanline[0] != 1) {
    return;
  }

  for (int i = 0; i < pixelByteCount; i++) {
    struct AverageColor* segment = &segments[calculateSegmentIndex(
        i, pixelByteCount, segPixelsX, segPixelsY, lineNum,
        scanlinePixelCount)];
    aggregateColors(segment, scanline[i + 1], i, pixelByteCount);
  }

  for (int i = 1 + pixelByteCount; i < scanlineLen; i++) {
    if (debugMosaic >= 2) {
      printf("i %d pixelByteCount %d buffer[x] %d buffer[a] %d\n", i,
             pixelByteCount, scanline[i], scanline[i - pixelByteCount]);
    }
    scanline[i] += scanline[i - pixelByteCount];
    struct AverageColor* segment = &segments[calculateSegmentIndex(
        i - 1, pixelByteCount, segPixelsX, segPixelsY, lineNum,
        scanlinePixelCount)];
    aggregateColors(segment, scanline[i], i - 1, pixelByteCount);
  }
}

void filterSub(unsigned char* scanline,
               int scanlinePixelCount,
               int pixelByteCount) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  if (scanline[0] != 1) {
    scanline[0] = 0;
    return;
  }
  for (int i = scanlineLen - 1; i > pixelByteCount; i--) {
    scanline[i] -= scanline[i - pixelByteCount];
  }
}

void reconUp(unsigned char* curScanline,
             unsigned char* prevScanline,
             int scanlinePixelCount,
             int pixelByteCount,
             struct AverageColor* segments,
             int lineNum,
             int segPixelsX,
             int segPixelsY) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  if (curScanline[0] != 2) {
    return;
  }

  for (int i = 1; i < scanlineLen; i++) {
    if (debugMosaic >= 2) {
      printf("i %d pixelByteCount %d buffer[x] %d buffer[a] %d\n", i,
             pixelByteCount, curScanline[i], prevScanline[i]);
    }
    curScanline[i] += prevScanline[i];
    struct AverageColor* segment = &segments[calculateSegmentIndex(
        i - 1, pixelByteCount, segPixelsX, segPixelsY, lineNum,
        scanlinePixelCount)];
    aggregateColors(segment, curScanline[i], i - 1, pixelByteCount);
  }
}

void reconAvg(unsigned char* curScanline,
              unsigned char* prevScanline,
              int scanlinePixelCount,
              int pixelByteCount,
              struct AverageColor* segments,
              int lineNum,
              int segPixelsX,
              int segPixelsY) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  if (curScanline[0] != 3) {
    return;
  }

  for (int i = 0; i < pixelByteCount; i++) {
    curScanline[1 + i] += prevScanline[1 + i] / 2;
    struct AverageColor* segment = &segments[calculateSegmentIndex(
        i, pixelByteCount, segPixelsX, segPixelsY, lineNum,
        scanlinePixelCount)];
    aggregateColors(segment, curScanline[i + 1], i, pixelByteCount);
  }
  for (int i = 1 + pixelByteCount; i < scanlineLen; i++) {
    if (debugMosaic >= 2) {
      printf("i %d pixelByteCount %d buffer[x] %d buffer[a] %d\n", i,
             pixelByteCount, curScanline[i - pixelByteCount], prevScanline[i]);
    }
    int x = ((int)curScanline[i - pixelByteCount] + (int)prevScanline[i]) / 2;
    curScanline[i] += x;
    struct AverageColor* segment = &segments[calculateSegmentIndex(
        i - 1, pixelByteCount, segPixelsX, segPixelsY, lineNum,
        scanlinePixelCount)];
    aggregateColors(segment, curScanline[i], i - 1, pixelByteCount);
  }
}

unsigned char paethPredict(unsigned char a, unsigned char b, unsigned char c) {
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

void reconPaeth(unsigned char* curScanline,
                unsigned char* prevScanline,
                int scanlinePixelCount,
                int pixelByteCount,
                struct AverageColor* segments,
                int lineNum,
                int segPixelsX,
                int segPixelsY) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  if (curScanline[0] != 4) {
    return;
  }
  for (int i = 0; i < pixelByteCount; i++) {
    unsigned char b = prevScanline[1 + i];
    unsigned char x = curScanline[1 + i] + paethPredict(0, b, 0);
    curScanline[1 + i] = x;
    struct AverageColor* segment = &segments[calculateSegmentIndex(
        i, pixelByteCount, segPixelsX, segPixelsY, lineNum,
        scanlinePixelCount)];
    aggregateColors(segment, curScanline[i + 1], i, pixelByteCount);
  }
  for (int i = 1 + pixelByteCount; i < scanlineLen; i++) {
    if (debugMosaic >= 2) {
      printf("i %d pixelByteCount %d buffer[x] %d buffer[a] %d\n", i,
             pixelByteCount, curScanline[i], curScanline[i - pixelByteCount]);
    }
    unsigned char a = curScanline[i - pixelByteCount];
    unsigned char b = prevScanline[i];
    unsigned char c = prevScanline[i - pixelByteCount];
    unsigned char x = curScanline[i] + paethPredict(a, b, c);
    curScanline[i] = x;
    struct AverageColor* segment = &segments[calculateSegmentIndex(
        i - 1, pixelByteCount, segPixelsX, segPixelsY, lineNum,
        scanlinePixelCount)];
    aggregateColors(segment, curScanline[i], i - 1, pixelByteCount);
  }
}

void filterPaeth(unsigned char* curScanline,
                 unsigned char* prevScanline,
                 int scanlinePixelCount,
                 int pixelByteCount) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  if (curScanline[0] != 4) {
    curScanline[0] = 0;
    return;
  }
  for (int i = scanlineLen - 1; i > pixelByteCount; i--) {
    unsigned char a = curScanline[i - pixelByteCount];
    unsigned char b = prevScanline[i];
    unsigned char c = prevScanline[i - pixelByteCount];
    curScanline[i] -= paethPredict(a, b, c);
  }
  for (int i = 1; i <= pixelByteCount; i++) {
    unsigned char b = prevScanline[i];
    curScanline[i] -= paethPredict(0, b, 0);
  }
}

int reconImage(unsigned char* newScanline,
               unsigned char* prevScanline,
               int pixelPerScanline,
               int bytePerPixel,
               struct AverageColor* segments,
               int lineNum,
               int segCountX,
               int segPixelsX,
               int segPixelsY,
               bool ifTarget) {
  int scanlineLen = pixelPerScanline * bytePerPixel + 1;
  unsigned char filter_type = newScanline[0];
  if (debugMosaic >= 2) {
    printf("Filter type is %d\n", filter_type);
  }

  if (filter_type > 1 && prevScanline == NULL) {
    printf("No previous scanline detected\n");
    return 1;
  }

  switch (filter_type) {
    case 1:
      reconSub(newScanline, pixelPerScanline, bytePerPixel, segments, lineNum,
               segPixelsX, segPixelsY);
      break;
    case 2:
      reconUp(newScanline, prevScanline, pixelPerScanline, bytePerPixel,
              segments, lineNum, segPixelsX, segPixelsY);
      break;
    case 3:
      reconAvg(newScanline, prevScanline, pixelPerScanline, bytePerPixel,
               segments, lineNum, segPixelsX, segPixelsY);
      break;
    case 4:
      reconPaeth(newScanline, prevScanline, pixelPerScanline, bytePerPixel,
                 segments, lineNum, segPixelsX, segPixelsY);
      break;
    default:
      break;
  };
  /*if (ifTarget) {
    char* tempPath = malloc(34);
    snprintf(tempPath, 34, "/Users/aujus.garg/github/%d.raw", pixelPerScanline);
    FILE* inflatedMosaic = fopen(tempPath, "ab");
    printf("Writing %d data to %s file at offset %d\n",
           bytePerPixel * pixelPerScanline, tempPath,
           lineNum * bytePerPixel * pixelPerScanline);
    free(tempPath);
    size_t written = fwrite(&newScanline[1], 1, bytePerPixel * pixelPerScanline,
                            inflatedMosaic);
    printf("Wrote %zu bytes\n", written);
    fclose(inflatedMosaic);
  }*/
  return 0;
}

int processHeaderChunk(int* pixelPerScanline,
                       unsigned char* buffer,
                       int i,
                       int* scanlineCount,
                       int* segCountX,
                       int* segCountY,
                       int* bytePerPixel,
                       FILE* mosaicOriginal,
                       /*FILE* inflatedMosaic,*/
                       int* bytesPerScanline,
                       unsigned char* inflatedBuffer,
                       struct AverageColor* segments,
                       bool ifTarget,
                       int* segPixelsX,
                       int* segPixelsY) {
  memcpy(pixelPerScanline, &buffer[i + 8], sizeof(int));
  *pixelPerScanline = ntohl(*pixelPerScanline);
  memcpy(scanlineCount, &buffer[i + 12], sizeof(int));
  *scanlineCount = ntohl(*scanlineCount);
  if (ifTarget == true) {
    newDimensions(*pixelPerScanline, *scanlineCount, segCountX, segCountY,
                  segPixelsX, segPixelsY);
    if (debugMosaic >= 1) {
      printf("For target image %d, %d, %d, %d\n", *segCountX, *segCountY,
             *segPixelsX, *segPixelsY);
    }
  } else {
    *segCountX = roundUp(*pixelPerScanline, *segPixelsX);
    *segCountY = roundUp(*scanlineCount, *segPixelsY);
    if (debugMosaic >= 1) {
      printf("For src image %d, %d, %d, %d\n", *segCountX, *segCountY,
             *segPixelsX, *segPixelsY);
    }
  }
  switch (buffer[i + 17]) {
    case (2):
      *bytePerPixel = (buffer[i + 16] == 8) ? 3 : 6;
      break;
    case (4):
      *bytePerPixel = (buffer[i + 16] == 8) ? 2 : 4;
      break;
    case (6):
      *bytePerPixel = (buffer[i + 16] == 8) ? 4 : 8;
      break;
    default:
      printf("Unable to process bit depth and color type\n");
      return 1;
  }
  if (debugMosaic >= 1) {
    printf("PNG dimensions: %d * %d\n", *pixelPerScanline, *scanlineCount);
    printf("Color Type: %d\n", buffer[i + 17]);
    printf("Bit Depth: %d\n", buffer[i + 16]);
    printf("Bytes per pixel: %d\n\n", *bytePerPixel);
    printf(
        "The amount of segments width-wise is %d \nand the amount of "
        "segments length-wise is %d\n\n",
        *segCountX, *segCountY);
  }
  *bytesPerScanline = *pixelPerScanline * *bytePerPixel + 1;
  return 0;
}

int processIDATChunk(unsigned char* inflatedBuffer,
                     z_stream* strm,
                     unsigned char* buffer,
                     int bufferStart,
                     int chunkDataLength,
                     int* lineNum,
                     int scanlineCount,
                     int bytesPerScanline,
                     int* scanlineFilled,
                     int pixelPerScanline,
                     int bytePerPixel,
                     struct AverageColor* segments,
                     int segCountX,
                     FILE* mosaicOriginal,
                     int segPixelsX,
                     int segPixelsY,
                     bool ifTarget) {
  if (inflatedBuffer == NULL) {
    printf("No header chunk\n");
    return 1;
  }

  strm->next_in = &buffer[bufferStart + 8];
  strm->avail_in = chunkDataLength;
  int lineNumber = *lineNum;
  int scanlineOffset = *scanlineFilled;

  while (lineNumber < scanlineCount) {
    unsigned char* curScanline =
        &inflatedBuffer[(lineNumber % 2) * bytesPerScanline];
    unsigned char* prevScanline =
        (lineNumber == 0)
            ? NULL
            : &inflatedBuffer[((lineNumber - 1) % 2) * bytesPerScanline];

    strm->next_out = &curScanline[scanlineOffset];
    strm->avail_out = bytesPerScanline - scanlineOffset;
    int ret = inflate(strm, Z_NO_FLUSH);
    scanlineOffset = strm->total_out - lineNumber * bytesPerScanline;
    if (scanlineOffset >= bytesPerScanline) {
      if (reconImage(curScanline, prevScanline, pixelPerScanline, bytePerPixel,
                     /*inflatedMosaic,*/ segments, lineNumber, segCountX,
                     segPixelsX, segPixelsY, ifTarget) == 1) {
        return 1;
      }
      lineNumber++;
      scanlineOffset -= bytesPerScanline;
    } else {
      if (debugMosaic >= 1) {
        printf("Found an incomplete scanline, needs another IDAT chunk\n");
      }
      break;
    }

    if (ret == Z_STREAM_END) {
      inflateEnd(strm);
    } else if (ret != Z_OK) {
      inflateEnd(strm);
      printf("Failed to decompress the buffer. Exiting... ERRNO %d\n", ret);
      return 1;
    }
  }
  *lineNum = lineNumber;
  *scanlineFilled = scanlineOffset;
  return 0;
}

int processPNG(FILE* mosaicOriginal,
               struct AverageColor** final_segments,
               struct AverageColor* total,
               int* segCountX,
               int* segCountY,
               int* width,
               int* height,
               int* bytesPerPixel,
               bool ifTarget) {
  fseek(mosaicOriginal, 0L, SEEK_END);
  int fileLen = ftell(mosaicOriginal);
  rewind(mosaicOriginal);

  unsigned char* buffer = (unsigned char*)malloc(fileLen);
  unsigned char* inflatedBuffer = NULL;
  if (debugMosaic >= 1) {
    printf("Reading into %p, %d bytes, from %p\n\n", buffer, fileLen,
           mosaicOriginal);
  }
  size_t read = fread(buffer, 1, fileLen, mosaicOriginal);

  int chunkCount = 0;
  int chunkDataLength;
  int bytePerPixel = *bytesPerPixel;
  int bytesPerScanline = 0;
  int pixelPerScanline = *width;
  int scanlineCount = *height;
  int lineNum = 0;
  int ret;
  int scanlineFilled = 0;
  int segPixelsX = SEGMENT_DIMENSION;
  int segPixelsY = SEGMENT_DIMENSION;
  struct AverageColor* segments = NULL;

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
    memcpy(&chunkDataLength, &buffer[i], sizeof(chunkDataLength));
    chunkDataLength = ntohl(chunkDataLength);
    chunkCount++;

    if (debugMosaic >= 1) {
      printf("[%d]Theres is %d byte(s) in chunk #%d\n", i, chunkDataLength,
             chunkCount);
      printf("%c %c %c %c\n\n", buffer[i + 4], buffer[i + 5], buffer[i + 6],
             buffer[i + 7]);
    }

    if (memcmp(&buffer[i + 4], "IHDR", 4) == 0) {
      if (inflatedBuffer != NULL) {
        free(inflatedBuffer);
      }

      if (processHeaderChunk(&pixelPerScanline, buffer, i, &scanlineCount,
                             segCountX, segCountY, &bytePerPixel,
                             mosaicOriginal, &bytesPerScanline, inflatedBuffer,
                             segments, ifTarget, &segPixelsX,
                             &segPixelsY) != 0) {
        free(buffer);
        return 1;
      }

      inflatedBuffer = (unsigned char*)malloc(bytesPerScanline * 2);
      int size = sizeof(struct AverageColor) * *segCountX * *segCountY;
      segments = (struct AverageColor*)malloc(size);
      if (debugMosaic >= 2) {
        printf("x %d, y %d, size %d, ptr %p\n", *segCountX, *segCountY, size,
               segments);
      }
      memset(segments, 0, size);
    } else if (memcmp(&buffer[i + 4], "IDAT", 4) == 0) {
      if (debugMosaic >= 2) {
        printf("Scanline Count %d, current scan line %d\n", scanlineCount,
               lineNum);
      }
      if (processIDATChunk(inflatedBuffer, &strm, buffer, i, chunkDataLength,
                           &lineNum, scanlineCount, bytesPerScanline,
                           &scanlineFilled, pixelPerScanline, bytePerPixel,
                           segments, *segCountX, mosaicOriginal, segPixelsX,
                           segPixelsY, ifTarget) != 0) {
        if (inflatedBuffer != NULL) {
          free(inflatedBuffer);
        }
        free(segments);
        free(buffer);
        return 1;
      }
    }
    i += chunkDataLength;
  }
  if (debugMosaic >= 1) {
    printf("The chunk count is: %d\n\n", chunkCount);
  }
  int pixelsPerSegment = segPixelsX * segPixelsY;
  for (int i = 0; i < *segCountX * *segCountY; i++) {
    segments[i].r = segments[i].r / pixelsPerSegment;
    segments[i].g = segments[i].g / pixelsPerSegment;
    segments[i].b = segments[i].b / pixelsPerSegment;
    segments[i].a = segments[i].a / pixelsPerSegment;
    if (debugMosaic >= 2) {
      printf("[%d] %d %d %d (%d)\n", i, segments[i].r, segments[i].g,
             segments[i].b, segments[i].a);
    }
  }

  memset(total, 0, sizeof(struct AverageColor));
  for (int i = 0; i < *segCountX * *segCountY; i++) {
    total->r += segments[i].r;
    total->g += segments[i].g;
    total->b += segments[i].b;
    total->a += segments[i].a;
  }
  total->r = total->r / (*segCountX * *segCountY);
  total->g = total->g / (*segCountX * *segCountY);
  total->b = total->b / (*segCountX * *segCountY);
  total->a = total->a / (*segCountX * *segCountY);
  if (debugMosaic >= 1) {
    printf("The RGB(A) of source is %d %d %d (%d)\n\n\n", total->r, total->g,
           total->b, total->a);
  }
  *final_segments = segments;
  *bytesPerPixel = bytePerPixel;
  *width = pixelPerScanline;
  *height = scanlineCount;
  if (inflatedBuffer != NULL) {
    free(inflatedBuffer);
  }
  free(buffer);
  return 0;
}

int main(int argc, char* argv[]) {
  if (argc < 6) {
    printf("Not enough arguments\n");
    return 1;
  }

  if (argc >= 6) {
    if (strcmp(argv[6], "debug1") == 0) {
      debugMosaic = 1;
    } else if (strcmp(argv[6], "debug2") == 0) {
      debugMosaic = 2;
    }
  }
  char* imagePath = pngPath(argv[1], argv[2]);
  FILE* mosaicOriginal = fopen(imagePath, "rb");
  free(imagePath);
  if (mosaicOriginal == NULL) {
    printf("Target file could not be opened\n");
    return 1;
  }

  struct AverageColor* targetSegments = NULL;
  struct AverageColor targetTotal;
  int targetsegCountX = 0;
  int targetsegCountY = 0;
  int bytesPerPixel = 0;
  int targetWidth = 0;
  int targetHeight = 0;

  if (processPNG(mosaicOriginal, &targetSegments, &targetTotal,
                 &targetsegCountX, &targetsegCountY, &targetWidth,
                 &targetHeight, &bytesPerPixel, true) != 0) {
    fclose(mosaicOriginal);
    return 1;
  }
  fclose(mosaicOriginal);

  DIR* sourceDirStream = opendir(argv[1]);
  if (sourceDirStream == NULL) {
    printf("Directory not found\n");
    return 1;
  }

  int sourcePixelX = 0;
  int sourcePixelY = 0;

  struct dirent* sourceDirent = readdir(sourceDirStream);
  for (; sourceDirent != NULL; sourceDirent = readdir(sourceDirStream)) {
    if (strcmp(&sourceDirent->d_name[sourceDirent->d_namlen - 4], ".png") ==
        0) {
      if (debugMosaic >= 1) {
        printf("Name of file is: %s[%d]\n", sourceDirent->d_name,
               sourceDirent->d_namlen);
      }
      imagePath = pngPath(argv[1], sourceDirent->d_name);
      FILE* sourcePNG = fopen(imagePath, "rb");
      free(imagePath);
      if (sourcePNG == NULL) {
        printf("A source file could not be opened\n");
        closedir(sourceDirStream);
        free(targetSegments);
        return 1;
      }
      struct AverageColor* sourceSegments = NULL;
      struct AverageColor sourceTotal;
      int sourceBytePerPixel = 0;
      int sourcesegCountX = 0;
      int sourcesegCountY = 0;
      int sourceWidth = 0;
      int sourceHeight = 0;

      if (processPNG(sourcePNG, &sourceSegments, &sourceTotal, &sourcesegCountX,
                     &sourcesegCountY, &sourceWidth, &sourceHeight,
                     &sourceBytePerPixel, false) != 0) {
        fclose(sourcePNG);
        return 1;
      }
      if (targetWidth == sourceWidth && targetHeight == sourceHeight) {
        insertNode(sourceTotal);
        sourcePixelX = sourcesegCountX;
        sourcePixelY = sourcesegCountY;
        char* pixelizedSourcePath = pixelizedPath(argv[3], sourceTotal);
        FILE* pixelizedSourceImage = fopen(pixelizedSourcePath, "wb");
        free(pixelizedSourcePath);
        if (pixelizedSourceImage == NULL) {
          printf("A pixelized source file could not be opened\n");
          fclose(sourcePNG);
          free(sourceSegments);
          free(targetSegments);
          closedir(sourceDirStream);
          return 1;
        }
        for (int i = 0; i < sourcesegCountX * sourcesegCountY; i++) {
          fwrite(&sourceSegments[i].r, 1, 1, pixelizedSourceImage);
          fwrite(&sourceSegments[i].g, 1, 1, pixelizedSourceImage);
          fwrite(&sourceSegments[i].b, 1, 1, pixelizedSourceImage);
          if (bytesPerPixel == 4) {
            fwrite(&targetSegments[i].a, 1, 1, pixelizedSourceImage);
          }
        }
        fclose(pixelizedSourceImage);
      }
      fclose(sourcePNG);
      free(sourceSegments);
    }
  }
  closedir(sourceDirStream);

  for (int targetSegCounter = 0;
       targetSegCounter < targetsegCountX * targetsegCountY;
       targetSegCounter++) {
    if (debugMosaic >= 2) {
      printf("%d %d %d\n", targetSegments[targetSegCounter].r,
             targetSegments[targetSegCounter].g,
             targetSegments[targetSegCounter].b);
    }
    int distance = 256;
    struct AverageColor closestSource;
    memset(&closestSource, 0, sizeof(struct AverageColor));
    for (struct node* current = head; current != NULL;
         current = current->next) {
      int curDistance =
          rgbaDistance(current->sourceTotal, targetSegments[targetSegCounter]);
      if (curDistance < distance) {
        distance = curDistance;
        closestSource.r = current->sourceTotal.r;
        closestSource.g = current->sourceTotal.g;
        closestSource.b = current->sourceTotal.b;
      }
    }
    targetSegments[targetSegCounter].r = closestSource.r;
    targetSegments[targetSegCounter].g = closestSource.g;
    targetSegments[targetSegCounter].b = closestSource.b;
    targetSegments[targetSegCounter].a = 0;
    if (debugMosaic >= 2) {
      printf("%d %d %d\n\n", targetSegments[targetSegCounter].r,
             targetSegments[targetSegCounter].g,
             targetSegments[targetSegCounter].b);
    }
  }

  char* rawImagePath = pngPath(argv[1], argv[4]);
  FILE* rawMosaic = fopen(rawImagePath, "wb+");
  free(rawImagePath);
  if (rawMosaic == NULL) {
    printf("Unable to open raw mosaic file\n");
    return 1;
  }

  unsigned char* buffer = (unsigned char*)malloc(sourcePixelX * bytesPerPixel);

  for (int sourceYNumber = 0; sourceYNumber < targetsegCountY;
       sourceYNumber++) {
    for (int sourceImageYLoc = 0; sourceImageYLoc < sourcePixelY;
         sourceImageYLoc++) {
      for (int sourceXNumber = 0; sourceXNumber < targetsegCountX;
           sourceXNumber++) {
        char* sourcePath = pixelizedPath(
            argv[3],
            targetSegments[sourceXNumber + sourceYNumber * targetsegCountX]);
        FILE* sourceImage = fopen(sourcePath, "rb");
        if (sourceImage == NULL) {
          printf("Unable to reopen source image %s\n", sourcePath);
          return 1;
        }
        free(sourcePath);
        int bytesPerScanLine = sourcePixelX * bytesPerPixel;
        fseek(sourceImage, bytesPerScanLine * sourceImageYLoc, SEEK_SET);
        fread(buffer, 1, bytesPerScanLine, sourceImage);
        fwrite(buffer, 1, bytesPerScanLine, rawMosaic);
        fclose(sourceImage);
      }
    }
  }

  free(buffer);
  free(targetSegments);
  fseek(rawMosaic, 0, SEEK_SET);

  int finalMosaicPathLen = strlen(argv[1]) + strlen(argv[2]) + 12;
  char* finalMosaicPath = malloc(finalMosaicPathLen);
  snprintf(finalMosaicPath, finalMosaicPathLen, "%s/finalMosaic.png", argv[5]);
  FILE* finalMosaic = fopen(finalMosaicPath, "wb");
  unsigned char finalPNGStartBytes[12];
  finalPNGStartBytes[0] = 0x89;
  finalPNGStartBytes[1] = 0x50;
  finalPNGStartBytes[2] = 0x4E;
  finalPNGStartBytes[3] = 0x47;
  finalPNGStartBytes[4] = 0x0D;
  finalPNGStartBytes[5] = 0x0A;
  finalPNGStartBytes[6] = 0x1A;
  finalPNGStartBytes[7] = 0x0A;
  finalPNGStartBytes[8] = 0;
  finalPNGStartBytes[9] = 0;
  finalPNGStartBytes[10] = 0;
  finalPNGStartBytes[11] = 0x0D;
  unsigned char pngIHDRtype[17];
  printBuffer(finalPNGStartBytes, 12);
  fwrite(&finalPNGStartBytes, 1, 12, finalMosaic);
  if (finalMosaic == NULL) {
    printf("Unable to open final png\n");
    return 1;
  }
  pngIHDRtype[0] = 'I';
  pngIHDRtype[1] = 'H';
  pngIHDRtype[2] = 'D';
  pngIHDRtype[3] = 'R';
  int finalXDim = sourcePixelX * targetsegCountX;
  int finalYDim = sourcePixelY * targetsegCountY;
  int scanlineLen = finalXDim * bytesPerPixel + 1;
  int finalFilterType = 0;
  int ret;
  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;
  strm.total_out = 0;
  strm.total_in = 0;
  ret = deflateInit(&strm, 6);
  if (ret != Z_OK) {
    printf("%d\n", ret);
    return 1;
  }
  struct IHDRChunk finalIHDR;
  int tempXDim = htonl((uint32_t)finalXDim);
  memcpy(finalIHDR.width, &tempXDim, 4);
  int tempYDim = htonl((uint32_t)finalYDim);
  memcpy(finalIHDR.height, &tempYDim, 4);
  finalIHDR.bitDepth = 8;
  finalIHDR.colorType = (bytesPerPixel == 3) ? 2 : 6;
  memset(finalIHDR.remainingVals, 0, 3);
  memcpy(&pngIHDRtype[4], &finalIHDR.width, 4);
  memcpy(&pngIHDRtype[8], &finalIHDR.height, 4);
  memcpy(&pngIHDRtype[12], &finalIHDR.bitDepth, 1);
  memcpy(&pngIHDRtype[13], &finalIHDR.colorType, 1);
  memcpy(&pngIHDRtype[14], &finalIHDR.remainingVals, 3);
  printBuffer(pngIHDRtype, 17);
  unsigned int crcIHDR = rc_crc32(0, pngIHDRtype, 17);
  crcIHDR = htonl(crcIHDR);
  printf("Calculated CRC 0x%x\n", crcIHDR);
  fwrite(&pngIHDRtype, 1, 17, finalMosaic);
  fwrite(&crcIHDR, 1, 4, finalMosaic);
  unsigned char* bufferIDAT = malloc(scanlineLen * finalYDim + 4);
  if (debugMosaic >= 1) {
    printf("Allocating %d bytes for bufferIDAT\n", scanlineLen * finalYDim + 8);
  }
  bufferIDAT[0] = 'I';
  bufferIDAT[1] = 'D';
  bufferIDAT[2] = 'A';
  bufferIDAT[3] = 'T';
  unsigned char* filteringBuffer = malloc(scanlineLen * 2);
  unsigned char* filteredScanlineBuffer = malloc(scanlineLen * finalYDim);
  strm.avail_out = scanlineLen * finalYDim;
  strm.next_out = &bufferIDAT[4];
  // FILE* filteredMosaic =
  //    fopen("/Users/aujus.garg/github/filteredFile.raw", "wb");
  for (int scanlineNum = 0; scanlineNum < finalYDim; scanlineNum++) {
    int curBufferLine = scanlineNum % 2;
    int prevBufferLine = (scanlineNum + 1) % 2;
    strm.next_in = &filteringBuffer[prevBufferLine * scanlineLen];
    strm.avail_in = scanlineLen;
    fseek(rawMosaic, (scanlineLen - 1) * scanlineNum, SEEK_SET);
    filteringBuffer[curBufferLine * scanlineLen] =
        (unsigned char)finalFilterType;
    fread(&filteringBuffer[curBufferLine * scanlineLen + 1], 1, scanlineLen - 1,
          rawMosaic);
    switch (finalFilterType) {
      case (1):
        filterSub(&filteringBuffer[curBufferLine * scanlineLen], finalXDim,
                  bytesPerPixel);
        // finalFilterType = 4;
        break;
      case (4):
        filterPaeth(&filteringBuffer[curBufferLine * scanlineLen],
                    &filteringBuffer[prevBufferLine * scanlineLen], finalXDim,
                    bytesPerPixel);
        break;
    }
    // fwrite(&filteringBuffer[curBufferLine * scanlineLen], 1, scanlineLen,
    //       filteredMosaic);
    // fflush(filteredMosaic);
    if (scanlineNum >= 1) {
      ret = deflate(&strm, Z_NO_FLUSH);
      if (ret != Z_OK) {
        printf("Failed to compress final buffer... ERRNO %d\n", ret);
        free(bufferIDAT);
        free(filteringBuffer);
        free(filteredScanlineBuffer);
        fclose(rawMosaic);
        fclose(finalMosaic);
        return 1;
      }
    }
  }
  strm.next_in = &filteringBuffer[((finalYDim + 1) % 2) * scanlineLen];
  strm.avail_in = scanlineLen;
  ret = deflate(&strm, Z_FINISH);
  unsigned int compressedDataLen = scanlineLen * finalYDim - strm.avail_out;
  if (ret != Z_STREAM_END) {
    printf("Failed to compress final buffer... ERRNO %d\n", ret);
    free(bufferIDAT);
    free(filteringBuffer);
    free(filteredScanlineBuffer);
    fclose(rawMosaic);
    fclose(finalMosaic);
    return 1;
  }
  deflateEnd(&strm);
  unsigned char chunkIDATLen[4];
  int tempDataLen = htonl((uint32_t)compressedDataLen);
  memcpy(&chunkIDATLen, &tempDataLen, 4);
  fwrite(&chunkIDATLen, 1, 4, finalMosaic);
  unsigned int crcIDAT = rc_crc32(0, bufferIDAT, 4 + compressedDataLen);
  crcIDAT = htonl(crcIDAT);
  if (debugMosaic >= 1) {
    printf("Writing %d bytes to file\n\n", 4 + compressedDataLen);
    printf("Writing CRC at %d offset\n", 4 + compressedDataLen);
  }
  fwrite(bufferIDAT, 1, compressedDataLen + 4, finalMosaic);
  fwrite(&crcIDAT, 1, 4, finalMosaic);
  free(bufferIDAT);
  free(filteringBuffer);
  free(filteredScanlineBuffer);
  fclose(rawMosaic);
  unsigned char lenIEND[4];
  memset(lenIEND, 0, 4);
  fwrite(&lenIEND, 1, 4, finalMosaic);
  unsigned char finalIEND[4];
  finalIEND[0] = 'I';
  finalIEND[1] = 'E';
  finalIEND[2] = 'N';
  finalIEND[3] = 'D';
  unsigned int crcIEND = rc_crc32(0, finalIEND, 4);
  crcIEND = htonl(crcIEND);
  fwrite(&finalIEND, 1, 4, finalMosaic);
  fwrite(&crcIEND, 1, 4, finalMosaic);
  fflush(finalMosaic);
  fclose(finalMosaic);
  finalMosaic = fopen(finalMosaicPath, "rb");
  struct AverageColor total;
  struct AverageColor* segments;
  int segCountX;
  int segCountY;
  int width;
  int height;
  processPNG(finalMosaic, &segments, &total, &segCountX, &segCountY, &width,
             &height, &bytesPerPixel, true);
  fclose(finalMosaic);
  free(finalMosaicPath);
  return 0;
}