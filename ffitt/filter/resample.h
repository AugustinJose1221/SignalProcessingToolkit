#ifndef RESAMPLE_H
#define RESAMPLE_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <ffitt/core/real.h>
#include <ffitt/core/ringbuf.h>
#include <ffitt/filter/fir.h>
#else
#include "real.h"
#include "ringbuf.h"
#include "fir.h"
#endif

// Changing the rate at which a signal is sampled.
//
// WHY THIS IS A MODULE AND NOT A LINE OF CODE
//
// Keeping every fourth sample looks like the whole of it, and it is the half
// that goes wrong. A signal sampled at 32 kHz may hold frequencies up to
// 16 kHz. Keep every 64th sample and the new rate is 500 Hz, which can hold
// nothing above 250. Every frequency above 250 does not disappear: it comes
// back somewhere else, at a frequency it never had, and once it is there
// NOTHING can take it out again, because it now sits on top of the signal and
// looks exactly like part of it.
//
// A hum at 4 kHz decimated by 64 arrives at 0 Hz and looks like a drift. A
// noise at 300 Hz arrives at 200 Hz and looks like a signal. The reading looks
// perfectly reasonable and is wrong, and no later step can find out.
//
// The answer is a filter BEFORE the samples are thrown away, and this module
// puts the two together so that they cannot be separated by accident.
//
// GOING UP HAS THE MIRROR OF THE SAME PROBLEM. Putting zeros between the
// samples leaves copies of the signal at every multiple of the old rate. The
// filter after them takes the copies away, and without it the answer holds
// tones that were never in the signal.
//
// THE TWO THINGS THIS MODULE DOES
//
//   resample_decimate     one sample kept for each factor, filter first
//   resample_interpolate  factor samples made for each one, filter after
//
// A rate that changes by a ratio rather than by a whole number, such as 44100
// to 48000, is an interpolator by 160 followed by a decimator by 147. Build
// the two and put the output of the first into the second. This module gives
// no single function for that, because the two filters can be joined into one
// only when both factors are known at the time the filter is designed, and
// then the joined filter is what the caller wants and not this module.
//
// WHAT IT COSTS, AND THE ONE THING THAT MAKES IT CHEAP
//
// A filter that runs at the high rate and then throws most of its answers away
// is doing work for nothing. Going down by 64, sixty-three of every sixty-four
// answers are never used.
//
// This module works out only the answers it keeps. The filtering costs the
// same as the OUTPUT rate and not the input rate, thus decimating by 64 with a
// filter of 128 coefficients costs 2 multiplications for each input sample and
// not 128. That is the whole reason a long filter is affordable here.
//
// HOW LONG A FILTER
//
// The filter must pass what is wanted and stop everything above half the new
// rate. Those two edges lie close together when the factor is large, and a
// close pair of edges needs a long filter: the width of the turn is about
// 4/length of the rate it runs at. resample_advised_length gives a length that
// works, and a caller who wants a sharper edge gives a longer one.
//
// A LARGE FACTOR IS BETTER DONE IN STAGES. Going from 32 kHz to 500 Hz in one
// step needs a filter of about 2000 coefficients. Doing it as 8 then 8 needs
// two filters of about 40, and the two together cost far less than the one.

// How many coefficients a filter needs for a given factor, as a rule of thumb.
//
// This gives a turn of about a fifth of the new rate, and a stop band about
// 60 dB down. It is a starting point and not a law: a caller who needs a
// sharper edge gives a longer filter, and one who can accept a softer edge
// saves work with a shorter one.
//
// THE 60 dB IS THE STOP BAND AND NOT THE EDGE OF IT. A frequency just above
// half the new rate does not sit in the stop band at all: it sits on the turn,
// where the filter is still on its way down. Measured, swept finely right up
// to the edge:
//
//     factor      worst rejection near the edge
//        2               52.7 dB
//        3               53.2 dB
//        4               53.0 dB
//        5               53.7 dB
//        8               53.9 dB
//
// About 53 dB for every factor, and better everywhere further in. A caller who
// needs the full 60 dB right at the edge gives a longer filter.
uint32_t resample_advised_length(uint32_t factor);

