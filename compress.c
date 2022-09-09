#include "compress.h"

int main(int argc, char **argv)
{
  // Program options/arguments
  char *algorithm = "c";
  bool benchmarking = false;
  unsigned long n = 0;

  // Remember file paths
  char *infile = NULL;
  char *outfile = NULL;

  int option_index = 0;
  static struct option long_options[] = {
      {"help", no_argument, NULL, 'h'},
      {"algorithm", required_argument, NULL, 'a'},
      {"output", required_argument, NULL, 'o'},
      {"benchmark", required_argument, NULL, 'b'},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "ha:o:b:", long_options, &option_index)) != -1)
  {
    switch (opt)
    {
    case 'h':
      printLongUsage(argv);
      return 0;
    case 'a':
      if (!parseChosenAlgorithm(optarg, &algorithm))
      {
        fprintf(stderr, "%s is an invalid argument.\n", optarg);
        fprintf(stdout, "available arguments: c, asm, simd\n\n");
        printShortUsage(argv);
        return 1;
      }
      break;
    case 'o':
      outfile = optarg;
      break;
    case 'b':
      if (!parseBenchmarkingRepetitions(optarg, &n))
      {
        printShortUsage(argv);
        return 1;
      }
      benchmarking = true;
      break;
    default:
      printShortUsage(argv);
      return 1;
    }
  }

  // If not benchmarking and outfile has not been specified
  if (!benchmarking && !outfile)
  {
    printShortUsage(argv);
    return 1;
  }

  // Make sure infile path has been specified at the end
  if (optind != argc - 1)
  {
    printShortUsage(argv);
    return 1;
  }

  infile = argv[optind];

  // Open input image
  FILE *inptr = fopen(infile, "r");
  if (!inptr)
  {
    perror("Could not open input image");
    return 2;
  }

  BITMAPFILEHEADER header;
  BITMAPINFOHEADER info;

  // Read input image header
  if (readMetadata(infile, &header, &info))
  {
    fclose(inptr);
    return 4;
  }

  if (checkValidity(&header, &info))
  {
    fclose(inptr);
    return 5;
  }

  // Calculate size of remaining offset
  size_t remainingOffset = header.pixelDataOffset - sizeof(BITMAPFILEHEADER) - sizeof(BITMAPINFOHEADER);

  // Allocate memory for the offset between metadata and actual pixels data
  char *offset = malloc(remainingOffset);
  if (!offset)
  {
    fclose(inptr);
    fprintf(stderr, "Couldn't allocate enough memory for input image offset.\n");
    return 6;
  }

  // Read remaining offset
  if (readOffset(infile, offset, remainingOffset))
  {
    fclose(inptr);
    free(offset);
    return 7;
  }

  // Allocate memory for pixels
  char *pixels = malloc(info.imageSize);
  if (!pixels)
  {
    fclose(inptr);
    free(offset);
    fprintf(stderr, "Couldn't allocate enough memory for input image pixels.\n");
    return 8;
  }

  // Read image pixels
  if (readPixels(infile, pixels, header.pixelDataOffset, info.imageSize))
  {
    fclose(inptr);
    free(offset);
    free(pixels);
    return 9;
  }

  // Allocate memory for compressed pixels
  // Choose a large number for worst case scenario, this number should technically never be needed, but it's good to be sure
  RLEPIXEL *compressed = malloc(info.imageSize * 2 + ENDOFLINE * (info.imageHeight - 1) + ENDOFBITMAP);
  if (!compressed)
  {
    fclose(inptr);
    free(offset);
    free(pixels);
    fprintf(stderr, "Couldn't allocate enough memory to compress image.\n");
    return 10;
  }

  if (benchmarking)
  {
    double time = benchmark(n, algorithm, pixels, compressed, info.imageWidth, info.imageHeight);
    fprintf(stdout, "Average compression time of %lu repetitions using %s algorithm is %.2f ms.\n", n, algorithm, time * 1000);
  }

  if (!outfile)
  {
    return 0;
  }

  // Create output image
  FILE *outptr = fopen(outfile, "w");
  if (!outptr)
  {
    perror("Could not create output image");
    fclose(inptr);
    free(compressed);
    free(offset);
    free(pixels);
    return 3;
  }

  // Compress the pixels
  unsigned int compressedCount;

  if (!strcmp(algorithm, "simd"))
  {
    compressedCount = bmp_rle_simd(pixels, compressed, info.imageWidth, info.imageHeight);
  }
  else if (!strcmp(algorithm, "asm"))
  {
    compressedCount = bmp_rle(pixels, compressed, info.imageWidth, info.imageHeight);
  }
  else
  {
    compressedCount = compress(pixels, compressed, info.imageWidth, info.imageHeight);
  }

  // save the old file size to calculate the compression ratio later
  unsigned int oldFileSize = header.fileSize;

  // Adjust compressed image metadata
  info.compression = 1;
  info.imageSize = compressedCount * 2;
  header.fileSize = info.imageSize + header.pixelDataOffset;

  // write the image to an output file
  if (writeImage(outptr, &header, &info, offset, remainingOffset, compressed, compressedCount))
  {
    fclose(inptr);
    fclose(outptr);
    free(offset);
    free(pixels);
    free(compressed);
    return 11;
  }

  fprintf(stdout, "Image has been successfully compressed.\n");
  fprintf(stdout, "Compression ratio: %.2f\n", (double)oldFileSize / (double)header.fileSize);
  fprintf(stdout, "%.2f%% of the image has been encoded.\n", (1 - (double)header.fileSize / (double)oldFileSize) * 100);

  fclose(inptr);
  fclose(outptr);
  free(offset);
  free(pixels);
  free(compressed);
  return 0;
}

