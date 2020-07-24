#include <arpa/inet.h>
#include <dirent.h>
#include <math.h>
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

struct AverageColor {
  int r;
  int g;
  int b;
  int a;
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
  new_node->sourceTotal.a = sourceTotal.a;
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
           pixelizedTotal.a);
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

int roundUp(int dividend, int divisor) {
  return dividend / divisor + !!(dividend % divisor);
}

int calculateSegmentIndex(int bytePosition,
                          int bytesPerPixel,
                          int segmentDim,
                          int scanline,
                          int pixelsPerScanline) {
  int index = (bytePosition / bytesPerPixel) / segmentDim +
              (scanline / segmentDim) * (pixelsPerScanline / segmentDim);
  if (debugMosaic >= 3) {
    printf("[%d][%d] = [%d]\n", bytePosition, scanline, index);
  }
  return index;
}

void reconSub(unsigned char* scanline,
              int scanlinePixelCount,
              int pixelByteCount,
              struct AverageColor* segments,
              int lineNum) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  if (scanline[0] != 1) {
    return;
  }

  for (int i = 0; i < pixelByteCount; i++) {
    struct AverageColor* segment = &segments[calculateSegmentIndex(
        i, pixelByteCount, SEGMENT_DIMENSION, lineNum, scanlinePixelCount)];
    aggregateColors(segment, scanline[i + 1], i, pixelByteCount);
  }

  for (int i = 1 + pixelByteCount; i < scanlineLen; i++) {
    if (debugMosaic >= 2) {
      printf("i %d pixelByteCount %d buffer[x] %d buffer[a] %d\n", i,
             pixelByteCount, scanline[i], scanline[i - pixelByteCount]);
    }
    scanline[i] += scanline[i - pixelByteCount];
    struct AverageColor* segment = &segments[calculateSegmentIndex(
        i - 1, pixelByteCount, SEGMENT_DIMENSION, lineNum, scanlinePixelCount)];
    aggregateColors(segment, scanline[i], i - 1, pixelByteCount);
  }
}

void reconUp(unsigned char* curScanline,
             unsigned char* prevScanline,
             int scanlinePixelCount,
             int pixelByteCount,
             struct AverageColor* segments,
             int lineNum) {
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
        i - 1, pixelByteCount, SEGMENT_DIMENSION, lineNum, scanlinePixelCount)];
    aggregateColors(segment, curScanline[i], i - 1, pixelByteCount);
  }
}

void reconAvg(unsigned char* curScanline,
              unsigned char* prevScanline,
              int scanlinePixelCount,
              int pixelByteCount,
              struct AverageColor* segments,
              int lineNum) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  if (curScanline[0] != 3) {
    return;
  }

  for (int i = 0; i < pixelByteCount; i++) {
    curScanline[1 + i] += prevScanline[1 + i] / 2;
    struct AverageColor* segment = &segments[calculateSegmentIndex(
        i, pixelByteCount, SEGMENT_DIMENSION, lineNum, scanlinePixelCount)];
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
        i - 1, pixelByteCount, SEGMENT_DIMENSION, lineNum, scanlinePixelCount)];
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
                int lineNum) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  if (curScanline[0] != 4) {
    return;
  }
  for (int i = 0; i < pixelByteCount; i++) {
    unsigned char b = prevScanline[1 + i];
    unsigned char x = curScanline[1 + i] + paethPredict(0, b, 0);
    curScanline[1 + i] = x;
    struct AverageColor* segment = &segments[calculateSegmentIndex(
        i, pixelByteCount, SEGMENT_DIMENSION, lineNum, scanlinePixelCount)];
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
        i - 1, pixelByteCount, SEGMENT_DIMENSION, lineNum, scanlinePixelCount)];
    aggregateColors(segment, curScanline[i], i - 1, pixelByteCount);
  }
}

