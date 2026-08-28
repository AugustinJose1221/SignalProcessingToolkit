#ifndef FARROW_H
#define FARROW_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#include <sptk/core/ringbuf.h>
#else
#include "real.h"
#include "ringbuf.h"
#endif

// Delay a signal by a part of a sample.
//
// delay_by_phase measures how far one reading stands behind another to below a
// sample. THIS IS THE OTHER HALF OF THAT: having measured a delay of 2.35
// samples, something has to be able to apply it. Lining two readings up, steering
// an array of microphones, resampling by a ratio that is not a whole number, and
// following a clock that drifts all come down to the same thing.
//
// A DELAY OF A WHOLE NUMBER OF SAMPLES COSTS NOTHING: it is reading from further
// back in a buffer, and ringbuf already does it. A delay of a PART of a sample
// is a filter, because the value between two samples is not in the reading and
// has to be worked out from the samples around it.
//
// WHAT IT CANNOT DO, AND NOTHING CAN. The exact answer needs a filter of
// infinite length. Every real one is a guess made from a handful of samples: it
// is right where the signal changes slowly and wrong where it changes fast.
//
// IT GOES WRONG IN TWO WAYS, AND THE ONE THAT MATTERS IS NOT THE OBVIOUS ONE.
//
// The obvious one is that the delay comes out a little different from the delay
// asked for. Measured at a quarter of a sample past the halfway point, in
// samples:
//
//   part of the rate     order 1    order 3    order 5    order 7
//   ----------------    --------   --------   --------   --------
//   0.05                  0.0016     0.0000     0.0000     0.0000
//   0.10                  0.0063     0.0005     0.0000     0.0000
//   0.20                  0.0273     0.0076     0.0023     0.0007
//   0.30                  0.0698     0.0358     0.0201     0.0118
//   0.40                  0.1457     0.1110     0.0885     0.0724
//
// THE ONE THAT MATTERS IS THAT IT QUIETENS THE SIGNAL. Working out a value
// between two samples averages them, and averaging takes the fast part of a
// signal away. How much of the signal is left, at the delay halfway between two
// samples, which is the worst place there is:
//
//   part of the rate     order 1    order 3    order 5    order 7
//   ----------------    --------   --------   --------   --------
//   0.05                  0.9877     0.9998     1.0000     1.0000
//   0.10                  0.9511     0.9965     0.9997     1.0000
//   0.20                  0.8090     0.9488     0.9850     0.9955
//   0.30                  0.5878     0.7801     0.8746     0.9261
//   0.40                  0.3090     0.4488     0.5436     0.6150
//
// SET THE TWO TABLES BESIDE EACH OTHER. At four tenths of the rate an order of 1
// puts the delay out by a seventh of a sample, which sounds tolerable, and
// throws away SEVEN TENTHS OF THE SIGNAL, which is not. A caller watching only
// the delay would call that filter good.
//
// THE RULE THAT FOLLOWS: keep the signal well below half the sample rate, and
// choose the order by the second table and not the first. Below a fifth of the
// rate an order of 3 keeps 95 in every hundred and puts the delay out by less
// than a hundredth of a sample. Above three tenths no order here is worth much,
// and the answer is to sample faster rather than to interpolate harder.
//
// AT A DELAY OF A WHOLE NUMBER OF SAMPLES THERE IS NO ERROR AT ALL, in either
// table, because there is nothing between samples to work out. The tables give
// the worst place, which is halfway between two of them.
//
// AND A HIGH ORDER COSTS SOMETHING AT 32 BITS THAT IT DOES NOT COST AT 64.
//
// The weights are worked out from products and divisions that grow quickly with
// the order: at an order of 8 the divisor reaches 40320 and the numbers that go
// into it are far larger still. The weights should add up to exactly one at
// every delay, because a signal that is not changing must come through
// unchanged, and how far they miss by is the level error below. Measured as a
// share of the signal, at the worst of a hundred delays across the range:
//
//   order      32 bits      64 bits
//   -----     --------     --------
//   1 to 2           0            0
//   3          1.4e-07      2.5e-16
//   4          9.5e-07      1.8e-15
//   5          3.6e-05      6.8e-14
//   6          1.4e-05      8.9e-14
//   7          3.0e-04      5.8e-13
//   8          1.6e-03      4.1e-12
//
// At 64 bits none of it matters. At 32 bits an order of 8 puts a level error of
// a part in six hundred into a signal that has none, and a chain of such filters
// adds it up. THE ORDER IS NOT CAPPED BY THE WIDTH, because a part in six
// hundred is nothing to many callers and everything to a few; the table is here
// so that the few can see it.
//
// The table is taken across a HUNDRED delays and not a handful. Taken across
// twenty it read 5.4e-04 at an order of 8, which is a third of what a hundred
// found. THE ERROR IS NOT SMOOTH IN THE DELAY, thus even a hundred is a scale
// and not a bound: delays chosen at random have since found four times the
// figure above at an order of 6. Read the table as the size of the thing and
// leave room above it.
//
// THE FILTER ADDS A WHOLE DELAY OF ITS OWN, and it cannot not. It works out the
// value between samples from the samples either side, thus it must have them,
// thus it must wait. The delay it applies therefore runs from half the order to
// half the order plus one, and farrow_smallest_delay and farrow_largest_delay
// give those. FOR A LARGER DELAY, take the whole samples with a ringbuf and
// leave the part to this. That is the cheap way round and the only one.

