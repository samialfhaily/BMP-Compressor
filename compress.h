#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <getopt.h>

#include "bmp.h"

bool parseChosenAlgorithm(char *arg, char **restrict algorithm);
bool parseBenchmarkingRepetitions(const char *arg, unsigned long *restrict parsed);

void printLongUsage(char **restrict argv);
void printShortUsage(char **restrict argv);
void metadata(const BITMAPFILEHEADER *restrict header, const BITMAPINFOHEADER *restrict info);

unsigned int compress(const char *source, RLEPIXEL *destination, const int width, const int height);
extern unsigned int bmp_rle(const char *source, RLEPIXEL *destination, const int width, const int height);
extern unsigned int bmp_rle_simd(const char *source, RLEPIXEL *destination, const int width, const int height);

double benchmark(const unsigned long n, const char *algorithm, const char *restrict pixels, RLEPIXEL *compressed, const int width, const int height);

int readOffset(const char *path, char *offset, const ssize_t size);
int checkValidity(const BITMAPFILEHEADER *restrict header, const BITMAPINFOHEADER *restrict info);
int readMetadata(const char *path, BITMAPFILEHEADER *header, BITMAPINFOHEADER *info);
int readPixels(const char *path, char *pixels, const unsigned int offset, const ssize_t imageSize);
int writeImage(FILE *restrict outptr, const BITMAPFILEHEADER *header, const BITMAPINFOHEADER *info, const char *offset, const size_t offsetCount, const RLEPIXEL *compressed, const size_t pixelsCount);

void metadata(const BITMAPFILEHEADER *restrict header, const BITMAPINFOHEADER *restrict info)
{
  printf("Header:\n");
  printf("File type: %u.\n", header->fileType);
  printf("File size: %u\n", header->fileSize);
  printf("Reserved 1: %hu.\n", header->reserved1);
  printf("Reserved 2: %hu.\n", header->reserved2);
  printf("Pixel data offset: %u\n\n", header->pixelDataOffset);

  printf("Info:\n");
  printf("Header size: %u.\n", info->headerSize);
  printf("Image width: %i\n", info->imageWidth);
  printf("Image height: %i\n", info->imageHeight);
  printf("Planes: %hu.\n", info->planes);
  printf("bpp: %hu.\n", info->bitsPerPixel);
  printf("Compression: %u.\n", info->compression);
  printf("Image size: %u\n", info->imageSize);
  printf("X ppm: %i\n", info->xPixelsPerMeter);
  printf("Y ppm: %i\n", info->yPixelsPerMeter);
  printf("Total colors: %u.\n", info->totalColors);
  printf("Important colors: %u.\n", info->importantColors);
}

void printShortUsage(char **restrict argv)
{
  fprintf(stderr, "\nSIMPLE USAGE: %s -o <outfile> <infile>\n\n", argv[0]);
  fprintf(stderr, "%s --help for more information\n\n", argv[0]);
}

void printLongUsage(char **restrict argv)
{
  fprintf(stdout, "\nOVERVIEW: A CLI tool to compress 8bpp BMP images using the RLE algorithm\n\n");

  fprintf(stderr, "USAGE: %s [-b <repetitions>] [-a <algorithm>] -o <outfile> <infile>\n\n", argv[0]);

  fprintf(stdout, "OPTIONS:\n");
  fprintf(stdout, "\t-h, --help\t\t\t\t\t\t\tShow help information.\n\n");
  fprintf(stdout, "\t-o, --output\t\t\t\t\t\t\tSpecify the output path of the compressed image.\n\n");
  fprintf(stdout, "\t-a <algorithm>, --algorithm <algorithm>\t\t\t\tManually specify algorithm to use for compression.\n");
  fprintf(stdout, "\t\t\t\t\t\t\t\t\t\tavailable arguments: c, asm, simd (default: c)\n\n");
  fprintf(stdout, "\t-b <repetitions>, --benchmark <repetitions>\t\t\tCompresses the input image \"repetitions\" times and displays the average compression time,\n");
  fprintf(stdout, "\t\t\t\t\t\t\t\t\t\tin addition to writing the compressed image to the output path (if specified)\n\n");

  fprintf(stdout, "EXAMPLE USAGE:\n");
  fprintf(stdout, "\t%s -o ./output.bmp ./input.bmp\n\n", argv[0]);
  fprintf(stdout, "\t%s -a c --output ./output.bmp ./input.bmp\n", argv[0]);
  fprintf(stdout, "\t%s --algorithm simd -o ./output.bmp ./input.bmp\n\n", argv[0]);
  fprintf(stdout, "\t%s -b 42 ./input.bmp\n", argv[0]);
  fprintf(stdout, "\t%s -b 42 -o ./output.bmp ./input.bmp\n", argv[0]);
  fprintf(stdout, "\t%s -b 42 -o ./output.bmp -a asm ./input.bmp\n", argv[0]);
  fprintf(stdout, "\t%s --benchmark 42 -o ./output.bmp ./input.bmp\n\n", argv[0]);
}

// Returns true if argument has been parsed correctly, otherwise false
bool parseBenchmarkingRepetitions(const char *arg, unsigned long *restrict parsed)
{
  // If number provided is negative
  if (*arg == '-')
  {
    fprintf(stderr, "Number of repetitions should be at least 1.\n\n");
    return false;
  }

  char *endptr;
  errno = 0;
  *parsed = strtoul(arg, &endptr, 10);

  if (errno == EINVAL)
  {
    fprintf(stderr, "An error has occured while converting %s\n", arg);
    return false;
  }
  else if (errno == ERANGE)
  {
    fprintf(stderr, "%s is out of range.\n\n", arg);
    return false;
  }
  else if (*endptr && *endptr != '\n')
  {
    fprintf(stderr, "%s is an invalid argument.\n\n", arg);
    return false;
  }
  else if (*parsed < 1)
  {
    fprintf(stderr, "Number of repetitions should be at least 1.\n\n");
    return false;
  }

  return true;
}

// Returns true if argument has been parsed correctly, otherwise false
bool parseChosenAlgorithm(char *arg, char **restrict algorithm)
{
  if (!strcmp(arg, "c") || !strcmp(arg, "asm") || !strcmp(arg, "simd"))
  {
    *algorithm = arg;
    return true;
  }
  else
  {
    return false;
  }
}
