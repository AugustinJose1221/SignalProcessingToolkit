#ifndef MEDFILT_H
#define MEDFILT_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <ffitt/core/real.h>
#include <ffitt/core/ringbuf.h>
#else
#include "real.h"
#include "ringbuf.h"
#endif

// The median of the last samples.
//
// This filter gives, for each sample, the middle value of the window that ends
// there. It is the answer to a fault that no mean and no low pass can answer.
//
// WHAT IT IS FOR
//
// A sample can be wrong rather than noisy. A knock on an electrode, a sample
// lost on a wire, a spike from a switching supply: each puts one value in the
// signal that has nothing to do with the signal. Such a value is not small,
// and it does not average away.
//
// A mean SPREADS it. One sample that is a thousand times too large, in a
// window of 50, moves every one of the 50 answers that the window touches by
// twenty times the true signal. The fault goes in as one bad sample and comes
// out as fifty.
//
// A median REMOVES it. The middle of the window does not move at all while
// fewer than half of the samples are wrong. One spike in a window of 50 has no
// effect whatever, and the signal on both sides of it is untouched.
//
// WHAT IT COSTS
//
// A median is not a filter of frequency. It does not pass one band and stop
// another, and it cannot be described by a gain at each frequency, because it
// is not linear. Two signals filtered and added do not give the same answer as
// the two added and filtered.
//
// It also removes any peak that is narrower than half the window, whether that
// peak is a fault or not. THIS IS THE TRAP. A window long enough to take out a
// wide spike also takes out a real QRS, a real pulse, a real edge. Choose the
// window from the width of the fault and not from the width of the signal:
// long enough to cover the spike, and shorter than anything worth keeping.
//
// It costs one pass over the window for each sample, and no more. The filter
// holds the window in order, thus a new sample is put into its place and the
// old one taken out of its place, and neither needs a sort.
//
// WHERE IT SITS BESIDE THE OTHERS
//
//   movavg   the mean of the window        smooth, and spreads a bad sample
//   medfilt  the middle of the window      removes a bad sample, keeps an edge
//   savgol   a polynomial through the window   keeps the height of a peak
//   fir, iir a band of frequencies         the right tool when frequency is
//                                          what parts the signal from the noise
//
// A common chain is a median first to take the spikes out, then a filter of
// frequency to take the noise out. The other order does not work, because the
// filter of frequency spreads each spike before the median can reach it.
//
// AN ODD WINDOW IS BETTER
//
// A window of an odd size has a true middle sample. A window of an even size
// has two, and the filter gives their mean. That mean is no longer one of the
// samples, thus a little of the spreading of a mean comes back.

typedef struct{
    ringbuf_t window;           // The samples in the order they arrived
    real_t* sorted;              // The same samples, in order of value
    bool dynamic_alloc;         // True if the memory comes from the heap
}medfilt_t;

// Give a filter with a window of the given size. The memory comes from the
// heap. Give the filter to medfilt_free when you no longer need it.
medfilt_t medfilt_alloc(uint32_t size);

// Give a filter that uses the memory of the caller. Both lists must hold as
// many float values as the given size. This function takes no memory from the
// heap.
medfilt_t medfilt_static_alloc(uint32_t size, real_t* window, real_t* sorted);

// Forget every sample.
void medfilt_reset(medfilt_t* medfilt);

// Put one sample in and give the median of the window as it now stands.
//
// While the window is still filling, the median is taken over the samples that
// have arrived and not over the whole size.
real_t medfilt_process_sample(medfilt_t* medfilt, real_t sample);

// Filter a whole block. The input and the output may be the same list.
void medfilt_process_block(medfilt_t* medfilt, const real_t* input, real_t* output,
                           uint32_t size);

// Give the median of the window without putting a sample in.
real_t medfilt_get_median(medfilt_t* medfilt);

// Give the value below which the given part of the window stands. A part of
// 0.5 gives the median, 0.25 the first quarter.
//
// The window is already held in order, thus this costs nothing more than the
// median does. It suits a caller that watches how far a signal spreads as well
// as where its middle stands.
real_t medfilt_get_percentile(medfilt_t* medfilt, real_t part);

// Give how many samples the window holds now.
uint32_t medfilt_count(const medfilt_t* medfilt);

// True when the window holds as many samples as its size.
bool medfilt_is_full(const medfilt_t* medfilt);

// Release the memory of a filter that came from medfilt_alloc. This function
// does nothing for a filter that came from medfilt_static_alloc.
void medfilt_free(medfilt_t* medfilt);

#endif//MEDFILT_H
