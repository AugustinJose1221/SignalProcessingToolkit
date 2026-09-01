#ifndef PEAKDETECT_H
#define PEAKDETECT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#ifndef TEST
#include <ffitt/core/real.h>
#else
#include "real.h"
#endif

// Finding the peaks of a signal, and finding out which of them are real.
//
// EVERY LOCAL MAXIMUM IS NOT A PEAK
//
// A sample larger than the two beside it is a local maximum. On a clean signal
// that is what a peak means. On a real one it is not: noise puts a local
// maximum every few samples, and a recording of a heart at 500 samples in a
// second holds about a hundred of them for every beat.
//
// peakdetect_get_peaks gives every one of them, which is what a caller wants
// when the signal is already clean, and what the emd module wants. On live
// data a caller needs to say which ones count, and peakdetect_find takes four
// rules for that.
//
// THE FOUR RULES, AND WHICH ONE MATTERS MOST
//
// HEIGHT is the obvious one and the weakest. It cannot tell a small peak
// standing alone from a small wobble on the side of a large one, and a signal
// whose level drifts defeats it entirely.
//
// PROMINENCE is the one to reach for. It asks: how far must you descend from
// this peak before you can climb to a higher one? A wobble on the side of a
// large peak has almost no prominence however high it stands, because you need
// only step down a little to reach the larger peak. A small peak standing
// alone in a valley has a large prominence.
//
// Prominence does not care where the signal sits, thus a drifting level does
// not defeat it. That is why it is the rule that works on real data.
//
// WIDTH throws away what is too narrow to be real. A spike one sample wide is
// noise; a heartbeat is thirty samples wide.
//
// DISTANCE says no two peaks may stand closer than so many samples. Where two
// do, the taller is kept. A heart cannot beat twice in 200 ms, thus a second
// peak inside that is the same beat counted twice.
//
// A VALLEY IS A PEAK OF THE SIGNAL TURNED UPSIDE DOWN. There is no separate
// set of these rules for valleys; negate the signal and use these.

// A flat top counts as one peak, and its index is the middle of the flat part.
//
// This matters on real data. A reading from a converter is a whole number of
// counts, thus the top of a peak is often two or three samples of exactly the
// same value. Treating each of them as no peak at all, which a test of
// "larger than both neighbours" does, loses the peak completely.

typedef struct{
    real_t minimum_height;      // A peak below this is not counted
    real_t minimum_prominence;  // A peak that stands out less is not counted
    real_t minimum_width;       // A peak narrower than this is not counted
    uint32_t minimum_distance;  // No two peaks may stand closer than this
}peakdetect_options_t;

// Give the rules with nothing switched on, so that a caller may set only the
// ones it wants.
peakdetect_options_t peakdetect_no_rules(void);

// Find every peak of the signal and give the number of them.
//
// A peak is a sample that is larger than the sample before it and larger than
// the sample after it. Thus the first sample and the last sample are never
// peaks, and a signal with fewer than three samples holds no peak.
//
// The function writes the index of each peak into index_buffer and the value
// of each peak into peak_buffer. Both buffers must hold room for as many
// values as the signal holds.
uint32_t peakdetect_get_peaks(real_t* input, real_t* index_buffer, real_t* peak_buffer, uint32_t size);

// How far the signal must descend from this peak before it can climb to a
// higher one, which is the prominence of the peak.
//
// Look from the peak outwards in both directions until the signal rises above
// the peak or the signal ends. The lowest point reached on each side is that
// side's base. The prominence is the height of the peak above the HIGHER of
// the two bases: the peak must clear that one to be worth calling a peak.
//
// Give 0 if the index is not a peak or lies outside the signal.
real_t peakdetect_prominence(const real_t* input, uint32_t size, uint32_t peak);

// How wide the peak is, measured at the given part of the way down from its
// top towards its base.
//
// A part of 0.5 measures at half the prominence below the top, which is the
// usual choice and is what "the width of a peak" ordinarily means. A part of 1
// measures at the base itself.
//
// The two edges are found by looking outwards until the signal falls below
// that level, and the place is taken between the two samples either side of
// the crossing, thus the width is not limited to whole samples.
//
// Give 0 if the index is not a peak or the part is outside 0 to 1.
real_t peakdetect_width(const real_t* input, uint32_t size, uint32_t peak,
                        real_t part);

