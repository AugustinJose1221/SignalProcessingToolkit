#ifndef WINDOW_H
#define WINDOW_H

#include <stdint.h>
#include <stdbool.h>
#ifndef TEST
#include <sptk/core/real.h>
#else
#include "real.h"
#endif

// Windows.
//
// A transform reads a block of samples and takes it to be one period of a
// signal that repeats for ever. Almost no real signal fits a block exactly.
// The end of the block then does not meet its start, and the transform sees a
// step there. A step holds every frequency, thus one tone smears across the
// whole result and a small tone beside a large one disappears under it.
//
// A window is a list of numbers that the block is multiplied by before the
// transform. It falls to nothing at both ends, thus the block always meets
// itself and there is no step.
//
// WHICH ONE TO TAKE
//
// Every window trades two things against each other. A tone that does not sit
// exactly on a bin spreads over the bins beside it: that spread is the MAIN
// LOBE, and a wider one hides a tone that stands close. What is left over
// reaches further out: those are the SIDE LOBES, and higher ones hide a tone
// that stands far away but is weak.
//
//   Window            Main lobe   Highest side lobe   Take it when
//   ----------------  ---------   -----------------   ------------------------
//   Rectangular          1.0            -13 dB        The block already fits,
//                                                     as for one whole period
//   Hann                 2.0            -31 dB        Nothing else is known.
//                                                     This is the usual choice
//   Hamming              2.0            -43 dB        One tone must be seen
//                                                     beside a near one
//   Blackman             3.0            -58 dB        A weak tone must be seen
//                                                     beside a strong one
//   Blackman-Harris      4.0            -92 dB        The same, when the strong
//                                                     one is very much larger
//   Tukey                varies         varies        Only the ends need to
//                                                     fall, and the middle
//                                                     must stay as it is
//   Kaiser               varies         varies        The side lobes must meet
//                                                     a number that is given
//
// The main lobe is in bins, against the rectangular window.
//
// WHAT A WINDOW DOES TO THE ANSWER
//
// A window makes the signal smaller, thus every height in the result is too
// low, and by how much depends on the window. Two numbers put that right:
//
//   window_coherent_gain    divide a peak by this to get the height of a tone
//   window_noise_gain       divide by this to get the height of noise
//
// Forgetting them is the usual fault. A Hann window has a coherent gain of
// 0.5, thus every tone comes out at half its height, and a reading that does
// not divide by it is wrong by a factor of two.
//
// This module gets no memory. It writes into a list that the caller holds.

typedef enum{
    WINDOW_RECTANGULAR = 0,     // No window. Every value is one
    WINDOW_HANN,                // The usual choice
    WINDOW_HAMMING,             // Lower first side lobe, higher far ones
    WINDOW_BLACKMAN,            // Lower side lobes, wider main lobe
    WINDOW_BLACKMAN_HARRIS,     // The lowest side lobes of the fixed windows
    WINDOW_TUKEY,               // Flat in the middle, falls at the ends
    WINDOW_KAISER               // The shape follows a parameter
}window_kind_t;

// True if the module knows this kind of window.
bool window_is_valid_kind(window_kind_t kind);

// True if a window of this kind can usefully be built at this size.
//
// A symmetric window of two values is its two ends, and the ends are where a
// taper is nothing. Thus every window that tapers is refused at a size of 2,
// and a rectangular window, which takes nothing away, is allowed at any size.
//
// The values a tapered window of 2 gives are not wrong; two ends really are
// all there is. But window_coherent_gain is then nothing or near it, and this
// header tells a caller to DIVIDE by that. Ask this first.
bool window_is_valid_size(uint32_t size, window_kind_t kind);

// True if this kind of window takes a parameter. A window that takes one needs
// window_build_with; a window that does not takes window_build.
bool window_takes_a_parameter(window_kind_t kind);

