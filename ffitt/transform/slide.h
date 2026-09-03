#ifndef SLIDE_H
#define SLIDE_H

#include <stdint.h>
#include <stdbool.h>
#ifndef TEST
#include <ffitt/core/real.h>
#include <ffitt/core/ringbuf.h>
#include <ffitt/linalg/cnum.h>
#else
#include "real.h"
#include "ringbuf.h"
#include "cnum.h"
#endif

// A transform that answers at EVERY sample, for a few frequencies only.
//
// The transform beside this one needs the whole block in memory and answers
// once the block is full. goertzel needs no block but answers once per block
// as well, and it must be reset between them. Between the two sits the case
// that neither serves: a program that must know, AT EVERY SAMPLE, how much of
// two or three known frequencies the last N samples held.
//
// That is what this does. It holds one running total for each frequency
// watched, and each new sample costs one complex multiplication and two
// additions per frequency, whatever N is. A watcher on a 1024 sample window
// holds a few dozen bytes for each frequency, and not the 1024 samples' worth
// that a block transform must keep to answer at all.
//
// HOW IT WORKS. The total for one bin, when the window slides one sample on,
// differs from the total before it by the sample that arrived and the sample
// that fell off the end, the whole then turned by one bin's worth of angle:
//
//     X[n] = (X[n-1] + arrived - left) * turn
//
// Nothing about that grows with N. The window itself must still be kept, so
// that the sample which left can be subtracted, and that is what the ringbuf
// inside holds.
//
// THE RECURRENCE IS ONLY JUST STABLE, AND THIS IS THE THING TO KNOW ABOUT IT.
//
// The turning factor has a magnitude of exactly one, thus nothing in the
// arithmetic ever shrinks: every rounding error that goes in stays in and goes
// round for ever. The mending is to make the factor a shade smaller than one,
// which is what the damping is. An error then fades instead of circling.
//
// WHAT THE DAMPING COSTS. It is not free: a total that is always shrinking a
// little reads low, by a fixed amount that does not go away. MEASURED against
// the transform of the same window, for a steady tone sitting on the bin, and
// the same at both widths:
//
//   damping     the answer reads low by     an error fades to a tenth in
//   1.0                            0 %      never
//   0.9999                      1.27 %      about 23000 samples
//   0.999                      11.78 %      about 2300 samples
//   0.99                       64.10 %      about 230 samples
//
// WHAT SWITCHING IT OFF COSTS, which is the other half and the surprising
// half. MEASURED on a steady tone, against the transform of the same window,
// with the damping at exactly one:
//
//   width      worst drift over 20 million samples   past 1 part in 1000 at
//   32 bit                              1.0e-03            54000 samples
//   64 bit                              4.0e-10            never reached
//
// READ THOSE TWO TABLES TOGETHER BEFORE CHOOSING.
//
//   AT 32 BITS the damping earns its keep. Without it a watcher must be reset
//   every few tens of thousands of samples, which at 8 kHz is every few
//   seconds, and a reset costs a whole window of filling before the answer
//   means anything again.
//
//   AT 64 BITS IT LARGELY DOES NOT. The plain recurrence drifted by four parts
//   in ten thousand million over twenty million samples, which is below
//   anything a watcher would act on. A caller at that width who wants the
//   answer to READ TRUE - one comparing against a threshold in the unit of the
//   signal, rather than watching for a change - should give a damping of 1.0
//   and take the exactness.
//
// The default is 0.9999, because the library is built for a float unless told
// otherwise, and at that width the drift is real.
//
// WHAT THIS IS NOT. It gives the bins it was told to watch and no others. A
// caller who wants the whole spectrum wants fft, which does all N bins for
// less than N of these. The crossover is the same one goertzel names: past
// about log2(N) frequencies the whole transform is cheaper.

// The bins are of a transform of this many samples, thus the size decides both
// the window and where the bins fall. It must be at least two.
bool slide_is_valid_size(uint32_t size);

