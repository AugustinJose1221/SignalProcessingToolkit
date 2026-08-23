#ifndef CORRELATE_H
#define CORRELATE_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#include <sptk/linalg/cnum.h>
#include <sptk/transform/fft.h>
#else
#include "real.h"
#include "cnum.h"
#include "fft.h"
#endif

// How much one signal is like another when one of them is moved in time.
//
// This answers three questions that come up again and again, and all three are
// the same question with different signals put into it.
//
//   HOW LONG IS THE DELAY between two recordings of the same thing? Correlate
//   the two. The lag where the answer is largest is the delay.
//
//   DOES THIS SIGNAL REPEAT, and how often? Correlate the signal with itself.
//   A signal that repeats every 100 samples has a peak at a lag of 100.
//
//   IS THIS SHAPE IN THAT SIGNAL? Correlate the signal with the shape. A peak
//   says where the shape sits.
//
// WHAT THE ANSWER IS SCALED BY, AND WHY IT MATTERS
//
// The raw sum of products grows with the length of the signal and with how
// large the samples are, thus two answers cannot be set beside each other. The
// scaling decides what the number means:
//
//   CORRELATE_RAW         the sum of the products, and nothing more
//   CORRELATE_BIASED      divided by the number of samples
//   CORRELATE_UNBIASED    divided by how many samples overlapped at that lag
//   CORRELATE_COEFFICIENT a number between -1 and 1
//
// TAKE CORRELATE_COEFFICIENT WHEN THE ANSWER MUST BE JUDGED and not only
// compared with itself. It is the only one that means the same thing for every
// signal: 1 is a perfect match, 0 is no likeness at all, -1 is the same shape
// turned upside down. A threshold on it holds from one recording to the next,
// and a threshold on any of the others does not.
//
// THE MEAN MUST COME OFF FIRST, AND THE COEFFICIENT TAKES IT OFF
//
// A signal that never goes below zero correlates well with itself at EVERY
// lag, because the product of two positive numbers is positive whatever the
// lag. The mean then swamps the part that actually repeats, and the largest
// answer lands wherever the arithmetic happens to put it rather than at the
// period.
//
// CORRELATE_COEFFICIENT takes the mean off, because a correlation coefficient
// is not defined any other way. The other three do not, because they are sums
// and a caller who asks for a sum should get one.
//
// The coefficient is worked out over the samples that overlap AT EACH LAG and
// no others. The shorter way, which is the sum at each lag divided by the sum
// at no lag, falls away as the lag grows: at a lag of k only size-k samples
// overlap, thus the answer comes out about (size-k)/size of the truth. For a
// lag of an eighth of the signal that is an eighth too small, and a threshold
// set on it would hold for one length of signal and not for another. That
// would defeat the whole reason for having a coefficient.
//
// WHAT IT COSTS, AND THE FAST WAY
//
// The plain method multiplies and adds once for each sample at each lag, thus
// it costs size times lags. For 4096 samples and 4096 lags that is 17 million
// operations.
//
// The transform does the same work in three transforms and one multiplication
// of each bin, because a correlation in time is a multiplication in frequency.
// For the same numbers that is about 300 thousand operations, which is fifty
// times less. It needs memory to work in, which the caller gives, and the size
// must be a power of two. Below about 300 samples the plain method wins,
// because the transform has a fixed cost that the plain method does not.

typedef enum{
    CORRELATE_RAW = 0,          // The sum of the products
    CORRELATE_BIASED,           // Divided by the number of samples
    CORRELATE_UNBIASED,         // Divided by the overlap at each lag
    CORRELATE_COEFFICIENT       // Between -1 and 1, with the mean taken off
}correlate_scaling_t;

// True if the module knows this scaling.
bool correlate_is_valid_scaling(correlate_scaling_t scaling);

// Correlate a signal with itself, over the lags 0 to max_lag.
//
// The output holds max_lag+1 values, and output[k] is the answer at the lag k.
// At a lag of 0 a signal always matches itself, thus output[0] is the largest
// value that any lag can reach, and with CORRELATE_COEFFICIENT it is 1.
//
// The max_lag must be below the size. A lag as large as the size leaves no
// samples that overlap, thus there is nothing to correlate.
//
// Give false if the sizes do not fit together or the scaling is unknown.
bool correlate_auto(const real_t* data, uint32_t size, real_t* output,
                    uint32_t max_lag, correlate_scaling_t scaling);

// Correlate one signal with another, over the lags 0 to max_lag.
//
// The lag moves the SECOND signal later in time. Thus output[k] is large when
// b holds at k samples later what a holds now, which is to say when b lags a
// by k samples.
//
// Both signals must hold the same number of samples.
//
// Give false if the sizes do not fit together or the scaling is unknown.
bool correlate_cross(const real_t* a, const real_t* b, uint32_t size,
                     real_t* output, uint32_t max_lag,
                     correlate_scaling_t scaling);

// Give the lag between low_lag and high_lag where a signal is most like
// itself, and write how strong that likeness is into strength.
//
// This is the whole of finding a period in one call. A signal that repeats
// every 100 samples gives back 100. The strength is a coefficient, thus it can
// be judged: a signal that truly repeats gives something near 1, and a signal
// that does not gives something near 0. Measured on a recording of a heart
// inside a scanner, the artefact of the scanner gave 0.998 and the same
// recording with no scanner gave 0.011.
//
// The lag of 0 must be left out of the range, because every signal matches
// itself perfectly there and that answer says nothing.
//
// The output list must hold high_lag+1 values, and the function uses it to
// work in. Give NULL for strength if the strength is not wanted.
//
// Give 0 and a strength of 0 if the range does not fit inside the signal.
uint32_t correlate_best_lag(const real_t* data, uint32_t size, real_t* output,
                            uint32_t low_lag, uint32_t high_lag,
                            real_t* strength);

// Give the size of the transform that the fast method needs for a signal of
// the given size, or 0 if no transform of a size it can use is large enough.
//
// The transform must be at least twice the size of the signal, so that the two
// ends of the signal cannot wrap round and correlate with each other.
uint32_t correlate_transform_size(uint32_t size);

// Correlate a signal with itself using the transform.
//
// This gives the same answer as correlate_auto, to the last digit that the
// width can hold, and costs far less for a long signal.
//
// IT SERVES THE THREE SCALINGS THAT ARE SUMS AND NOT THE COEFFICIENT. A
// transform gives the sum at each lag and nothing else, and a coefficient
// needs the mean and the energy of the samples that overlap at each lag on
// their own. Give CORRELATE_COEFFICIENT to correlate_auto instead, which works
// them out lag by lag.
//
// The caller gives everything it needs, thus this module takes no memory of
// its own and works on a target with no heap:
//
//   fft      made for correlate_transform_size(size), by either fft_alloc or
//            fft_static_alloc
//   work     correlate_transform_size(size) complex values
//   window   correlate_transform_size(size) real values
//
// Give false if the sizes do not fit together, if the scaling is unknown or is
// CORRELATE_COEFFICIENT, if no transform large enough can be made for this
// size, or if the transform that was given is not of that size.
bool correlate_auto_by_transform(const real_t* data, uint32_t size,
                                 real_t* output, uint32_t max_lag,
                                 correlate_scaling_t scaling,
                                 fft_t* fft, cnum_t* work, real_t* window);

#endif//CORRELATE_H
