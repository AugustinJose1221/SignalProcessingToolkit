#ifndef PEAKDETECT_H
#define PEAKDETECT_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#ifndef TEST
#include <sptk/core/real.h>
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