typedef struct{
    fir_t filter;               // The filter that keeps the aliases out
    ringbuf_t history;          // The last samples at the input rate
    uint32_t factor;            // How many samples in for each one out
    uint32_t phase;             // Where the next output falls
    bool dynamic_alloc;         // True if the memory comes from the heap
}resample_t;

// True if this factor can be used. It must be 2 or more; a factor of 1 changes
// nothing and a factor of 0 means nothing.
bool resample_is_valid_factor(uint32_t factor);

// How many values the memory of a decimator must hold, for the caller who
// gives that memory rather than taking it from the heap.
//
// The filter keeps its coefficients and its history, which is the length
// twice, and the resampler keeps the samples at the input rate, which is the
// length once more.
#define RESAMPLE_DECIMATOR_MEMPOOL_SIZE(length)         (3u * (length))

// How many values the memory of an interpolator must hold.
//
// The filter keeps the same two lists. The history is shorter here: one input
// sample feeds every factor-th coefficient, thus only the length divided by
// the factor, rounded up, is ever read.
#define RESAMPLE_INTERPOLATOR_MEMPOOL_SIZE(factor, length) \
    ((2u * (length)) + ((((length) + (factor)) - 1u) / (factor)))

// Give a decimator that keeps one sample for each factor, with a filter of the
// given length. The memory comes from the heap.
//
// Give resample_advised_length(factor) for the length unless there is a reason
// to give another. The length must be odd, so that the filter has a middle and
// delays every frequency by the same time.
resample_t resample_alloc_decimator(uint32_t factor, uint32_t length);

// Give an interpolator that makes factor samples for each one, with a filter
// of the given length. The memory comes from the heap.
resample_t resample_alloc_interpolator(uint32_t factor, uint32_t length);

// Give a decimator that uses the memory the caller holds, which must hold as
// many values as RESAMPLE_DECIMATOR_MEMPOOL_SIZE gives for the same length.
// This function takes no memory from the heap and cannot fail for want of it.
//
// THE COEFFICIENTS ARE STILL WORKED OUT HERE, thus this call does the same
// arithmetic as the one that takes memory from the heap. It is the memory that
// the caller has taken over and not the design.
//
// A decimator built this way is given to resample_free like any other, and
// that call then does nothing, thus one road serves both kinds.
resample_t resample_static_alloc_decimator(uint32_t factor, uint32_t length,
                                           real_t* mempool);

// Give an interpolator that uses the memory the caller holds, which must hold
// as many values as RESAMPLE_INTERPOLATOR_MEMPOOL_SIZE gives for the same
// factor and length. This function takes no memory from the heap.
resample_t resample_static_alloc_interpolator(uint32_t factor, uint32_t length,
                                              real_t* mempool);

// Forget every sample. The filter keeps its coefficients.
void resample_reset(resample_t* resample);

// Put one sample into a decimator.
//
// Give true when an output sample is ready, and write it into output. That
// happens once for each factor samples put in.
bool resample_decimate(resample_t* resample, real_t sample, real_t* output);

// Put one sample into an interpolator and write the factor samples that come
// out of it.
//
// The output must hold as many values as the factor. Give how many were
// written, which is always the factor.
uint32_t resample_interpolate(resample_t* resample, real_t sample,
                              real_t* output);

// Run a whole block through a decimator. The output must have room for
// size/factor samples. Give how many were written.
uint32_t resample_decimate_block(resample_t* resample, const real_t* input,
                                 real_t* output, uint32_t size);

// Run a whole block through an interpolator. The output must have room for
// size*factor samples. Give how many were written.
uint32_t resample_interpolate_block(resample_t* resample, const real_t* input,
                                    real_t* output, uint32_t size);

// How many samples the answer comes behind the input, counted at the OUTPUT
// rate.
//
// A filter with a middle delays every frequency by half its length. For a
// decimator that is half the length divided by the factor, because the output
// samples are further apart.
uint32_t resample_delay(const resample_t* resample);

// Release the memory of a resampler that came from one of the alloc functions.
void resample_free(resample_t* resample);

#endif//RESAMPLE_H
