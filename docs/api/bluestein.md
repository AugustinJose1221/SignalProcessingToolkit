# bluestein

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

A transform of any size. Declared in `ffitt/transform/bluestein.h`.

[Back to the index](../API.md) | [How the transform modules work](../../ffitt/transform/README.md)

## Macros

### `BLUESTEIN_LARGEST_SIZE`

```c
#define BLUESTEIN_LARGEST_SIZE      ((uint32_t)1u << 20)
```

The largest size this module takes.

The transform inside must hold at least twice the size, thus a larger size
than this cannot be served by any power of two that fits in the count.

### `BLUESTEIN_CHIRP_COUNT`

```c
#define BLUESTEIN_CHIRP_COUNT(size)     (size)
```

The number of turning factors that a transform of the given size needs.

## Types

### `bluestein_t`

```c
typedef struct{
    uint32_t size;              // The number of points, any size
    fft_t fft;                  // The transform of a power of two inside
    cnum_t* chirp;              // One turning factor for each point
    cnum_t* kernel;             // The shape to convolve with, transformed
    cnum_t* first;              // Working room, the larger size
    cnum_t* second;             // Working room, the larger size
    bool dynamic_alloc;         // True if the memory comes from the heap
}bluestein_t;
```

## Functions

### `bluestein_is_valid_size`

```c
bool bluestein_is_valid_size(uint32_t size);
```

True if the module can transform this size. Any size from 2 up to
BLUESTEIN_LARGEST_SIZE will do, whether it is a power of two or not.

### `bluestein_transform_size`

```c
uint32_t bluestein_transform_size(uint32_t size);
```

Give the size of the transform that runs inside, which is the smallest power
of two that holds the convolution whole.

Use this to work out the memory before allocating: it is the size of the
transform inside and the length of both working buffers. Give 0 for a size
the module cannot serve.

### `bluestein_alloc`

```c
bluestein_t bluestein_alloc(uint32_t size);
```

Give a transform for the given number of points. The memory comes from the
heap. Give it to bluestein_free when it is no longer needed.

A transform whose size is 0 came back because the size is not one the module
takes. Examine the size with bluestein_is_valid_size first.

### `bluestein_static_alloc`

```c
bluestein_t bluestein_static_alloc(uint32_t size, cnum_t* twiddle, uint32_t* reverse, cnum_t* chirp, cnum_t* kernel, cnum_t* first, cnum_t* second);
```

Give a transform that uses the memory the caller holds, taking nothing from
the heap.

The table twiddle must hold FFT_TWIDDLE_COUNT(m) complex numbers and reverse
must hold FFT_REVERSE_COUNT(m) values, where m is bluestein_transform_size
of the size. The table chirp must hold BLUESTEIN_CHIRP_COUNT(size) complex
numbers, and kernel, first and second must each hold m of them.

### `bluestein_forward`

```c
void bluestein_forward(bluestein_t* bluestein, cnum_t* data);
```

Change the given data from the time domain into the frequency domain.

The data holds as many complex numbers as the size, and the result is
written over it. The result is the same as a transform of that size worked
out directly, to the precision the table above records.

### `bluestein_inverse`

```c
void bluestein_inverse(bluestein_t* bluestein, cnum_t* data);
```

Change the given data from the frequency domain into the time domain.

A forward transform and then an inverse transform give the first data again.

### `bluestein_bin_frequency`

```c
real_t bluestein_bin_frequency(uint32_t index, uint32_t size, real_t sample_rate);
```

Give the frequency in hertz that the bin with the given index holds.

This is the reason to use a size that is not a power of two. At 3000 samples
in a second and a size of 60, bin 1 holds exactly 50 hertz.

### `bluestein_free`

```c
void bluestein_free(bluestein_t* bluestein);
```

Release the memory of a transform that came from bluestein_alloc. This does
nothing for one that came from bluestein_static_alloc, thus a call for
either kind is safe. A second call does nothing.
