#include <arpa/inet.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define SEGMENT_DIMENSION (16)

int debugMosaic = 0;

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

void colAdd(struct avgCol* color,
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

char* pngPath(char* directoryPath, char* pngName) {
  int pathSize = strlen(directoryPath) + strlen(pngName) + 2;
  char* imagePath = malloc(pathSize);
  snprintf(imagePath, pathSize, "%s/%s", directoryPath, pngName);
  return imagePath;
}

int rgbaDistance(struct avgCol sourceRGBA, struct avgCol targetRGBA) {
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

void recon_sub(unsigned char* scanline,
               int scanlinePixelCount,
               int pixelByteCount,
               struct avgCol* segments,
               int lineNum) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  if (scanline[0] != 1) {
    return;
  }

  for (int i = 0; i < pixelByteCount; i++) {
    struct avgCol* segment = &segments[calculateSegmentIndex(
        i, pixelByteCount, SEGMENT_DIMENSION, lineNum, scanlinePixelCount)];
    colAdd(segment, scanline[i + 1], i, pixelByteCount);
  }

  for (int i = 1 + pixelByteCount; i < scanlineLen; i++) {
    if (debugMosaic >= 2) {
      printf("i %d pixelByteCount %d buffer[x] %d buffer[a] %d\n", i,
             pixelByteCount, scanline[i], scanline[i - pixelByteCount]);
    }
    scanline[i] += scanline[i - pixelByteCount];
    struct avgCol* segment = &segments[calculateSegmentIndex(
        i - 1, pixelByteCount, SEGMENT_DIMENSION, lineNum, scanlinePixelCount)];
    colAdd(segment, scanline[i], i - 1, pixelByteCount);
  }
}

void recon_up(unsigned char* curScanline,
              unsigned char* prevScanline,
              int scanlinePixelCount,
              int pixelByteCount,
              struct avgCol* segments,
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
    struct avgCol* segment = &segments[calculateSegmentIndex(
        i - 1, pixelByteCount, SEGMENT_DIMENSION, lineNum, scanlinePixelCount)];
    colAdd(segment, curScanline[i], i - 1, pixelByteCount);
  }
}

void recon_avg(unsigned char* curScanline,
               unsigned char* prevScanline,
               int scanlinePixelCount,
               int pixelByteCount,
               struct avgCol* segments,
               int lineNum) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  if (curScanline[0] != 3) {
    return;
  }

  for (int i = 0; i < pixelByteCount; i++) {
    curScanline[1 + i] += prevScanline[1 + i] / 2;
    struct avgCol* segment = &segments[calculateSegmentIndex(
        i, pixelByteCount, SEGMENT_DIMENSION, lineNum, scanlinePixelCount)];
    colAdd(segment, curScanline[i + 1], i, pixelByteCount);
  }
  for (int i = 1 + pixelByteCount; i < scanlineLen; i++) {
    if (debugMosaic >= 2) {
      printf("i %d pixelByteCount %d buffer[x] %d buffer[a] %d\n", i,
             pixelByteCount, curScanline[i - pixelByteCount], prevScanline[i]);
    }
    int x = ((int)curScanline[i - pixelByteCount] + (int)prevScanline[i]) / 2;
    curScanline[i] += x;
    struct avgCol* segment = &segments[calculateSegmentIndex(
        i - 1, pixelByteCount, SEGMENT_DIMENSION, lineNum, scanlinePixelCount)];
    colAdd(segment, curScanline[i], i - 1, pixelByteCount);
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
                 struct avgCol* segments,
                 int lineNum) {
  int scanlineLen = scanlinePixelCount * pixelByteCount + 1;
  if (curScanline[0] != 4) {
    return;
  }
  for (int i = 0; i < pixelByteCount; i++) {
    unsigned char b = prevScanline[1 + i];
    unsigned char x = curScanline[1 + i] + paeth_predict(0, b, 0);
    curScanline[1 + i] = x;
    struct avgCol* segment = &segments[calculateSegmentIndex(
        i, pixelByteCount, SEGMENT_DIMENSION, lineNum, scanlinePixelCount)];
    colAdd(segment, curScanline[i + 1], i, pixelByteCount);
  }
  for (int i = 1 + pixelByteCount; i < scanlineLen; i++) {
    if (debugMosaic >= 2) {
      printf("i %d pixelByteCount %d buffer[x] %d buffer[a] %d\n", i,
             pixelByteCount, curScanline[i], curScanline[i - pixelByteCount]);
    }
    unsigned char a = curScanline[i - pixelByteCount];
    unsigned char b = prevScanline[i];
    unsigned char c = prevScanline[i - pixelByteCount];
    unsigned char x = curScanline[i] + paeth_predict(a, b, c);
    curScanline[i] = x;
    struct avgCol* segment = &segments[calculateSegmentIndex(
        i - 1, pixelByteCount, SEGMENT_DIMENSION, lineNum, scanlinePixelCount)];
    colAdd(segment, curScanline[i], i - 1, pixelByteCount);
  }
}

