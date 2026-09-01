#ifndef DWT_H
#define DWT_H

#include <stdint.h>
#include <stdbool.h>
#ifndef TEST
#include <ffitt/core/real.h>
#else
#include "real.h"
#endif

// The discrete wavelet transform.
//
// The transform takes a signal apart into an approximation and a detail. The
// approximation holds the slow part of the signal at half the number of
// samples, and the detail holds the fast part, also at half the number of
// samples. Together they hold as many values as the signal, thus the transform
// loses nothing and dwt_inverse gives the signal again.
//
// A Fourier transform says which frequencies the signal holds but not where
// they are. A wavelet transform says both, because each value of the detail
// belongs to one place of the signal. Thus the transform suits a signal that
// holds a short event, such as a step or a spike.
//
// The main use is to take noise out of a signal. Take the transform, set every
// small value of the detail to zero, and take the inverse transform. The noise
// spreads over every value of the detail, while the signal itself holds few
// large values. Thus this step takes away much of the noise and keeps the
// edges of the signal, which a low pass filter would make round.
//
// The size of the signal must be even for one level. For several levels the
// size must divide by two as many times as there are levels.
//
// The module holds two wavelets:
//
// - Haar, which is the simplest one. It looks at two samples at a time, thus
//   it finds a step very well and a smooth curve badly.
// - Daubechies with four coefficients, which looks at four samples at a time.
//   It follows a curve better, and it gives a smoother result.

typedef enum{
    DWT_HAAR = 0,               // Two coefficients
    DWT_DAUBECHIES4 = 1         // Four coefficients
}dwt_wavelet_t;

// The largest number of coefficients that a wavelet of this module holds.
#define DWT_MAX_COEFFICIENT_COUNT   4u

typedef struct{
    dwt_wavelet_t wavelet;      // Which wavelet the transform uses
    uint32_t length;            // The number of coefficients of that wavelet
    real_t low[DWT_MAX_COEFFICIENT_COUNT];   // The filter of the approximation
    real_t high[DWT_MAX_COEFFICIENT_COUNT];  // The filter of the detail
}dwt_t;

// Give a transform that uses the given wavelet. This function takes no memory.
// The whole state lies inside the structure.
dwt_t dwt_init(dwt_wavelet_t wavelet);

// True if a signal of the given size can go through the given number of
// levels. Each level halves the size, thus the size must divide by two that
// many times, and the size of the last level must still hold at least two
// samples.
bool dwt_is_valid_size(uint32_t size, uint32_t levels);

// Take one level of the transform.
//
// The signal holds size values. The function writes size/2 values into the
// approximation and size/2 values into the detail. The size must be even, and
// the three lists must not be the same memory.
void dwt_forward(dwt_t* dwt, const real_t* signal, uint32_t size,
                 real_t* approximation, real_t* detail);

// Take one level of the inverse transform.
//
// The approximation and the detail hold size/2 values each, and the function
// writes size values into the signal. The size must be even.
void dwt_inverse(dwt_t* dwt, const real_t* approximation, const real_t* detail,
                 uint32_t size, real_t* signal);

// Take several levels of the transform, one after the other.
//
// The function writes the result over the signal. After the call the first
// size/(2^levels) values hold the approximation of the last level. The values
// after it hold the detail of the last level, then the detail of the level
// before it, and so on up to the detail of the first level, which fills the
// second half of the list.
//
// The work buffer must hold as many values as the signal. The function gets no
// memory.
void dwt_forward_multi(dwt_t* dwt, real_t* signal, uint32_t size, uint32_t levels,
                       real_t* work);

// Take several levels of the inverse transform. The list holds the result of
// dwt_forward_multi, and the function writes the signal over it. The work
// buffer must hold as many values as the signal.
void dwt_inverse_multi(dwt_t* dwt, real_t* signal, uint32_t size, uint32_t levels,
                       real_t* work);

// Set every value of the list whose size is below the limit to zero.
//
// Use this function on the detail values of a transform to take noise out of a
// signal. Give it the part of the list that holds the details, and not the
// approximation.
void dwt_threshold(real_t* data, uint32_t size, real_t limit);

#endif//DWT_H
