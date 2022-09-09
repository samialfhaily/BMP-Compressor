# BMP Image Compressor

This is an 8 bits-per-pixel bmp image compression CLI tool written in C and Assembly.

The tool uses the RLE compression algorithm written in `C`, `Assembly`, and `Assembly using SIMD`.

You can specify which implementation you want to choose when executing the program on an image using the `-a` option. The default used implementation is `C`.

**Example Usage**

```c
./compress -a c --output ./output.bmp ./input.bmp
./compress --algorithm asm --output ./output.bmp ./input.bmp
./compress -b 42 -o ./output.bmp -a simd ./input.bmp // runs simd implementation 42 times and logs benchmarks
```

For more options, run `./compress --help`.
