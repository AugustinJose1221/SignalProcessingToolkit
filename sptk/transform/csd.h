#ifndef CSD_H
#define CSD_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#include <sptk/linalg/cnum.h>
#include <sptk/transform/fft.h>
#include <sptk/transform/window.h>
#else
#include "real.h"
#include "cnum.h"
#include "fft.h"
#include "window.h"
#endif

// What TWO signals have in common at each frequency, and how much of one
// explains the other.
//
// The psd module says what one signal holds. This one takes two, and answers
// questions that one signal cannot:
//
//   IS THIS VIBRATION COMING FROM THAT MOTOR? Both hold a peak at 50 hertz.
//   That alone proves nothing; half the building holds a peak at 50 hertz.
//   Coherence says whether the two peaks move together.
//   WHAT DOES THIS BOX DO TO A SIGNAL? Measure in and out, and the transfer
//   estimate gives the gain and the phase shift at every frequency at once,
//   without ever putting a single tone through it.
//   HOW FAR APART ARE THESE TWO MICROPHONES? The phase of the cross spectrum
//   rises steadily with frequency, and the rate it rises at is the delay.
//
// COHERENCE IS THE ONE THAT IS MOST USED AND MOST OFTEN WRONG
//
// It reads from 0 to 1: 1 means that at this frequency one signal explains the
// other completely, and 0 means they have nothing to do with each other.
//
// THE TRAP: A SINGLE BLOCK GIVES A COHERENCE OF EXACTLY 1, ALWAYS. Two signals
// of pure noise, with nothing whatever in common, read 1 at every frequency.
// It is not a rounding matter and no width fixes it: with one block the
// arithmetic reduces to a number divided by itself. The estimate only means
// anything once several blocks have been averaged, and what is being measured
// is whether the relation HOLDS STILL from block to block.
//
// Measured, on two signals of noise that are wholly unrelated, where the true
// coherence is 0 at every frequency. The mean reading across all bins:
//
//     blocks         1       2       4       8      16      32      64
//     reading     1.00    0.46    0.35    0.13    0.06    0.04    0.02
//
// The reading falls as about 1 divided by the number of blocks. A reading of
// 0.35 is evidence of nothing at all if it came from 4 blocks. THIS MODULE
// REFUSES BELOW CSD_SMALLEST_BLOCK_COUNT BLOCKS, and above it the table is the
// rule of thumb: take a reading seriously when it stands well above 1 divided
// by the number of blocks.
//
// THE SIGNALS MUST BE MEASURED AT THE SAME MOMENTS. Two recordings started a
// second apart hold the same events at different sample numbers, and every
// answer here is then about a relation that is not there. Where a delay is
// what is being looked for, that is the point; where it is not, it is a fault.

typedef struct{
    uint32_t block;             // Samples in one block
    uint32_t overlap;           // Samples that two blocks share
    window_kind_t kind;         // The window laid on each block
    real_t parameter;           // The parameter of that window, where it takes one
    real_t* window;             // The window itself, block values
    real_t* windowed;           // One block after the window, block values
    cnum_t* first;              // The transform of a block of the first signal
    cnum_t* second;             // The transform of a block of the second
    cnum_t* cross;              // What the two share, one for each bin
    real_t* first_power;        // What the first holds, one for each bin
    real_t* second_power;       // What the second holds, one for each bin
    fft_t fft;                  // The transform
    real_t window_power;        // The sum of the squares of the window
    bool designed;              // True once csd_design has been called
    bool dynamic_alloc;         // True if the memory comes from the heap
}csd_t;

// The fewest blocks that give an answer worth having.
//
// Below this the reading is mostly the arithmetic looking at itself. Eight
// blocks put the reading for two unrelated signals near 0.13, which leaves
// room to tell a real relation from none.
#ifndef CSD_SMALLEST_BLOCK_COUNT
#define CSD_SMALLEST_BLOCK_COUNT    8u
#endif

// How many values an answer holds, which is half the block and one more.
#define CSD_BIN_COUNT(block)        (((block)/2) + 1)

// True if a block of this size can be used, which means a power of two.
bool csd_is_valid_block(uint32_t block);

// Give an estimator for blocks of the given size. The memory comes from the
// heap. Give it to csd_free when it is no longer needed.
csd_t csd_alloc(uint32_t block);

// Give an estimator that uses the memory of the caller, taking nothing from
// the heap.
//
// The lists window, windowed, first and second must each hold as many values
// as the block. The lists cross, first_power and second_power must each hold
// CSD_BIN_COUNT(block) values. The transform must be made for the same block.
csd_t csd_static_alloc(uint32_t block, real_t* window, real_t* windowed,
                       cnum_t* first, cnum_t* second, cnum_t* cross,
                       real_t* first_power, real_t* second_power, fft_t fft);

// Choose the overlap and the window. Half the block is the usual overlap.
//
// Give false if the overlap is not below the block or the window is unknown.
bool csd_design(csd_t* csd, uint32_t overlap, window_kind_t kind,
                real_t parameter);

// How many blocks a signal of the given size will be cut into.
uint32_t csd_block_count(const csd_t* csd, uint32_t size);

// The frequency that a bin stands at, in the units of the sample rate.
real_t csd_bin_frequency(const csd_t* csd, uint32_t bin, real_t sample_rate);

// Work out the cross spectral density of two signals.
//
// The output holds CSD_BIN_COUNT complex numbers. The size of each one says
// how much the two signals share at that frequency, and its angle says how far
// the second lags behind the first there.
//
// The two signals must hold the same number of samples and must have been
// measured at the same moments.
//
// This takes no memory: the working room came with the estimator.
//
// Give false if the estimator has not been designed, or if the signals are
// shorter than CSD_SMALLEST_BLOCK_COUNT blocks.
bool csd_estimate(csd_t* csd, const real_t* first, const real_t* second,
                  uint32_t size, real_t sample_rate, cnum_t* output);

// Work out how much of one signal is explained by the other at each frequency,
// from 0 to 1.
//
// The output holds CSD_BIN_COUNT values. Read the header before believing any
// of them: a reading near 1 divided by the number of blocks is what two
// unrelated signals give, and it is evidence of nothing.
//
// Give false for the same reasons as csd_estimate.
bool csd_coherence(csd_t* csd, const real_t* first, const real_t* second,
                   uint32_t size, real_t* output);

// Work out what one signal does to the other at each frequency: the gain and
// the phase shift of whatever lies between them.
//
// The output holds CSD_BIN_COUNT complex numbers, being the cross spectrum
// divided by the density of the first signal. This is the estimate that is
// blind to noise ADDED TO THE SECOND signal, which is the usual case: the
// noise of a sensor at the output does not bend the answer, while noise on the
// input does.
//
// LOOK AT THE COHERENCE BESIDE IT. Where the coherence is low the gain here is
// still a number, and it is a number about nothing. A transfer estimate
// without a coherence beside it is not a measurement.
//
// Give false for the same reasons as csd_estimate.
bool csd_transfer(csd_t* csd, const real_t* first, const real_t* second,
                  uint32_t size, cnum_t* output);

// Release the memory of an estimator that came from csd_alloc. This does
// nothing for one that came from csd_static_alloc.
void csd_free(csd_t* csd);

#endif//CSD_H