// Returns a positive value if an error has occurred
int writeImage(FILE *restrict outptr, const BITMAPFILEHEADER *header, const BITMAPINFOHEADER *info, const char *offset, const size_t offsetCount, const RLEPIXEL *compressed, const size_t pixelsCount)
{
  if (fwrite(header, sizeof(BITMAPFILEHEADER), 1, outptr) != 1)
  {
    fprintf(stderr, "An error has occurred while writing the file header of the output image.\n");
    return 1;
  }

  if (fwrite(info, sizeof(BITMAPINFOHEADER), 1, outptr) != 1)
  {
    fprintf(stderr, "An error has occurred while writing the info header of the output image.\n");
    return 2;
  }

  if (fwrite(offset, sizeof(char), offsetCount, outptr) != offsetCount)
  {
    fprintf(stderr, "An error has occurred while writing the offset of the output image.\n");
    return 3;
  }

  if (fwrite(compressed, sizeof(RLEPIXEL), pixelsCount, outptr) != pixelsCount)
  {
    fprintf(stderr, "An error has occurred while writing the pixels of the output image.\n");
    return 5;
  }

  return 0;
}

// Returns a positive value if an error has occurred
int readMetadata(const char *path, BITMAPFILEHEADER *header, BITMAPINFOHEADER *info)
{
  int fd = open(path, O_RDONLY);

  if (pread(fd, header, FILEHEADERSIZE, 0) != FILEHEADERSIZE)
  {
    fprintf(stderr, "Couldn't read BITMAPFILEHEADER.\n");
    return 1;
  }
  if (pread(fd, info, INFOHEADERSIZE, FILEHEADERSIZE) != INFOHEADERSIZE)
  {
    fprintf(stderr, "Couldn't read BITMAPINFOHEADER.\n");
    return 2;
  }

  // Get fileSize in case it's not been set correctly in the BMP file
  struct stat buf;

  if (fstat(fd, &buf))
  {
    fprintf(stderr, "An error has occurred while reading file size.\n");
  }

  unsigned int fileSize = (unsigned int)buf.st_size;

  // Change data in the header to make sure it's set correctly
  header->fileSize = fileSize;
  info->imageSize = header->fileSize - header->pixelDataOffset;

  if (close(fd) == -1)
  {
    fprintf(stderr, "Couldn't close the file descriptor.\n");
    return 3;
  }

  return 0;
}

// Returns a positive value if an error has occurred
int readOffset(const char *path, char *offset, const ssize_t size)
{
  int fd = open(path, O_RDONLY);

  if (pread(fd, offset, size, FILEHEADERSIZE + INFOHEADERSIZE) != size)
  {
    fprintf(stderr, "Couldn't read the entire offset.\n");
    return 1;
  }

  if (close(fd) == -1)
  {
    fprintf(stderr, "Couldn't close the file descriptor.\n");
    return 2;
  }

  return 0;
}

// Returns a positive value if an error has occurred
int readPixels(const char *path, char *pixels, const unsigned int offset, const ssize_t imageSize)
{
  int fd = open(path, O_RDONLY);

  if (pread(fd, pixels, imageSize, offset) != imageSize)
  {
    fprintf(stderr, "Couldn't read all pixels\n.");
    return 1;
  }

  if (close(fd) == -1)
  {
    fprintf(stderr, "Couldn't close the file descriptor.\n");
    return 2;
  }

  return 0;
}