typedef struct{
    ringbuf_t history;          // The samples the answer is worked out from
    real_t* weight;             // The Farrow matrix, (order+1) by (order+1)
    real_t* working;            // One running total for each power of the part
    uint32_t order;             // The order of the curve laid through the samples
    real_t delay;               // The delay asked for, in samples
    bool dynamic_alloc;         // True if the memory comes from the heap
}farrow_t;

#ifndef FARROW_LARGEST_ORDER
#define FARROW_LARGEST_ORDER    8u
#endif

// How many samples a filter of this order works from, and how many values each
// of its two lists must hold.
#define FARROW_TAP_COUNT(order)     ((order) + 1u)
#define FARROW_WEIGHT_COUNT(order)  (FARROW_TAP_COUNT(order) \
                                     * FARROW_TAP_COUNT(order))

// True if this is an order the filter can be built at.
//
// An order of nothing would take one sample and give it back, which is no delay
// at all. The bound above is where the curve laid through the samples begins to
// swing between them more than it follows them.
bool farrow_is_valid_order(uint32_t order);

// The smallest delay a filter of this order can apply, in samples, which is
// half its order. The reason is in the header above.
real_t farrow_smallest_delay(uint32_t order);

// The largest, which stands one sample past the smallest. For more than that,
// take the whole samples with a ringbuf and leave the part to this.
real_t farrow_largest_delay(uint32_t order);

// True if this filter can apply this delay.
bool farrow_is_valid_delay(const farrow_t* farrow, real_t delay);

// Give a filter of the given order. The memory comes from the heap. Give it to
// farrow_free when you no longer need it.
//
// It starts at the smallest delay it can apply, which is half its order.
farrow_t farrow_alloc(uint32_t order);

// Give a filter that uses the memory the caller holds. The history must hold
// FARROW_TAP_COUNT(order) values, the weights FARROW_WEIGHT_COUNT(order), and
// the working room FARROW_TAP_COUNT(order). This takes nothing from the heap.
farrow_t farrow_static_alloc(uint32_t order, real_t* history, real_t* weight,
                             real_t* working);

// Choose the delay, in samples.
//
// THE DELAY MAY BE CHANGED AT ANY SAMPLE AND THE ANSWER DOES NOT JUMP, which is
// the whole reason this is built the way it is. The weights are polynomials in
// the part of a sample, worked out once when the filter is built; changing the
// delay changes only the number those polynomials are read at. A filter that
// worked its weights out afresh would cost far more and would still be this.
//
// Give false and leave the filter as it was if the delay is not one
// farrow_is_valid_delay accepts.
bool farrow_set_delay(farrow_t* farrow, real_t delay);

// Give the delay the filter is applying, in samples.
real_t farrow_get_delay(const farrow_t* farrow);

// Give one sample and take the delayed answer.
//
// The first FARROW_TAP_COUNT(order) samples come out of a filter that has not
// yet seen enough of the signal to work from. They are not wrong so much as
// unfinished, and a measurement should start after them.
real_t farrow_process_sample(farrow_t* farrow, real_t sample);

// Run a block through. The input and the output may be the same list.
bool farrow_process_block(farrow_t* farrow, const real_t* input,
                          real_t* output, uint32_t count);

// Forget every sample seen so far. The order and the delay are kept.
void farrow_reset(farrow_t* farrow);

// Give back the memory that farrow_alloc took.
void farrow_free(farrow_t* farrow);

#endif//FARROW_H