// Where the top of a peak really stands, as an offset from the sample it was
// found at, between -0.5 and 0.5.
//
// A PEAK ALMOST NEVER STANDS ON A SAMPLE. What is measured is a smooth thing
// read at fixed places, and the largest of those places is merely the nearest
// one to the top. Taking that place as the answer rounds the top to the nearest
// sample, and the error does not go away with a longer run: it is there in
// every measurement equally.
//
// Three points fix one curve of the second order, and the top of that curve is
// where the top of a smooth thing stands. Add this offset to the index to get
// the place.
//
// HOW CLOSE IT COMES DEPENDS ON THE SHAPE, and the answer is exact only for a
// peak that really is of the second order. Measured against the shapes in
// curve.h, sampled five to a width, with the top moved through a hundred places
// between two samples and the worst of them taken. In samples:
//
//   shape                        refined     rounded to the nearest sample
//   -------------------------   --------     -----------------------------
//   gaussian                      0.0019                             0.5000
//   lorentzian                    0.0049                             0.5000
//   skewed gaussian, skew 2       0.0346                             0.5070
//   skewed gaussian, skew 4       0.1263                             0.5256
//   skewed gaussian, skew 8       0.3403                             0.5948
//
// READ THE TWO COLUMNS TOGETHER. Refining beats rounding on every shape, thus
// it is always worth doing, and HOW MUCH it beats it by falls away as the peak
// leans: by two hundred and sixty times on a gaussian, by four times at a skew
// of 4, and by only one and three quarter times at a skew of 8. A fitter tested
// only on a gaussian is being tested on the kindest shape there is.
//
// THE TABLE IS THE WORST ACROSS EVERY PLACE THE TOP CAN FALL, and not the error
// at each place on its own. Where the top happens to land almost exactly ON a
// sample, rounding to that sample is already right and refining can only add a
// little to it. That is not a fault and it is not worth guarding against: a
// caller cannot know which case it has, and the worst is what a measurement
// must be trusted to within.
//
// Give 0 where the peak stands at either end, because there are not three
// points there, and where the three points do not bend downwards, because then
// the middle one is not a peak at all.
real_t peakdetect_refine(const real_t* input, uint32_t size, uint32_t peak);

// How tall the peak really is, which the largest sample under-reports.
//
// The same curve that says where the top stands says how high it reaches, and
// it comes free with it. A SAMPLED PEAK IS ALWAYS SHORTER THAN THE REAL ONE,
// because the nearest sample stands to one side of the top and the shape has
// already begun to fall there. The shortfall is largest exactly when the top
// falls half way between two samples.
//
// Measured the same way as above, as a share of a top of one:
//
//   shape                        refined     the largest sample
//   -------------------------   --------     ------------------
//   gaussian                      0.0001                 0.0050
//   lorentzian                    0.0004                 0.0064
//   skewed gaussian, skew 2       0.0017                 0.0119
//   skewed gaussian, skew 4       0.0067                 0.0191
//   skewed gaussian, skew 8       0.0303                 0.0270
//
// Across most of that the gain is larger than it is for the place, because the
// curve is flattest exactly where the top is and a small error in the place
// costs almost nothing in the height.
//
// READ THE LAST ROW. AT A SKEW OF 8 THIS IS WORSE THAN DOING NOTHING. The
// largest sample always stands BELOW the real top; the fitted curve stands
// above it, and on a peak that leans that hard it overshoots by more than the
// sample undershoots. The place is still worth refining there and the height is
// not. Past about a skew of 4, take the largest sample as the height.
//
// Give the value at the peak itself where there are not three points to fit, or
// where they do not bend downwards.
real_t peakdetect_refine_height(const real_t* input, uint32_t size,
                                uint32_t peak);

// Find the peaks that pass every rule, and write their indices in the order
// they stand in the signal.
//
// The room says how many indices the list can hold. Where more peaks pass than
// there is room for, the ones that stand out most are kept.
//
// Give how many indices were written.
uint32_t peakdetect_find(const real_t* input, uint32_t size,
                         const peakdetect_options_t* options,
                         uint32_t* index_out, uint32_t room);

#endif//PEAKDETECT_H
