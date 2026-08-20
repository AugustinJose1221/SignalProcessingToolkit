#ifndef STATS_H
#define STATS_H

#include <stdint.h>
#ifndef TEST
#include <sptk/core/real.h>
#else
#include "real.h"
#endif

// Measures of a list of samples.
//
// Two kinds stand here, and the difference between them decides which one a
// piece of work needs.
//
// THE PLAIN MEASURES: mean, variance, deviation, root mean square, smallest
// and largest. Each one reads the list once and changes nothing. Each one also
// follows every sample, and that is their weakness: ONE bad sample moves them
// all. A knock on an electrode, a sample lost in a wire, a spike from a
// switching supply -- any of these pulls the mean and pulls the deviation far
// more, because the deviation squares the distance.
//
// THE ROBUST MEASURES: median, percentile, and the median absolute deviation.
// These follow the middle of the list and not its edges. Half of the samples
// may be wrong before the median moves at all. They cost more, because they
// must put the list in order, and they reorder the list that the caller gives.
//
// WHICH TO TAKE
//
// Take the plain ones when the samples are known to be sound, as inside a
// block that some other step has already cleaned.
//
// Take the robust ones to set a threshold from live data. This is the usual
// case and the plain ones are the usual mistake in it. A detector that puts
// its threshold at a few deviations above the mean is undone by the first
// spike: the spike raises the threshold that was meant to catch it, and the
// detector then sees nothing at all.
//
// The median absolute deviation answers that. For samples that follow a normal
// spread it estimates the same number as the deviation does, but a spike does
// not move it. Multiply it by STATS_MAD_TO_DEVIATION to get a number that
// stands beside a deviation.
//
// Every function gives 0 for an empty list.
//
// WHAT THE WIDTH OF real_t COSTS HERE
//
// Every sum in this module runs at the width of the build, and a sum is where
// the digits run out first. Five samples that sit at eight million and move by
// one have a variance of exactly 2. The variance already takes the mean away
// before it squares, which is the careful way, and even so:
//
//     32 bits    2.25      out by an eighth
//     64 bits    2.00      right
//
// The reason is the SUM, not the squaring: adding five samples near eight
// million gives a total near forty million, where one step of a float is 4.
//
// Thus a caller whose readings sit far from zero should either build in 64
// bits or take the level away first with the dcblock module. The tests hold
// both numbers, so that this cost is recorded and not forgotten.

// What the median absolute deviation must be multiplied by to estimate the
// standard deviation of samples that follow a normal spread.
//
// The number is 1/0.6745, because for a normal spread the median absolute
// deviation is 0.6745 of the deviation.
#define STATS_MAD_TO_DEVIATION      REAL_C(1.4826)

// Give the sum of the samples.
real_t stats_sum(const real_t* data, uint32_t size);

// Give the mean of the samples.
real_t stats_mean(const real_t* data, uint32_t size);

// Give the variance of the samples, divided by the number of samples.
//
// This is the variance of the list as it stands. To estimate the variance of
// the thing the list was drawn FROM, multiply by size/(size-1).
real_t stats_variance(const real_t* data, uint32_t size);

// Give the standard deviation, which is the root of the variance.
real_t stats_deviation(const real_t* data, uint32_t size);

// Give the root of the mean of the squares.
//
// This is not the deviation. The root mean square holds the mean inside it,
// thus for a signal that sits at 100 and wanders by 1 it gives about 100. The
// deviation gives 1. Take this one for the power of a signal and the other one
// for how much the signal moves.
real_t stats_rms(const real_t* data, uint32_t size);

// Give the smallest sample.
real_t stats_min(const real_t* data, uint32_t size);

// Give the largest sample.
real_t stats_max(const real_t* data, uint32_t size);

// Give the median of the samples.
//
// THIS FUNCTION REORDERS THE LIST. It has to put the samples in order to find
// the middle one, and it does that in the memory of the caller so that it
// needs none of its own. Copy the list first if the order matters.
//
// For a list of an even size the median lies between the two middle samples,
// and the function gives their mean.
real_t stats_median(real_t* data, uint32_t size);

// Give the sample below which the given part of the list stands. A part of 0.5
// gives the median, 0.25 the first quarter, 0.9 the ninth tenth.
//
// THIS FUNCTION REORDERS THE LIST, for the same reason as stats_median.
//
// Where the part falls between two samples, the answer lies between them in
// the same measure.
real_t stats_percentile(real_t* data, uint32_t size, real_t part);

// Give the median absolute deviation: the median of how far each sample stands
// from the median of the list.
//
// The work list must hold as many float values as the data list. The function
// writes into it and leaves the data list as it was, thus this one function
// does not reorder what the caller gave it.
real_t stats_mad(const real_t* data, uint32_t size, real_t* work);

#endif//STATS_H