// Returns size of compressed pixels
unsigned int compress(const char *source, RLEPIXEL *destination, const int width, const int height)
{
  int remainder = width % 4;
  int padding = (remainder) ? (FOUR_BYTE_ALIGNMENT - remainder) : 0;

  // i - current index in source
  int i = 0;
  int checkpoint = 0;
  // j - current index in destination
  int j = 0;

  // Switch between encoded and absolute mode
  bool encoded = false;

  // Loop over lines
  for (int m = 0; m < height; m++)
  {
    // Loop over pixels in m-th line
    int n = 0;
    while (n < width)
    {
      if (encoded)
      {
        // Initial setup
        char current = source[i];
        i++;
        n++;
        uint8_t count = 1;

        // Calculate count of equal consecutive bits on the same line and count should be below 256 (8 bits -> 0...255)
        while (n < width && source[i] == current && count < 255)
        {
          count++;
          i++;
          n++;
        }

        // Write compressed pixel(s) to destination array
        RLEPIXEL compressedPixel = {.count = count, .color = current};
        destination[j] = compressedPixel;
        j++;

        // Switch back to absolute mode and adjust the checkpoint
        encoded = false;
        checkpoint = i;
      }
      else
      {
        // Save the current column, might be used later
        int copy = n;

        // Initial setup
        char current = source[i];
        uint8_t limit = 1; // used to stay below 256 (8 bits -> 0...255)
        i++;
        n++;

        // If we're at the beginning of a run, 3 consecutives bytes should be compressed with encoded mode
        if (n < width && source[i] == current && n + 1 < width && source[i + 1] == current)
        {
          // reset to latest checkpoint and switch to encoded mode
          i = checkpoint;
          n = copy;
          encoded = true;
          continue;
        }

        // Calculate count of equal consecutive bits on the same line
        while (((n < width && source[i] != current) ||
                (n + 1 < width && source[i + 1] != current) ||
                (n + 2 < width && source[i + 2] != current)) &&
               limit < 255)
        {
          current = source[i];
          limit++;
          i++;
          n++;
        }

        // If we're still on the same line, go back one step and switch to encoded mode
        if (n < width)
        {
          i--;
          n--;
          encoded = true;
        }

        int count = i - checkpoint;

        // Can't use absolute mode with count < 3, so reset to latest checkpoint and switch to encoded mode
        if (count < 3)
        {
          i = checkpoint;
          n = copy;
          encoded = true;
          continue;
        }

        // Write absolute mode denotation
        RLEPIXEL denotation = {.count = 0, .color = count};
        destination[j] = denotation;
        j++;

        // Move checkpoint to i and copy the bytes on the way
        while (checkpoint < i)
        {
          // Since absolute runs should be word-aligned,
          // We write a 0 at the ending if the number of bytes that were copied is odd
          RLEPIXEL pixels = {.count = source[checkpoint],
                             .color = checkpoint + 1 < i ? source[checkpoint + 1] : 0};

          destination[j] = pixels;
          j++;
          checkpoint += 2;
        }

        // Set checkpoint at the correct spot
        checkpoint = i;
      }
    }

    i += padding;
    checkpoint += padding;

    // If we're not in the last line, write end of line denotation
    if (m < height - 1)
    {
      RLEPIXEL denotation = {.count = 0, .color = 0};
      destination[j] = denotation;
      j++;
    }
  }

  // Write end of bitmap denotation
  RLEPIXEL denotation = {.count = 0, .color = 1};
  destination[j] = denotation;
  j++;
  return j;
}

// Returns average compression time
double benchmark(const unsigned long n, const char *algorithm, const char *pixels, RLEPIXEL *compressed, const int width, const int height)
{
  double total = 0.0;
  struct timespec start;
  struct timespec end;

  // for loops inside if statements to save time performing if checks for every iteration
  // get start time and end time right before and after the loop to be as accurate as possible
  if (!strcmp(algorithm, "simd"))
  {
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (unsigned long i = 0; i < n; i++)
    {
      bmp_rle_simd(pixels, compressed, width, height);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
  }
  else if (!strcmp(algorithm, "asm"))
  {
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (unsigned long i = 0; i < n; i++)
    {
      bmp_rle(pixels, compressed, width, height);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
  }
  else
  {
    clock_gettime(CLOCK_MONOTONIC, &start);
    for (unsigned long i = 0; i < n; i++)
    {
      compress(pixels, compressed, width, height);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
  }

  total = end.tv_sec - start.tv_sec + 1e-9 * (end.tv_nsec - start.tv_nsec);

  return total / n;
}

// Returns 0 if file is valid, otherwise 1
int checkValidity(const BITMAPFILEHEADER *restrict header, const BITMAPINFOHEADER *restrict info)
{
  if (header->fileType != 0x4d42 ||
      header->reserved1 != 0 ||
      header->reserved2 != 0 ||
      info->planes != 1 ||
      info->bitsPerPixel != 8 ||
      info->compression != 0)
  {
    fprintf(stderr, "Unsupported file format.\n");
    return 1;
  }

  return 0;
}