// True if the damping is one this module will take. It must be above nothing
// and not above one. A damping of exactly one switches the mending off, and
// the header above says what that costs.
bool slide_is_valid_damping(real_t damping);

// The damping a watcher gets unless it asks for another.
#define SLIDE_DAMPING       REAL_C(0.9999)

// How many values the memory of a watcher must hold, for the caller who gives
// that memory rather than taking it from the heap. The window is real values;
// the totals and the turning factors are complex.
#define SLIDE_WINDOW_COUNT(size)        (size)
#define SLIDE_BIN_COUNT(count)          (count)
#define SLIDE_TURN_COUNT(count)         (count)

typedef struct{
    ringbuf_t history;          // The window, so that what left can be taken
    cnum_t* total;              // One running total for each frequency watched
    cnum_t* turn;               // The turning factor of each, damping included
    real_t departing;           // What the sample that left is multiplied by
    real_t damping;             // A shade below one, which keeps it stable
    uint32_t size;              // The window, which is the size of the bins
    uint32_t count;             // How many frequencies are watched
    uint32_t seen;              // How many samples have arrived
    bool dynamic_alloc;         // True if the memory comes from the heap
}slide_t;

// Give a watcher of the given number of frequencies over a window of the given
// size. The memory comes from the heap. Give it to slide_free.
//
// Nothing is watched until slide_watch says which bins, thus a watcher fresh
// from here answers nothing.
slide_t slide_alloc(uint32_t size, uint32_t count);

// Give a watcher that uses the memory the caller holds. The window must hold
// SLIDE_WINDOW_COUNT values, and the two complex lists SLIDE_BIN_COUNT and
// SLIDE_TURN_COUNT. This function takes no memory from the heap.
slide_t slide_static_alloc(uint32_t size, uint32_t count, real_t* window,
                           cnum_t* total, cnum_t* turn);

// Say how quickly an error fades. Give SLIDE_DAMPING unless the header above
// gives a reason to give another. This forgets every total, because a change
// of damping changes what the totals mean.
//
// Give false if the damping is one slide_is_valid_damping refuses.
bool slide_design(slide_t* slide, real_t damping);

// Watch the given bin of the transform with the given watcher.
//
// The bin runs from 0 to the size, and slide_bin_frequency says what frequency
// each one stands at. A bin above half the size is the mirror of one below it
// and says nothing new about a real signal.
//
// Give false if either number is outside what this watcher holds.
bool slide_watch(slide_t* slide, uint32_t index, uint32_t bin);

// The frequency that a bin stands at, in the same unit as the sample rate.
real_t slide_bin_frequency(const slide_t* slide, uint32_t bin,
                           real_t sample_rate);

// Forget every sample and every total. What is watched is kept.
void slide_reset(slide_t* slide);

// Give one sample to the watcher. Every total moves.
void slide_process_sample(slide_t* slide, real_t sample);

// Give a whole block, one sample at a time. The totals afterwards are those of
// the last sample of the block.
void slide_process_block(slide_t* slide, const real_t* input, uint32_t count);

// True once as many samples have arrived as the window holds.
//
// READ THIS BEFORE THE ANSWER. Until the window is full the totals are of a
// window that is partly nothing, thus they are low, and how low depends on how
// far through the filling they are. They are not wrong so much as not yet
// about anything.
bool slide_is_full(const slide_t* slide);

// The running total of one watcher, as a complex number.
cnum_t slide_get(const slide_t* slide, uint32_t index);

// How large the wave at one watcher's bin is, which is the size of its total.
real_t slide_magnitude(const slide_t* slide, uint32_t index);

// Release the memory of a watcher that came from slide_alloc. This does
// nothing for one that came from slide_static_alloc, thus a call for either
// kind is safe.
void slide_free(slide_t* slide);

#endif//SLIDE_H