int recon_image(unsigned char* newScanline,
                unsigned char* prevScanline,
                int pixelPerScanline,
                int bytePerPixel,
                /*FILE* inflatedMosaic,*/
                struct avgCol* segments,
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
      recon_sub(newScanline, pixelPerScanline, bytePerPixel, segments, lineNum);
      break;
    case 2:
      recon_up(newScanline, prevScanline, pixelPerScanline, bytePerPixel,
               segments, lineNum);
      break;
    case 3:
      recon_avg(newScanline, prevScanline, pixelPerScanline, bytePerPixel,
                segments, lineNum);
      break;
    case 4:
      recon_paeth(newScanline, prevScanline, pixelPerScanline, bytePerPixel,
                  segments, lineNum);
      break;
    default:
      printf("Unknown filter type: %d\n", filter_type);
      return 1;
  };

  /* size_t written =
       fwrite(&newScanline[0 + 1], 1, scanlineLen - 1, inflatedMosaic);
   if (debugMosaic >= 2) {
     printf("Filter type %d, wrote %zu bytes\n", filter_type, written);
   } */
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
                       struct avgCol* segments) {
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
                     /*FILE* inflatedMosaic,*/
                     struct avgCol* segments,
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
      if (recon_image(curScanline, prevScanline, pixelPerScanline, bytePerPixel,
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
               struct avgCol** final_segments,
               struct avgCol* total) {
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
  int pixelPerScanline = 0;
  int scanlineCount = 0;
  int bytesPerScanline = 0;
  int lineNum = 0;
  int arrayXVal = 0;
  int arrayYVal = 0;
  int ret;
  int scanlineFilled = 0;
  struct avgCol* segments = NULL;

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
                             &arrayXVal, &arrayYVal, &bytePerPixel,
                             mosaicOriginal,
                             /*inflatedMosaic,*/ &bytesPerScanline,
                             inflatedBuffer, segments) != 0) {
        free(buffer);
        return 1;
      }

      inflatedBuffer = (unsigned char*)malloc(bytesPerScanline * 2);
      int size = sizeof(struct avgCol) * arrayXVal * arrayYVal;
      segments = (struct avgCol*)malloc(size);
      if (debugMosaic >= 2) {
        printf("x %d, y %d, size %d, ptr %p\n", arrayXVal, arrayYVal, size,
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
                           /*inflatedMosaic,*/ segments, arrayXVal,
                           mosaicOriginal) != 0) {
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
  for (int i = 0; i < arrayXVal * arrayYVal; i++) {
    segments[i].r = segments[i].r / pixelsPerSegment;
    segments[i].g = segments[i].g / pixelsPerSegment;
    segments[i].b = segments[i].b / pixelsPerSegment;
    segments[i].a = segments[i].a / pixelsPerSegment;
    if (debugMosaic >= 2) {
      printf("[%d] %d %d %d (%d)\n", i, segments[i].r, segments[i].g,
             segments[i].b, segments[i].a);
    }
    /* fwrite(&segments[i].r, 1, 1, pixelizedImage);
       fwrite(&segments[i].g, 1, 1, pixelizedImage);
       fwrite(&segments[i].b, 1, 1, pixelizedImage);
       if (bytePerPixel == 4) {
         fwrite(&segments[i].a, 1, 1, pixelizedImage);
       } */
  }

  memset(total, 0, sizeof(struct avgCol));
  for (int i = 0; i < arrayXVal * arrayYVal; i++) {
    total->r += segments[i].r;
    total->g += segments[i].g;
    total->b += segments[i].b;
    total->a += segments[i].a;
  }
  total->r = total->r / (arrayXVal * arrayYVal);
  total->g = total->g / (arrayXVal * arrayYVal);
  total->b = total->b / (arrayXVal * arrayYVal);
  total->a = total->a / (arrayXVal * arrayYVal);
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
  if (argc < 4) {
    printf("Not enough arguments\n");
    return 1;
  }

  if (argc >= 5) {
    if (strcmp(argv[4], "debug1") == 0) {
      debugMosaic = 1;
    } else if (strcmp(argv[4], "debug2") == 0) {
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

  /* FILE* inflatedMosaic =
     fopen("/Users/aujus.garg/github/testing_files/test.raw", "wb");
 if (inflatedMosaic == NULL) {
   printf(
       "File /Users/aujus.garg/github/testing_files/test.raw could not be "
       "opened\n");
   return 1;
 }


  FILE* pixelizedImage =
      fopen("/Users/aujus.garg/github/testing_files/pixelimage.raw", "wb");
  if (pixelizedImage == NULL) {
    printf(
        "File /Users/aujus.garg/github/testing_files/pixelimage.raw could not "
        "be opened");
    return 1;
  } */

  struct avgCol* targetSegments = NULL;
  struct avgCol targetTotal;

  if (processPNG(mosaicOriginal, &targetSegments, &targetTotal) != 0) {
    fclose(mosaicOriginal);
    return 1;
  }

  DIR* sourceDirStream = opendir(argv[1]);
  if (sourceDirStream == NULL) {
    printf("Directory not found\n");
    fclose(mosaicOriginal);
    return 1;
  }
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
        return 1;
      }
      struct avgCol* sourceSegments = NULL;
      struct avgCol sourceTotal;
      if (processPNG(sourcePNG, &sourceSegments, &sourceTotal) != 0) {
        fclose(sourcePNG);
        return 1;
      }
      fclose(sourcePNG);
      free(sourceSegments);
    }
  }

  closedir(sourceDirStream);
  fclose(mosaicOriginal);
  free(targetSegments);

  return 0;
}