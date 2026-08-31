#ifndef HAMPEL_H
#define HAMPEL_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <ffitt/core/real.h>
#include <ffitt/core/ringbuf.h>
#include <ffitt/filter/medfilt.h>
#else
#include "real.h"
#include "ringbuf.h"
#include "medfilt.h"
#endif

// Find the samples that are wrong and replace only those.
//
// WHAT IT IS FOR, AND WHY A MEDIAN ALONE IS NOT ENOUGH
//
// A median filter removes a spike, and it also changes every other sample it
// touches. Give it a clean signal and it gives back a different clean signal:
// every peak narrower than half its window is gone, and the rest is flattened
// a little. That is a heavy price to pay for a fault that happens once a
// second.
//
// This filter does not touch a sample unless it has a reason to. For each
// sample it asks one question: how far does this sample stand from the middle
// of its neighbours, measured against how far they usually stand from it? A
// sample that stands far outside is replaced by the middle. Every other sample
// is passed through EXACTLY as it arrived.
//
// Thus a recording with three bad samples in a minute comes back with three
// samples changed, and the heart, the peak or the edge in it is untouched.
//
// HOW FAR IS TOO FAR
//
// The measure of how far the neighbours usually stand is the median absolute
// deviation, not the standard deviation, and that choice is the whole of why
// this works.
//
// A deviation is moved by the very samples it is meant to catch. One spike
// raises it, the threshold rises with it, and the spike then sits inside the
// threshold that was put there to catch it. A detector built that way lets
// through exactly the faults it was built for, and the worse the fault the
// more surely it is missed.
//
// The median absolute deviation does not move for a spike. Half the window
// would have to be wrong before it moved at all.
//
// HAMPEL_THRESHOLD is how many deviations count as too far. Three is the usual
// choice: for samples that follow a normal spread it leaves about 997 in 1000
// alone. A lower number replaces more and risks flattening the signal; a
// higher number replaces less and lets small faults through.
//
// THE WINDOW LOOKS BOTH WAYS, THUS THE ANSWER COMES LATE
//
// A sample can only be judged against its neighbours on BOTH sides. The filter
// therefore holds the newest samples back until it has seen enough of what
// comes after them, and hampel_delay says how many. For a window of 2*half+1
// the answer for a sample arrives half samples later.
//
// A caller that reads a signal as it arrives must allow for that delay. A
// caller that has the whole signal in hand should use hampel_process_block,
// which puts the delay right and gives an output as long as its input.

// How many deviations away a sample must stand before it is called wrong.
#define HAMPEL_THRESHOLD    REAL_C(3.0)

// What the median absolute deviation must be multiplied by to stand beside a
// standard deviation, for samples that follow a normal spread.
#define HAMPEL_SCALE        REAL_C(1.4826)

typedef struct{
    medfilt_t middle;           // The window, held in order
    ringbuf_t history;          // The same samples in the order they arrived
    real_t* distance;           // Working room, one for each sample of the window
    real_t threshold;           // How many deviations count as too far
    uint32_t replaced;          // How many samples have been replaced
    uint32_t seen;              // How many samples have arrived
    bool dynamic_alloc;         // True if the memory comes from the heap
}hampel_t;

// True if a window of this size can be used. It must be odd and at least 3: an
// odd window has a true middle, and a window of one has no neighbours to judge
// a sample against.
bool hampel_is_valid_window(uint32_t window);

// Give a filter with a window of the given size. The memory comes from the
// heap. Give the filter to hampel_free when you no longer need it.
hampel_t hampel_alloc(uint32_t window);

// Give a filter that uses the memory of the caller. Each of the four lists
// must hold as many values as the window. This function takes no memory from
// the heap.
hampel_t hampel_static_alloc(uint32_t window, real_t* sorted, real_t* ordered,
                             real_t* history, real_t* distance);

// Set how many deviations away a sample must stand before it is replaced.
// Give false if the threshold is not above zero.
bool hampel_set_threshold(hampel_t* hampel, real_t threshold);

// Forget every sample and every count.
void hampel_reset(hampel_t* hampel);

// How many samples the answer comes behind the input, which is half the
// window.
uint32_t hampel_delay(const hampel_t* hampel);

// Put one sample in and give the answer for the sample that arrived
// hampel_delay samples ago.
//
// While the window is still filling the filter gives back the samples as they
// came, because it cannot yet judge them. Set was_replaced to say whether the
// sample it gives back was changed; give NULL if that is not wanted.
real_t hampel_process_sample(hampel_t* hampel, real_t sample,
                             bool* was_replaced);

// Clean a whole signal. The output holds as many samples as the input, and the
// two may be the same list.
//
// This puts the delay right, thus output[k] is the answer for input[k]. The
// samples at the two ends have no neighbours on one side; they are passed
// through as they arrived rather than judged against a window that is not
// there.
//
// Give how many samples were replaced.
uint32_t hampel_process_block(hampel_t* hampel, const real_t* input,
                              real_t* output, uint32_t size);

// How many samples this filter has replaced since it was last reset.
//
// Read this. It is the measure of how much was wrong with the signal, and a
// number that climbs is a fault in the wiring and not in the filter. A
// recording where one sample in fifty is replaced is a recording to look at
// rather than to trust.
uint32_t hampel_replaced_count(const hampel_t* hampel);

// Release the memory of a filter that came from hampel_alloc. This function
// does nothing for one that came from hampel_static_alloc.
void hampel_free(hampel_t* hampel);

#endif//HAMPEL_H
