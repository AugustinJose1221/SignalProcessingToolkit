#ifndef FFT_H
#define FFT_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#include <sptk/linalg/cnum.h>
#else
#include "real.h"
#include "cnum.h"
#endif

// The fast Fourier transform.
//
// The transform changes a signal in the time domain into a signal in the
// frequency domain. Element k of the result says how much of the signal turns
// at the frequency of the bin k. Use fft_bin_frequency to get that frequency
// in hertz.
//
// The module takes a size that is a power of two, such as 64, 256 or 1024.
// This is the radix-2 method of Cooley and Tukey. A size that is not a power
// of two needs another method with much more code, and that method holds less
// accuracy in a float. Use fft_is_valid_size to examine a size.
//
// The transform needs two tables that depend on the size only: the turning
// factors and the order of the bit reversal. The module calculates them one
// time at the allocation, thus a transform itself gets no memory. A program
// that transforms the same size again and again makes the fft_t one time.
//
// A float holds about 7 digits. The error of the transform grows with the
// logarithm of the size. Up to about 4096 points the result keeps at least 4
// digits. Above that, examine whether the accuracy is still enough for your
// work.

typedef struct{
    uint32_t size;              // The number of points, a power of two
    cnum_t* twiddle;            // The turning factors, size/2 of them
    uint32_t* reverse;          // The order of the bit reversal, size of them
    bool dynamic_alloc;         // True if the memory comes from the heap
}fft_t;

// The number of turning factors that a transform of the given size needs.
#define FFT_TWIDDLE_COUNT(size)     ((size)/2)

// The number of indices of the bit reversal that a transform of the given size
// needs.
#define FFT_REVERSE_COUNT(size)     (size)

// True if the size is a power of two and larger than one. Only such a size
// works with this module.
bool fft_is_valid_size(uint32_t size);

// Give a transform for the given number of points. The memory comes from the
// heap. Give the transform to fft_free when you no longer need it.
fft_t fft_alloc(uint32_t size);

// Give a transform that uses the memory that the caller holds. The table
// twiddle must hold FFT_TWIDDLE_COUNT(size) complex numbers, and the table
// reverse must hold FFT_REVERSE_COUNT(size) values. This function takes no
// memory from the heap.
fft_t fft_static_alloc(uint32_t size, cnum_t* twiddle, uint32_t* reverse);

// Change the given data from the time domain into the frequency domain. The
// data must hold as many complex numbers as the size of the transform. The
// function writes the result over the data, and it gets no memory.
void fft_forward(fft_t* fft, cnum_t* data);

// Change the given data from the frequency domain into the time domain. This
// operation is the opposite of fft_forward: a forward transform and then an
// inverse transform give the first data again. The function writes the result
// over the data, and it gets no memory.
void fft_inverse(fft_t* fft, cnum_t* data);

// Change a signal of float values into the frequency domain.
//
// The function writes each value of the input into the real part of the
// output, sets each imaginary part to zero, and then does a forward transform.
// The output must hold as many complex numbers as the size of the transform.
//
// A signal of real values gives a result where the second half mirrors the
// first half. Thus only the bins from 0 to size/2 hold new information. This
// function is not the faster method that uses that mirror. It gives the same
// result with less code.
void fft_forward_real(fft_t* fft, const real_t* input, cnum_t* output);

// Write the size of each element of the data into the magnitude list. The size
// of an element says how strong that frequency is in the signal. Both lists
// must hold as many values as the given size.
void fft_magnitude(const cnum_t* data, real_t* magnitude, uint32_t size);

// Write the square of the size of each element into the power list. This
// function takes no square root, thus it is faster than fft_magnitude. Both
// lists must hold as many values as the given size.
void fft_power(const cnum_t* data, real_t* power, uint32_t size);

// Give the frequency in hertz that the bin with the given index holds. The
// sample rate is the number of samples in one second.
//
// A bin above size/2 holds a frequency above half the sample rate. Such a bin
// mirrors a lower bin, and this function gives the negative frequency for it,
// which is the frequency that the mirror holds.
real_t fft_bin_frequency(uint32_t index, uint32_t size, real_t sample_rate);

// Release the memory of a transform that came from fft_alloc. This function
// does nothing for a transform that came from fft_static_alloc, thus a call
// for either kind is safe. A second call does nothing.
void fft_free(fft_t* fft);

#endif//FFT_H
