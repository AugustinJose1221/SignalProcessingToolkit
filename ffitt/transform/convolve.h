#ifndef CONVOLVE_H
#define CONVOLVE_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <ffitt/core/real.h>
#include <ffitt/linalg/cnum.h>
#include <ffitt/transform/fft.h>
#else
#include "real.h"
#include "cnum.h"
#include "fft.h"
#endif

// Sliding one signal along another, multiplying and adding at every place.
//
// This is the most basic operation there is in the field. Passing a signal
// through a filter IS a convolution with the coefficients of that filter, and
// the fir module is this operation done one sample at a time. This module does
// the same thing to a whole block at once, which is what a caller wants when
// the signal is already in hand.
//
// WHAT IT IS FOR, BESIDE FILTERING
//
// A SHAPE THAT A SIGNAL PASSES THROUGH. The answer of a room to a clap, of a
// spring to a knock, of a cable to a step: measure that answer once, and
// convolving any signal with it says what comes out the other side.
//
// SMOOTHING BY A SHAPE OF THE CALLER'S OWN. A mean over a window is a
// convolution with a block of equal values. Any other shape is a convolution
// with that shape, and the caller need not build a filter to use one.
//
// HOW IT DIFFERS FROM CORRELATION, WHICH IS THE USUAL CONFUSION
//
// The two are the same sum with one difference: a convolution TURNS ONE SIGNAL
// ROUND before sliding it. For a shape that is the same forwards and backwards
// they give the same answer, and a great deal of code is written on that
// assumption and then meets a shape that is not.
//
// Take a convolution when a signal PASSES THROUGH something. Take a
// correlation when you are asking HOW ALIKE two things are. The correlate
// module answers the second.
//
// HOW LONG THE ANSWER IS
//
// Sliding a shape of m along a signal of n touches n+m-1 places, and at most
// of them the two hang over each other's ends. What to do about that is the
// mode, and it must be chosen deliberately:
//
//   CONVOLVE_FULL   every place, n+m-1 values. Nothing is thrown away, and the
//                   ends are built from a signal that was taken to be zero
//                   outside itself.
//   CONVOLVE_SAME   n values, the middle of the full answer. This lines up
//                   with the input sample for sample, which is what smoothing
//                   wants, and the two ends are still built from that
//                   assumed zero.
//   CONVOLVE_VALID  n-m+1 values, only where the shape lies wholly inside the
//                   signal. NOTHING IS ASSUMED here, thus every value is
//                   real, and the answer is shorter than the input.
//
// TAKE CONVOLVE_VALID WHEN THE ENDS MATTER. The other two report values at the
// ends that were partly invented, and they look no different from the rest.
//
// WHAT IT COSTS
//
// The plain way multiplies and adds once for each sample of the shape at every
// place, thus n times m. For a signal of 4096 and a shape of 512 that is two
// million operations.
//
// A convolution in time is a multiplication in frequency, thus the transform
// does the same work in three transforms. For those numbers that is about 400
// thousand, which is five times less. Below a shape of about 60 the plain way
// wins, because the transform has a fixed cost that it has not.

typedef enum{
    CONVOLVE_FULL = 0,          // Every place the two touch
    CONVOLVE_SAME,              // As long as the signal, lined up with it
    CONVOLVE_VALID              // Only where the shape lies wholly inside
}convolve_mode_t;

// True if the module knows this mode.
bool convolve_is_valid_mode(convolve_mode_t mode);

// How many values the answer holds, or 0 if there is no answer to give.
//
// CONVOLVE_VALID gives 0 when the shape is longer than the signal, because
// there is then no place where the shape lies wholly inside.
uint32_t convolve_output_size(uint32_t signal_size, uint32_t shape_size,
                              convolve_mode_t mode);

// Slide the shape along the signal, the plain way.
//
// The output must hold convolve_output_size values. The input and the output
// must not be the same list.
//
// Give false if either size is nothing, if the mode is unknown, or if there is
// no answer to give.
bool convolve_direct(const real_t* signal, uint32_t signal_size,
                     const real_t* shape, uint32_t shape_size,
                     real_t* output, convolve_mode_t mode);

// Give the size of the transform that the fast way needs, or 0 if no transform
// of a size it can use is large enough.
//
// The transform must be at least as long as the whole answer, so that the two
// ends cannot wrap round and add to each other.
uint32_t convolve_transform_size(uint32_t signal_size, uint32_t shape_size);

// Slide the shape along the signal, using the transform.
//
// This gives the same answer as convolve_direct, to the last digit the width
// can hold, and costs far less for a long shape.
//
// The caller gives everything it needs, thus this module takes no memory of
// its own and works on a target with no heap:
//
//   fft      made for convolve_transform_size, by fft_alloc or fft_static_alloc
//   first    that many complex values
//   second   that many complex values
//   work     that many real values
//
// Give false for the same reasons as convolve_direct, or if the transform that
// was given is not of the right size.
bool convolve_by_transform(const real_t* signal, uint32_t signal_size,
                           const real_t* shape, uint32_t shape_size,
                           real_t* output, convolve_mode_t mode,
                           fft_t* fft, cnum_t* first, cnum_t* second,
                           real_t* work);

#endif//CONVOLVE_H