// Write the values of a window into the list, which must hold as many float
// values as the given size.
//
// The window is symmetric: the first value and the last value are the same.
// That is what a transform wants. A window for building a filter wants the
// same thing, thus this module serves both.
//
// A size of 1 gives the single value 1.
//
// A SIZE OF 2 IS DEGENERATE FOR EVERY WINDOW THAT TAPERS, and it is worth
// knowing before it is met. A symmetric window of two values is its two ends,
// and the ends are where a taper is nothing. Measured, the coherent gain:
//
//     size                 1       2       3       4
//     rectangular     1.0000  1.0000  1.0000  1.0000
//     hann            1.0000  0.0000  0.3333  0.3750
//     blackman        1.0000  0.0000  0.3333  0.3150
//     hamming         1.0000  0.0800  0.3867  0.4250
//
// The values are right; two ends really are all there is. But a caller that
// follows window_coherent_gain and DIVIDES by it has divided by nothing. From
// a size of 3 upwards every window here has a gain worth dividing by. Use no
// tapered window below 3.
void window_build(real_t* window, uint32_t size, window_kind_t kind);

// Write the values of a window that takes a parameter.
//
// For WINDOW_TUKEY the parameter is the part of the window that falls, from 0
// to 1. At 0 the window is rectangular and nothing falls; at 1 it is a Hann
// window and everything falls. At 0.5 the middle half stays as it is.
//
// For WINDOW_KAISER the parameter is beta, which is 0 or more. A larger beta
// gives lower side lobes and a wider main lobe. Measured, for a window of 64:
//
//     beta          0     2     4    5.65    6     8    8.6    10    12
//     side lobe   -13   -19   -31    -42   -44   -58   -63    -74   -90  dB
//
// Thus 0 gives a rectangular window and about 6 gives a window near Blackman.
// Use window_kaiser_beta to get beta from the stop band that a filter needs.
//
// A kind that takes no parameter ignores it, thus this function can always
// stand in for window_build.
void window_build_with(real_t* window, uint32_t size, window_kind_t kind,
                       real_t parameter);

// Give one value of a window, without building the whole of it. This suits a
// caller that has no room to hold the window, and one that builds a window
// into another list as it goes.
real_t window_value(uint32_t index, uint32_t size, window_kind_t kind,
                   real_t parameter);

// Give the beta of a Kaiser window for a FILTER whose stop band must lie the
// given number of decibels down. Give a positive number: 60 means 60 dB down.
// This is the rule of Kaiser.
//
// READ WHAT THIS NUMBER IS, because it is easy to take it for the other one.
// It is the stop band of a filter that is BUILT with the window. It is NOT the
// level of the side lobes of the window itself, and the two are far apart.
// Measured:
//
//     asked for       26    45    60    81    87    99   dB of stop band
//     beta            2.0   4.0   5.7   8.0   8.6  10.0
//     window lobes   -19   -31   -42   -58   -63   -74   dB
//
// The side lobes of the window always lie about 18 to 27 dB higher than the
// stop band that the same beta gives a filter. A reader who wants a window
// whose own side lobes lie 60 dB down needs a beta near 8.2, not near 5.7.
// Take beta from the table above the declaration of window_build_with for
// that, and take this function only for designing a filter.
real_t window_kaiser_beta(real_t stop_band_decibel);

// Give the coherent gain of a window, which is the mean of its values.
//
// A tone that stands exactly on a bin comes out of the transform at this part
// of its true height. Divide the height of a peak by this number to read the
// height of the tone. A rectangular window gives 1, a Hann window gives 0.5.
real_t window_coherent_gain(const real_t* window, uint32_t size);

// Give the noise gain of a window, which is the root of the mean of the
// squares of its values.
//
// Noise, unlike a tone, does not stand on one bin. It comes out at this part
// of its true size. Divide by this number to read the size of the noise. A
// rectangular window gives 1, a Hann window gives about 0.61.
real_t window_noise_gain(const real_t* window, uint32_t size);

// Give the equivalent noise bandwidth of a window, in bins.
//
// This is how many bins of noise a single bin holds after the window. A
// rectangular window gives 1.0, a Hann window gives 1.5. A measurement of the
// density of noise divides by this, and by the width of a bin.
real_t window_noise_bandwidth(const real_t* window, uint32_t size);

// Multiply a block of samples by a window. The input and the output may be the
// same list.
void window_apply(const real_t* window, const real_t* input, real_t* output,
                  uint32_t size);

#endif//WINDOW_H
