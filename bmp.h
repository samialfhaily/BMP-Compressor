#include <stdint.h>
#include <stddef.h>

#define ENDOFLINE 2
#define ENDOFBITMAP 2
#define FILEHEADERSIZE 14
#define INFOHEADERSIZE 40
#define FOUR_BYTE_ALIGNMENT 4

// 14 bytes
// 5 fields
typedef struct
{
	uint16_t fileType;
	unsigned int fileSize;
	unsigned short reserved1;
	unsigned short reserved2;
	unsigned int pixelDataOffset;
} __attribute__((packed)) BITMAPFILEHEADER;

// 40 bytes
// 11 fields
typedef struct
{
	unsigned int headerSize;
	int imageWidth;
	int imageHeight;
	unsigned short planes;
	unsigned short bitsPerPixel;
	unsigned int compression;
	unsigned int imageSize;
	int xPixelsPerMeter;
	int yPixelsPerMeter;
	unsigned int totalColors;
	unsigned int importantColors;
} __attribute__((packed)) BITMAPINFOHEADER;

// 4 bytes per entry
// 4 fields
typedef struct
{
	unsigned char red;
	unsigned char green;
	unsigned char blue;
	unsigned char reserved;
} __attribute__((packed)) ColorTableEntry;

typedef struct
{
	char count;
	char color;
} RLEPIXEL;
