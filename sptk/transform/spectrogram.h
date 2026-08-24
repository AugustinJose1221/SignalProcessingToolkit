#ifndef SPECTROGRAM_H
#define SPECTROGRAM_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#include <sptk/linalg/cnum.h>
#include <sptk/transform/stft.h>
#else
#include "real.h"
#include "cnum.h"
#include "stft.h"
#endif

// What the frames of a short-time transform mean, in a unit that can be read.
//
// stft_forward gives complex numbers, and a complex number is not something to
// look at. This module turns those frames into one real number for each bin of
// each frame, in whichever of four units the question asks for.
//
// THE SCALING IS THE PART THAT IS USUALLY WRONG, and the wrong answer looks
// perfectly reasonable. A transform of a longer block gives larger numbers for
// the same signal; a window makes them smaller; and half of the power sits in
// the mirrored half that is not there. Left uncorrected, the same tone reads
// differently for every choice of block and window, and the number means
// nothing outside the one program that made it.
//
// This module corrects all three, thus:
//
//   A WAVE OF AMPLITUDE A READS AS A, in SPECTROGRAM_AMPLITUDE, whatever the
//   block, the window or the hop.
//
// The four units:
//
//   SPECTROGRAM_AMPLITUDE  how large the wave at that bin is, in the unit of
//                          the signal. A wave of amplitude 2 reads 2.
//   SPECTROGRAM_POWER      the mean power of that wave, which is A*A/2. This
//                          is what adds up across bins.
//   SPECTROGRAM_DENSITY    power for each hertz, which is what psd gives, and
//                          the only one of the four that does not change when
//                          the block gets longer.
//   SPECTROGRAM_DECIBEL    the power in decibels against a reference of 1.
//
// WHICH ONE TO ASK FOR. To read the size of a tone off a picture, amplitude.
// To add the power of a band together, density. To draw the picture at all,
// decibels, because a spectrogram of anything real covers so many factors of
// ten that a linear scale shows one bright line and black everywhere else.
//
// THE FLOOR UNDER THE DECIBELS IS NOT A DETAIL. The logarithm of nothing has
// no value, and a bin that holds nothing is a thing that happens: a silent
// stretch of recording, or a bin above the cutoff of a filter. Without a floor
// the answer holds values that no arithmetic and no picture can use.
// SPECTROGRAM_FLOOR_DECIBEL is where this module stops.

// Which unit the answer is in.
typedef enum{
    // How large the wave at that bin is, in the unit of the signal.
    SPECTROGRAM_AMPLITUDE = 0,

    // The mean power of that wave, which is the amplitude squared and halved.
    SPECTROGRAM_POWER,

    // Power for each hertz. The only unit here that does not change when the
    // block gets longer.
    SPECTROGRAM_DENSITY,

    // The power in decibels against a reference of 1, held at
    // SPECTROGRAM_FLOOR_DECIBEL from below.
    SPECTROGRAM_DECIBEL
}spectrogram_kind_t;

// The lowest value the decibel unit gives.
//
// A bin holding nothing has no logarithm. 200 decibels below a reference of 1
// is far below anything a measurement can reach at either width, thus the
// floor cannot hide a real reading, and it keeps the answer to numbers that
// arithmetic and pictures can use.
#define SPECTROGRAM_FLOOR_DECIBEL   (-REAL_C(200.0))

// True if the unit is one this module knows.
bool spectrogram_is_valid_kind(spectrogram_kind_t kind);

// How many values a spectrogram of this many frames holds.
uint32_t spectrogram_value_count(const stft_t* stft, uint32_t frame_count);

// Turn the frames of a short-time transform into one real number for each bin.
//
// The frames are what stft_forward gave, and the output holds
// spectrogram_value_count values laid out the same way: the bin b of the frame
// f sits at (f * STFT_BIN_COUNT(block)) + b.
//
// The sample rate is used by SPECTROGRAM_DENSITY only, and it is ignored by
// the other three.
//
// Give false if the transform has not been designed, if the unit is unknown,
// if there are no frames, or if the room is too small.
bool spectrogram_build(const stft_t* stft, const cnum_t* frames,
                       uint32_t frame_count, spectrogram_kind_t kind,
                       real_t sample_rate, real_t* output, uint32_t room);

// Give the largest value in a spectrogram, which is what a picture is usually
// drawn against.
real_t spectrogram_largest(const real_t* values, uint32_t count);

// Turn a spectrogram of decibels into one measured from its own largest value,
// so that the largest reads 0 and everything else is below it.
//
// This is how a spectrogram is nearly always drawn, because the reading that
// matters is which parts are loud AGAINST THE REST and not against a reference
// that the recording never knew about. The output may be the input.
//
// Give false if the values are not decibels, which the caller must know, or if
// there are none.
bool spectrogram_against_the_largest(const real_t* values, uint32_t count,
                                     real_t* output);

#endif//SPECTROGRAM_H
