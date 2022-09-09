# BMP Image Compressor

## Description
This is an 8 bits-per-pixel bmp image compression CLI tool written in C and Assembly.

The tool uses the RLE compression algorithm written in `C`, `Assembly`, and `Assembly using SIMD`.

You can specify which implementation you want to choose when executing the program on an image using the `-a` option. The default used implementation is `C`.

**Note, the assembly implementations only work on Intel CPUs.**

## Usage

First, run `make` to build an executable.

```c
// Normal Usage
./compress -a c -o ./output.bmp ./input.bmp // compresses `input.bmp` using the C implementation
./compress --algorithm asm --output ./output.bmp ./input.bmp // compresses `input.bmp` using the assembly implementation

// Benchmarking
./compress --benchmark 10 -a c ./input.bmp // runs C implementation 10 times and logs benchmarks
./compress -b 42 -o ./output.bmp -a simd ./input.bmp // runs SIMD implementation 42 times, logs benchmarks, and outputs compressed image in `output.bmp`

// Help
./compress --help // lists all available options
```
