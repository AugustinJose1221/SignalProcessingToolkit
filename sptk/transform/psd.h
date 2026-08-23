#ifndef PSD_H
#define PSD_H

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

// How much power a signal holds at each frequency, by the method of Welch.
//
// WHY NOT SIMPLY TRANSFORM THE WHOLE SIGNAL
//
// One transform of a long signal gives a great many bins, and every one of
// them is as noisy as the last. Making the signal twice as long gives twice as
// many bins, each still as noisy: the answer becomes finer and no more
// certain. For a signal that holds noise, one transform of it is a poor
// measurement however long the signal is.
//
// Welch cuts the signal into blocks that overlap, transforms each one, and
// takes the mean of the results. The bins are coarser, because a block is
// shorter than the signal. But the noise in each bin falls as the number of
// blocks grows, and THAT is the trade this module offers:
//
//   fewer, longer blocks    fine in frequency, noisy in size
//   more, shorter blocks    coarse in frequency, steady in size
//
// A signal of 4096 samples cut into 8 overlapping blocks of 1024 gives bins 8
// times as wide and an answer about 3 times as steady.
//
// THE SCALING, WHICH IS THE PART THAT IS USUALLY WRONG
//
// A power spectral density is power for each hertz, thus its numbers do not
// change when the block gets longer or the window changes. Getting there needs
// three corrections, and leaving any of them out gives an answer that looks
// reasonable and is wrong by a factor that nobody notices:
//
//   THE WINDOW makes the signal smaller. The correction is the sum of the
//   squares of the window, not the sum of the window, because power follows
//   the square.
//
//   THE SAMPLE RATE turns power for each bin into power for each hertz.
//
//   THE OTHER HALF OF THE SPECTRUM holds the same power again, at the negative
//   frequencies. This module gives the one-sided answer, thus every bin except
//   the first and the last is doubled.
//
// With all three, a wave of amplitude A at one frequency has an area under the
// curve of A*A/2, whatever the block, the window or the overlap. The tests
// hold that.
//
// HOW MUCH TO OVERLAP
//
// A window throws away the samples at the two ends of every block. Overlapping
// the blocks uses those samples again in the next block, thus none of the
// signal is wasted. Half the block is the usual choice and suits every window
// here. More than that costs work and gains little, because blocks that
// overlap heavily hold much the same samples and their noise no longer
// averages away.

typedef struct{
    uint32_t block;             // Samples in one block
    uint32_t overlap;           // Samples that two blocks share
    window_kind_t kind;         // The window laid on each block
    real_t parameter;           // The parameter of that window, where it takes one
    real_t* window;             // The window itself, block values
    real_t* windowed;           // One block after the window, block values
    cnum_t* spectrum;           // The transform of that block, block values
    fft_t fft;                  // The transform
    real_t window_power;        // The sum of the squares of the window
    bool dynamic_alloc;         // True if the memory comes from the heap
}psd_t;

// True if a block of this size can be used. The block must be a size that the
// transform can take, which is a power of two.
bool psd_is_valid_block(uint32_t block);

// Give an estimator for blocks of the given size. The memory comes from the
// heap. Give it to psd_free when you no longer need it.
psd_t psd_alloc(uint32_t block);

// Give an estimator that uses the memory of the caller. The three lists must
// hold as many values as the block, and the transform must be made for the
// same block. This function takes no memory from the heap.
psd_t psd_static_alloc(uint32_t block, real_t* window, real_t* windowed,
                       cnum_t* spectrum, fft_t fft);

// Choose the overlap and the window.
//
// The overlap must be below the block. Half the block is the usual choice.
// The parameter belongs to the window and is ignored where the window takes
// none; window_takes_a_parameter says which do.
//
// Give false if the overlap is not below the block or the window is unknown.
bool psd_design(psd_t* psd, uint32_t overlap, window_kind_t kind,
                real_t parameter);

// How many values the answer holds, which is half the block plus one.
uint32_t psd_bin_count(const psd_t* psd);

// How many blocks a signal of the given size will be cut into.
uint32_t psd_block_count(const psd_t* psd, uint32_t size);

// The frequency that a bin stands at, in the units of the sample rate.
real_t psd_bin_frequency(const psd_t* psd, uint32_t bin, real_t sample_rate);

// The width of one bin, in the units of the sample rate.
//
// This is how finely the answer can be read. A tone that stands between two
// bins is seen in both of them and exactly in neither.
real_t psd_bin_width(const psd_t* psd, real_t sample_rate);

// Work out the density. The output holds psd_bin_count values, and each one is
// power for each hertz at the frequency that psd_bin_frequency gives.
//
// Give false if the signal is shorter than one block, or if the estimator has
// not been designed.
bool psd_estimate(psd_t* psd, const real_t* data, uint32_t size,
                  real_t sample_rate, real_t* output);

// Add up the density over a band, which gives the power in that band.
//
// This is what a density is for: the number for one bin means little on its
// own, and the area under a stretch of the curve is the power that the signal
// holds between those two frequencies.
real_t psd_band_power(const psd_t* psd, const real_t* density,
                      real_t sample_rate, real_t low, real_t high);

// Release the memory of an estimator that came from psd_alloc. This function
// does nothing for one that came from psd_static_alloc.
void psd_free(psd_t* psd);

#endif//PSD_H
