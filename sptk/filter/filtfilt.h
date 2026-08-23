#ifndef FILTFILT_H
#define FILTFILT_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#include <sptk/filter/iir.h>
#include <sptk/filter/fir.h>
#else
#include "real.h"
#include "iir.h"
#include "fir.h"
#endif

// Filtering with no delay at all, by running the filter both ways.
//
// WHAT PROBLEM THIS SOLVES
//
// Every filter delays what it passes, and one with feedback delays each
// frequency by a DIFFERENT amount. That is why the shape of a signal changes
// after filtering even when nothing was taken out of the band it lives in: the
// parts of it arrive at slightly different times and no longer line up.
//
// For a measurement that matters this is worse than the noise. The peak of a
// heartbeat moves. The edge of a step leans. The two ends of a pulse spread by
// different amounts and it is no longer the same width.
//
// Run the filter forwards, turn the answer round, and run it through again.
// The second pass delays every frequency by exactly what the first pass did,
// and in the opposite direction, thus the two cancel EXACTLY. What comes out
// lines up with what went in, sample for sample.
//
// THE TWO PRICES, AND BOTH MUST BE PAID KNOWINGLY
//
// THE WHOLE SIGNAL MUST BE IN HAND. There is no way to run a filter backwards
// over a signal that has not arrived yet. This cannot be used on a signal as
// it comes in, at any delay. It is for a recording.
//
// THE FILTER IS APPLIED TWICE, thus its gain is SQUARED. A cutoff is where a
// filter passes 0.707 of what arrives; run twice it passes 0.5 there, which is
// a different cutoff. The band is narrower than the one that was designed and
// the edges are twice as steep.
//
// Design for that. filtfilt_gain gives what a filter really does at a
// frequency when it is run both ways, so that a caller need not guess.
//
// WHAT IT NEEDS BY WAY OF MEMORY: NOTHING
//
// The filtering is done in the caller's own output list, one pass forwards and
// one backwards over the same memory. The input and the output may even be the
// same list.
//
// THE TWO ENDS
//
// A filter that starts from nothing answers the first sample as a step, and
// that answer can stand above the signal for a long time. Running both ways
// would then put a false swing at BOTH ends.
//
// Two things are done about that, and both are needed.
//
// The filter is FIRST SETTLED at the value it is about to meet, by feeding it
// that value until its answer stops moving. That says: assume the signal stood
// here for ever before now. Carrying the signal outwards alone is not enough,
// because a filter with a low cutoff takes thousands of samples to answer a
// step and the carried part is a few dozen.
//
// The signal is THEN CARRIED OUTWARDS past each end, turned about the end
// sample: the sample before the first stands as far above the first as the
// second stands below it. The signal then begins with no step and no corner.
//
// Nothing is stored for either, because the carried samples are worked out
// from the ones already in hand.

// How many samples are carried past each end, for a filter of the given size.
//
// Three times the length of the filter is enough for the answer to have
// settled. Where the signal is shorter than that, as much as there is is used.
uint32_t filtfilt_padding(uint32_t filter_size, uint32_t size);

// Give what a filter really does at a frequency when it is run both ways,
// which is the square of what it does in one pass.
//
// Use this rather than iir_get_gain when designing for filtfilt. At the cutoff
// a filter passes 0.707; run both ways it passes 0.5.
real_t filtfilt_iir_gain(iir_t* iir, real_t frequency);

// The same for a filter with a finite impulse response.
real_t filtfilt_fir_gain(fir_t* fir, real_t frequency);

// Filter a whole signal both ways with a filter that has feedback.
//
// The output holds as many samples as the input, and output[k] lines up with
// input[k] with no delay between them. The input and the output may be the
// same list.
//
// The state of the filter is used and changed. Its coefficients are not.
//
// Give false if the signal is too short to filter, which is when it holds
// fewer samples than the filter has state.
bool filtfilt_iir(iir_t* iir, const real_t* input, real_t* output,
                  uint32_t size);

// Filter a whole signal both ways with a filter that has no feedback.
//
// A filter with a middle coefficient already delays every frequency by the
// same time, thus running it both ways is not needed to keep the shape. It is
// still useful to make the edges of the band twice as steep without a longer
// filter, and to leave no delay at all to correct for.
//
// Give false if the signal holds fewer samples than the filter is long.
bool filtfilt_fir(fir_t* fir, const real_t* input, real_t* output,
                  uint32_t size);

#endif//FILTFILT_H
