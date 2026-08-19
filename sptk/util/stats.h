#ifndef STATS_H
#define STATS_H

#include <stdint.h>

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
// Every sum inside this module runs in double, although the samples and the
// answers are float. A float holds about seven digits, and a long sum of large
// samples loses the low ones: five samples that sit at eight million and move
// by one have a variance of exactly 2, and adding them in float gives 2.25.
// The double costs nothing worth counting and takes that error away.

// What the median absolute deviation must be multiplied by to estimate the
// standard deviation of samples that follow a normal spread.
//
// The number is 1/0.6745, because for a normal spread the median absolute
// deviation is 0.6745 of the deviation.
#define STATS_MAD_TO_DEVIATION      1.4826f

// Give the sum of the samples.
float stats_sum(const float* data, uint32_t size);

// Give the mean of the samples.
float stats_mean(const float* data, uint32_t size);

// Give the variance of the samples, divided by the number of samples.
//
// This is the variance of the list as it stands. To estimate the variance of
// the thing the list was drawn FROM, multiply by size/(size-1).
float stats_variance(const float* data, uint32_t size);

// Give the standard deviation, which is the root of the variance.
float stats_deviation(const float* data, uint32_t size);

// Give the root of the mean of the squares.
//
// This is not the deviation. The root mean square holds the mean inside it,
// thus for a signal that sits at 100 and wanders by 1 it gives about 100. The
// deviation gives 1. Take this one for the power of a signal and the other one
// for how much the signal moves.
float stats_rms(const float* data, uint32_t size);

// Give the smallest sample.
float stats_min(const float* data, uint32_t size);

// Give the largest sample.
float stats_max(const float* data, uint32_t size);

// Give the median of the samples.
//
// THIS FUNCTION REORDERS THE LIST. It has to put the samples in order to find
// the middle one, and it does that in the memory of the caller so that it
// needs none of its own. Copy the list first if the order matters.
//
// For a list of an even size the median lies between the two middle samples,
// and the function gives their mean.
float stats_median(float* data, uint32_t size);

// Give the sample below which the given part of the list stands. A part of 0.5
// gives the median, 0.25 the first quarter, 0.9 the ninth tenth.
//
// THIS FUNCTION REORDERS THE LIST, for the same reason as stats_median.
//
// Where the part falls between two samples, the answer lies between them in
// the same measure.
float stats_percentile(float* data, uint32_t size, float part);

// Give the median absolute deviation: the median of how far each sample stands
// from the median of the list.
//
// The work list must hold as many float values as the data list. The function
// writes into it and leaves the data list as it was, thus this one function
// does not reorder what the caller gave it.
float stats_mad(const float* data, uint32_t size, float* work);

#endif//STATS_H
