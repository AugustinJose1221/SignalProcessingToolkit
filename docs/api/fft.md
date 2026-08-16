# fft

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

The fast Fourier transform. Declared in `sptk/transform/fft.h`.

[Back to the index](../API.md)

## Macros

### `FFT_TWIDDLE_COUNT`

```c
#define FFT_TWIDDLE_COUNT(size)     ((size)/2)
```

The number of turning factors that a transform of the given size needs.

### `FFT_REVERSE_COUNT`

```c
#define FFT_REVERSE_COUNT(size)     (size)
```

The number of indices of the bit reversal that a transform of the given size
needs.

## Types

### `fft_t`

```c
typedef struct{
    uint32_t size;              // The number of points, a power of two
    cnum_t* twiddle;            // The turning factors, size/2 of them
    uint32_t* reverse;          // The order of the bit reversal, size of them
    bool dynamic_alloc;         // True if the memory comes from the heap
}fft_t;
```

## Functions

### `fft_is_valid_size`

```c
bool fft_is_valid_size(uint32_t size);
```

True if the size is a power of two and larger than one. Only such a size
works with this module.

### `fft_alloc`

```c
fft_t fft_alloc(uint32_t size);
```

Give a transform for the given number of points. The memory comes from the
heap. Give the transform to fft_free when you no longer need it.

### `fft_static_alloc`

```c
fft_t fft_static_alloc(uint32_t size, cnum_t* twiddle, uint32_t* reverse);
```

Give a transform that uses the memory that the caller holds. The table
twiddle must hold FFT_TWIDDLE_COUNT(size) complex numbers, and the table
reverse must hold FFT_REVERSE_COUNT(size) values. This function takes no
memory from the heap.

### `fft_forward`

```c
void fft_forward(fft_t* fft, cnum_t* data);
```

Change the given data from the time domain into the frequency domain. The
data must hold as many complex numbers as the size of the transform. The
function writes the result over the data, and it gets no memory.

### `fft_inverse`

```c
void fft_inverse(fft_t* fft, cnum_t* data);
```

Change the given data from the frequency domain into the time domain. This
operation is the opposite of fft_forward: a forward transform and then an
inverse transform give the first data again. The function writes the result
over the data, and it gets no memory.

### `fft_forward_real`

```c
void fft_forward_real(fft_t* fft, const float* input, cnum_t* output);
```

Change a signal of float values into the frequency domain.

The function writes each value of the input into the real part of the
output, sets each imaginary part to zero, and then does a forward transform.
The output must hold as many complex numbers as the size of the transform.

A signal of real values gives a result where the second half mirrors the
first half. Thus only the bins from 0 to size/2 hold new information. This
function is not the faster method that uses that mirror. It gives the same
result with less code.

### `fft_magnitude`

```c
void fft_magnitude(const cnum_t* data, float* magnitude, uint32_t size);
```

Write the size of each element of the data into the magnitude list. The size
of an element says how strong that frequency is in the signal. Both lists
must hold as many values as the given size.

### `fft_power`

```c
void fft_power(const cnum_t* data, float* power, uint32_t size);
```

Write the square of the size of each element into the power list. This
function takes no square root, thus it is faster than fft_magnitude. Both
lists must hold as many values as the given size.

### `fft_bin_frequency`

```c
float fft_bin_frequency(uint32_t index, uint32_t size, float sample_rate);
```

Give the frequency in hertz that the bin with the given index holds. The
sample rate is the number of samples in one second.

A bin above size/2 holds a frequency above half the sample rate. Such a bin
mirrors a lower bin, and this function gives the negative frequency for it,
which is the frequency that the mirror holds.

### `fft_free`

```c
void fft_free(fft_t* fft);
```

Release the memory of a transform that came from fft_alloc. This function
does nothing for a transform that came from fft_static_alloc, thus a call
for either kind is safe. A second call does nothing.
