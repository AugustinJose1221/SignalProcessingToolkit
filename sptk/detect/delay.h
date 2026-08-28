#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#include <sptk/linalg/cnum.h>
#include <sptk/transform/fft.h>
#else
#include "real.h"
#include "cnum.h"
#include "fft.h"
#endif

// Find how far one reading stands behind another, to below a sample.
//
// Two microphones hear the same sound and one hears it later. Two coils see the
// same pulse pass. A sounder sends and hears back. In each of them the delay IS
// the measurement: it gives the direction the sound came from, the speed the
// metal moved at, the depth of the water.
//
// A delay of a whole number of samples is easy, and correlate_best_lag already
// gives it. THE WHOLE NUMBER IS RARELY THE ANSWER. At 48 000 samples a second,
// one sample of delay between two microphones a hand apart is the difference
// between one bearing and another seven degrees away. Rounding the delay to a
// sample throws that away, and nothing in the answer says it was thrown away.
//
// TWO WAYS, AND THEY FAIL DIFFERENTLY.
//
//   FROM THE CORRELATION. Slide one reading along the other, find where they
//   agree best, and fit a curve through that point and its two neighbours. It
//   works on anything, it needs no transform, and it is only as fine as the
//   curve fits: a peak that is not shaped like the curve leans the answer
//   towards the nearer neighbour, and that lean does not go away with more
//   samples.
//
//   FROM THE PHASE. A delay turns into a slope of phase across the spectrum,
//   thus measuring that slope measures the delay. It uses every frequency the
//   two readings share instead of three points, thus it is finer and it settles
//   as the reading grows. It asks that the two readings really be the same
//   thing delayed. Where the path colours one of them, the slope leans.
//
//   AND IT ASKS FOR A READING THAT FILLS A BAND. The slope is read from how far
//   the phase turns from ONE BIN TO THE NEXT, thus a bin whose neighbour is
//   quiet contributes nothing. A handful of tones far apart leaves every step
//   saying nothing and the answer is then the rounding: measured on nine tones
//   spread across the band, a delay of 7 samples came back as 1.6. A rush of
//   noise, a chirp, a knock, or anything else that fills a band works. A few
//   loud tones do not, and the correlation is the way to reach for there.
//
// USE BOTH WHERE IT MATTERS. They agree when the reading suits them and part
// company when it does not, and that parting is the only warning either gives.

typedef enum{
    DELAY_CORRELATE = 0,        // From the peak of the cross correlation
    DELAY_PHASE                 // From the slope of the phase
}delay_way_t;

// Give whether this is one of the ways.
bool delay_is_valid_way(delay_way_t way);

// How many values the working list must hold for delay_by_correlation at this
// largest lag. Both signs of lag are needed, thus the count is twice the lag
// and one more for the lag of nothing.
#define DELAY_WORK_COUNT(largest_lag)   (((largest_lag) * 2u) + 1u)

// Fit a curve through a peak and its two neighbours, and give how far the top
// of that curve stands from the middle point, between -0.5 and 0.5.
//
// THIS IS peakdetect_refine UNDER ANOTHER NAME, and it gives exactly what that
// gives. It is here because refining the peak of a correlation is what this
// module does with it, and a caller working on delays should not have to know
// that the same question is asked of a spectrum. Reach for either.
//
// Give 0 where the peak stands at either end, because there are not three
// points there, and where the three points do not curve downwards, because then
// the middle point is not a peak at all.
real_t delay_refine_peak(const real_t* values, uint32_t count, uint32_t peak);

// How far the second reading stands behind the first, in samples, found from
// where the two agree best.
//
// A positive answer means the second reading is LATER. The answer is refined
// below a sample by delay_refine_peak.
//
// The working list must hold DELAY_WORK_COUNT(largest_lag) values. The largest
// lag must be smaller than the size, and it should be comfortably smaller: at a
// lag near the size the two readings barely overlap and the agreement is
// measured on a handful of samples.
//
// The strength says how much the two readings agree where they agree best,
// between -1 and 1. IT MUST BE READ. A pair of readings with nothing in common
// still has a place where they agree best, and the delay to that place is a
// number with nothing behind it. Give NULL if it is not wanted.
//
// Give false, and leave both answers as they were, if the way does not fit
// inside the reading.
bool delay_by_correlation(const real_t* first, const real_t* second,
                          uint32_t size, uint32_t largest_lag, real_t* work,
                          real_t* delay, real_t* strength);

// How far the second reading stands behind the first, in samples, found from
// the slope of the phase across the spectrum.
//
// The two lists of complex numbers are working room and must each hold as many
// values as the transform. Both readings must hold at least that many samples,
// and the transform decides how many are used: a longer transform uses more of
// the reading and settles closer.
//
// THE DELAY MUST BE SMALLER THAN HALF THE TRANSFORM. A slope of phase is only
// known between minus half a turn and half a turn from one bin to the next, thus
// a delay past that point comes back as a smaller one and nothing says so. Where
// the delay may be large, find the whole number of samples with
// delay_by_correlation first, take it off, and measure what is left with this.
//
// Give false if the transform is not one this can use or if either reading is
// shorter than it.
bool delay_by_phase(const real_t* first, const real_t* second, uint32_t size,
                    fft_t* fft, cnum_t* first_work, cnum_t* second_work,
                    real_t* delay);

#endif//DELAY_H