int reconImage(unsigned char* newScanline,
               unsigned char* prevScanline,
               int pixelPerScanline,
               int bytePerPixel,
               struct AverageColor* segments,
               int lineNum,
               int arrayXVal) {
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
    case 0:
      break;
    case 1:
      reconSub(newScanline, pixelPerScanline, bytePerPixel, segments, lineNum);
      break;
    case 2:
      reconUp(newScanline, prevScanline, pixelPerScanline, bytePerPixel,
              segments, lineNum);
      break;
    case 3:
      reconAvg(newScanline, prevScanline, pixelPerScanline, bytePerPixel,
               segments, lineNum);
      break;
    case 4:
      reconPaeth(newScanline, prevScanline, pixelPerScanline, bytePerPixel,
                 segments, lineNum);
      break;
    default:
      printf("Unknown filter type: %d\n", filter_type);
      return 1;
  };

  return 0;
}

int processHeaderChunk(int* pixelPerScanline,
                       unsigned char* buffer,
                       int i,
                       int* scanlineCount,
                       int* arrayXVal,
                       int* arrayYVal,
                       int* bytePerPixel,
                       FILE* mosaicOriginal,
                       /*FILE* inflatedMosaic,*/
                       int* bytesPerScanline,
                       unsigned char* inflatedBuffer,
                       struct AverageColor* segments) {
  memcpy(pixelPerScanline, &buffer[i + 8], sizeof(int));
  *pixelPerScanline = ntohl(*pixelPerScanline);
  memcpy(scanlineCount, &buffer[i + 12], sizeof(int));
  *scanlineCount = ntohl(*scanlineCount);

  *arrayXVal = roundUp(*pixelPerScanline, SEGMENT_DIMENSION);
  *arrayYVal = roundUp(*scanlineCount, SEGMENT_DIMENSION);
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
        *arrayXVal, *arrayYVal);
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
                     int arrayXVal,
                     FILE* mosaicOriginal) {
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
                     /*inflatedMosaic,*/ segments, lineNumber,
                     arrayXVal) == 1) {
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
               int* arrayXVal,
               int* arrayYVal) {
  fseek(mosaicOriginal, 0L, SEEK_END);
  int fileLen = ftell(mosaicOriginal);
  rewind(mosaicOriginal);

  unsigned char* buffer = (unsigned char*)malloc(fileLen);
  unsigned char* inflatedBuffer = NULL;
  if (debugMosaic >= 1) {
    printf("Reading into %p, %d bytes, from %p\n", buffer, fileLen,
           mosaicOriginal);
  }
  size_t read = fread(buffer, 1, fileLen, mosaicOriginal);

  int chunkCount = 0;
  int chunkDataLength;
  int bytePerPixel = 0;
  int bytesPerScanline = 0;
  int pixelPerScanline = 0;
  int scanlineCount = 0;
  int lineNum = 0;
  int ret;
  int scanlineFilled = 0;
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
                             arrayXVal, arrayYVal, &bytePerPixel,
                             mosaicOriginal, &bytesPerScanline, inflatedBuffer,
                             segments) != 0) {
        free(buffer);
        return 1;
      }

      inflatedBuffer = (unsigned char*)malloc(bytesPerScanline * 2);
      int size = sizeof(struct AverageColor) * *arrayXVal * *arrayYVal;
      segments = (struct AverageColor*)malloc(size);
      if (debugMosaic >= 2) {
        printf("x %d, y %d, size %d, ptr %p\n", *arrayXVal, *arrayYVal, size,
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
                           segments, *arrayXVal, mosaicOriginal) != 0) {
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
  int pixelsPerSegment = SEGMENT_DIMENSION * SEGMENT_DIMENSION;
  for (int i = 0; i < *arrayXVal * *arrayYVal; i++) {
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
  for (int i = 0; i < *arrayXVal * *arrayYVal; i++) {
    total->r += segments[i].r;
    total->g += segments[i].g;
    total->b += segments[i].b;
    total->a += segments[i].a;
  }
  total->r = total->r / (*arrayXVal * *arrayYVal);
  total->g = total->g / (*arrayXVal * *arrayYVal);
  total->b = total->b / (*arrayXVal * *arrayYVal);
  total->a = total->a / (*arrayXVal * *arrayYVal);
  if (debugMosaic >= 1) {
    printf("The RGB(A) of source is %d %d %d (%d)\n\n", total->r, total->g,
           total->b, total->a);
  }
  *final_segments = segments;

  if (inflatedBuffer != NULL) {
    free(inflatedBuffer);
  }
  free(buffer);
  return 0;
}

int main(int argc, char* argv[]) {
  if (argc < 5) {
    printf("Not enough arguments\n");
    return 1;
  }

  if (argc >= 6) {
    if (strcmp(argv[5], "debug1") == 0) {
      debugMosaic = 1;
    } else if (strcmp(argv[5], "debug2") == 0) {
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
  int targetArrayXVal = 0;
  int targetArrayYVal = 0;

  if (processPNG(mosaicOriginal, &targetSegments, &targetTotal,
                 &targetArrayXVal, &targetArrayYVal) != 0) {
    fclose(mosaicOriginal);
    return 1;
  }
  fclose(mosaicOriginal);

  DIR* sourceDirStream = opendir(argv[1]);
  if (sourceDirStream == NULL) {
    printf("Directory not found\n");
    return 1;
  }
  int sourceArrayXVal = 0;
  int sourceArrayYVal = 0;
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

      if (processPNG(sourcePNG, &sourceSegments, &sourceTotal, &sourceArrayXVal,
                     &sourceArrayYVal) != 0) {
        fclose(sourcePNG);
        return 1;
      }
      if (targetArrayXVal == sourceArrayXVal &&
          targetArrayYVal == sourceArrayYVal) {
        insertNode(sourceTotal);
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
        for (int i = 0; i < sourceArrayXVal * sourceArrayYVal; i++) {
          fwrite(&sourceSegments[i].r, 1, 1, pixelizedSourceImage);
          fwrite(&sourceSegments[i].g, 1, 1, pixelizedSourceImage);
          fwrite(&sourceSegments[i].b, 1, 1, pixelizedSourceImage);
        }
        fclose(pixelizedSourceImage);
      }
      fclose(sourcePNG);
      free(sourceSegments);
    }
  }
  closedir(sourceDirStream);

  for (int targetSegCounter = 0;
       targetSegCounter < targetArrayXVal * targetArrayYVal;
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
    if (debugMosaic >= 2) {
      printf("%d %d %d\n\n", targetSegments[targetSegCounter].r,
             targetSegments[targetSegCounter].g,
             targetSegments[targetSegCounter].b);
    }
  }

  char* rawImagePath = pngPath(argv[1], argv[4]);
  FILE* rawMosaic = fopen(rawImagePath, "wb");
  free(rawImagePath);
  if (rawMosaic == NULL) {
    printf("Unable to open raw mosaic file\n");
    return 1;
  }

  unsigned char* buffer = (unsigned char*)malloc(sourceArrayXVal * 3);

  for (int sourceYNumber = 0; sourceYNumber < targetArrayYVal;
       sourceYNumber++) {
    for (int sourceImageYLoc = 0; sourceImageYLoc < sourceArrayYVal;
         sourceImageYLoc++) {
      for (int sourceXNumber = 0; sourceXNumber < targetArrayXVal;
           sourceXNumber++) {
        char* sourcePath = pixelizedPath(
            argv[3],
            targetSegments[sourceXNumber + sourceYNumber * targetArrayXVal]);
        FILE* sourceImage = fopen(sourcePath, "rb");
        if (sourceImage == NULL) {
          printf("Unable to reopen source image %s\n", sourcePath);
          return 1;
        }
        free(sourcePath);
        int bytesPerScanLine = sourceArrayXVal * 3;
        fseek(sourceImage, bytesPerScanLine * sourceImageYLoc, SEEK_SET);
        fread(buffer, 1, bytesPerScanLine, sourceImage);
        fwrite(buffer, 1, bytesPerScanLine, rawMosaic);
        fclose(sourceImage);
      }
    }
  }

  fclose(rawMosaic);
  free(targetSegments);
  return 0;
}