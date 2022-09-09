CC=gcc
CFLAGS+=-std=gnu11 -O3 -Wall -Wextra -Werror -Wpedantic -Wshadow

.PHONY: all
all: compress
compress: compress.c bmp_rle.S bmp_rle_simd.S

.PHONY: clean
clean:
	rm -f compress
