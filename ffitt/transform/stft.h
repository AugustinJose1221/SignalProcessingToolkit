#ifndef STFT_H
#define STFT_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <ffitt/core/real.h>
#include <ffitt/linalg/cnum.h>
#include <ffitt/transform/fft.h>
#include <ffitt/transform/window.h>
#else
#include "real.h"
#include "cnum.h"
#include "fft.h"
#include "window.h"
#endif

// The transform of a signal in short pieces, so that WHEN a frequency was
// there can be seen and not only THAT it was.
//
// One transform of a whole recording says which frequencies it holds and says
// nothing at all about when. A recording of a bird and then a car gives the
// same answer as a recording of a car and then a bird. Cutting the recording
// into short pieces and transforming each one gives a frequency answer for
// each moment, which is what almost every real question wants.
//
// THE TRADE THAT CANNOT BE ESCAPED
//
// A short block sees when a thing happened but cannot say what frequency it
// was; a long block says the frequency finely but cannot say when. This is not
// a fault of the method and no method escapes it. A block of n samples at a
// sample rate of r covers n/r seconds and its bins stand r/n hertz apart, and
// the product of those two is 1 whatever is chosen.
//
//     block at 8000 samples in a second     covers      bins stand apart
//     128                                   16 ms       62.5 Hz
//     1024                                  128 ms       7.8 Hz
//     8192                                   1.02 s      0.98 Hz
//
// CHOOSE THE BLOCK FROM THE QUESTION. Speech asks what is being said, which
// changes every 20 ms, thus a block near that. A shaft turning at 50 hertz
// asks which of two nearby orders is growing, thus a block of a second or
// more. There is no default that suits both.
//
// GOING BACK IS NOT FREE, AND THE MODULE SAYS WHERE IT WORKED
//
// stft_inverse puts the pieces back together, and it can only do so where the
// windows covered the sample. Two different things can stop that.
//
// THE FIRST IS THE WINDOW AND THE HOP TOGETHER. A hop as long as the block
// leaves the samples at the ends of each block multiplied by nearly nothing,
// and nothing divided back out can recover them. stft_can_rebuild examines the
// window and the hop and says whether every sample inside the signal carries
// enough weight. Call it once after stft_design rather than finding out from a
// rebuilt signal that is quietly wrong.
//
// THE SECOND IS THE TWO ENDS OF THE SIGNAL ITSELF, and it catches everyone.
// The sample at the very start is covered by the FIRST BLOCK ONLY, where a
// sample in the middle is covered by as many blocks as fit across it. A hann
// window is zero at its first sample, thus the first sample of the whole
// signal is multiplied by zero and no arithmetic brings it back.
//
// stft_solid_range gives the stretch of samples where the cover is full and
// the answer is exact. OUTSIDE THAT STRETCH THE OUTPUT IS SET TO ZERO rather
// than left as a number that looks like an answer. Where the ends matter, put
// a block of zeros before the signal and another after it, and the stretch
// then covers everything that was really there.
//
// Measured on a block of 256, the worst error inside the solid stretch, at 32
// bits:
//
//     window       hop of block/4  hop of block/2  hop of the whole block
//     rectangular       0.0000005       0.0000005       0.0000007
//     hann              0.0000005       0.0000005   REFUSED
//     hamming           0.0000005       0.0000005       0.0000035
//     blackman          0.0000005       0.0000005   REFUSED
//
// That is the rounding of the transform itself and nothing more. At 64 bits
// every figure is below what these can show.
//
// A hop of half the block is the usual choice and rebuilds exactly with every
// window here. The refusals are real: a hann window at a hop of the whole block
// leaves the first sample of every block multiplied by zero.
//
// WHAT IS GIVEN BACK, AND HOW IT IS LAID OUT
//
// One frame for each block, and stft_bin_count bins for each frame. The frames
// lie one after another, thus the bin b of the frame f sits at
// (f * stft_bin_count(block)) + b. Only the bins up to half the block and one
// more are kept, because the signal is real and the rest is their mirror.

typedef struct{
    uint32_t block;             // Samples in one block, a power of two
    uint32_t hop;               // Samples from the start of one block to the next
    window_kind_t kind;         // The window laid on each block
    real_t parameter;           // The parameter of that window, where it takes one
    real_t* window;             // The window itself, block values
    real_t* windowed;           // One block after the window, block values
    cnum_t* spectrum;           // The transform of that block, block values
    fft_t fft;                  // The transform
    bool designed;              // True once stft_design has been called
    bool dynamic_alloc;         // True if the memory comes from the heap
}stft_t;

// The smallest weight a sample may carry, as a part of the largest, before
// stft_can_rebuild says no.
//
// Putting the pieces back divides each sample by the weight the windows laid
// on it. A sample carrying almost no weight is almost not there, and dividing
// it back up lifts whatever rounding it holds by the same amount. This holds
// that lift to a thousand.
#define STFT_SMALLEST_WEIGHT_PART   REAL_C(0.001)

// How many bins one frame holds, which is half the block and one more.
#define STFT_BIN_COUNT(block)       (((block)/2) + 1)

// True if a block of this size can be used, which means a power of two, since
// the transform underneath takes nothing else.
bool stft_is_valid_block(uint32_t block);

// True if this hop can be used with this block. The hop must be from 1 up to
// the block itself.
bool stft_is_valid_hop(uint32_t block, uint32_t hop);

// How many frames a signal of the given size gives.
//
// Only whole blocks are taken. A signal shorter than one block gives none, and
// the samples at the end that do not fill a block are not transformed. Where
// they matter, add zeros to the signal until they do.
uint32_t stft_frame_count(uint32_t size, uint32_t block, uint32_t hop);

// The fewest frames that leave any sample covered fully.
//
// A sample in the middle of a signal is under as many blocks as fit across it,
// thus this is the block divided by the hop, rounded up. Below it
// stft_solid_range gives false and stft_inverse refuses, because there is
// nothing to give back.
//
// Use it to work out the shortest signal worth taking apart, which is this
// many frames through stft_signal_size. Give 0 where the block and the hop
// cannot be used together.
uint32_t stft_fewest_frames(uint32_t block, uint32_t hop);

// How many samples come back from this many frames.
//
// This is the room stft_inverse needs, and it is NOT the size of the signal
// that went in: the samples at the end that did not fill a whole block are not
// there to come back.
uint32_t stft_signal_size(uint32_t frames, uint32_t block, uint32_t hop);

// Give a transform for blocks of the given size. The memory comes from the
// heap. Give it to stft_free when it is no longer needed.
//
// A transform whose block is 0 came back because the block is not one the
// module takes.
stft_t stft_alloc(uint32_t block);

// Give a transform that uses the memory the caller holds, taking nothing from
// the heap. The two lists of real values and the list of complex numbers must
// each hold as many values as the block, and the transform must be made for
// the same block.
stft_t stft_static_alloc(uint32_t block, real_t* window, real_t* windowed,
                         cnum_t* spectrum, fft_t fft);

// Choose the hop and the window.
//
// The hop is the distance from the start of one block to the start of the
// next, thus a hop of half the block means the blocks overlap by half. Half is
// the usual choice and rebuilds exactly with every window here.
//
// The parameter belongs to the window and is ignored where the window takes
// none; window_takes_a_parameter says which do.
//
// Give false if the hop is not from 1 to the block, or the window is unknown.
bool stft_design(stft_t* stft, uint32_t hop, window_kind_t kind,
                 real_t parameter);

// True if the window and the hop cover every sample well enough to put the
// signal back together.
//
// Call this once after stft_design rather than finding out from a rebuilt
// signal that is quietly wrong at the ends of every block.
bool stft_can_rebuild(const stft_t* stft);

// Cut the signal into blocks, window each one and transform it.
//
// The output holds stft_frame_count times STFT_BIN_COUNT complex numbers, and
// room says how many it can hold. The frames lie one after another.
//
// Give false if the transform has not been designed, if the signal is shorter
// than one block, or if the room is too small.
bool stft_forward(stft_t* stft, const real_t* signal, uint32_t size,
                  cnum_t* output, uint32_t room);

// Give the stretch of the output where the windows covered the samples fully,
// and where the answer is therefore exact.
//
// The first index and the number of samples are written out. Outside that
// stretch stft_inverse writes zero, because the samples at the two ends of the
// signal are covered by fewer blocks than the ones in the middle and cannot be
// recovered. Where the ends matter, put a block of zeros before the signal and
// another after it.
//
// Give false if the transform has not been designed, if there are no frames,
// or IF THERE ARE TOO FEW FRAMES FOR ANY SAMPLE TO BE COVERED FULLY. That last
// one is easy to meet by accident: a sample in the middle is under as many
// blocks as fit across it, thus the block divided by the hop is the fewest
// frames that can leave any sample solid at all. A block of 8 at a hop of 2
// needs 4 frames, and 3 frames leave nothing to give back. Ask
// stft_fewest_frames rather than finding out from a refusal.
bool stft_solid_range(const stft_t* stft, uint32_t frame_count,
                      uint32_t* first, uint32_t* count);

// Put the frames back together into a signal.
//
// Each frame is brought back to samples, windowed a second time and added
// where it belongs, and then every sample is divided by the weight the windows
// laid on it. Windowing a second time is what keeps the joins from showing
// when the frames have been changed in between, which is the usual reason for
// taking a signal apart at all.
//
// The output holds stft_signal_size values and the weight buffer holds the
// same, and it loses its content. Outside the stretch that stft_solid_range
// gives, the output is set to zero.
//
// Give false if the transform has not been designed, if stft_can_rebuild is
// false, if the room is too small, or if stft_solid_range gives false because
// there are too few frames for any sample to be covered fully. Nothing could
// be given back in that last case, thus nothing is: the answer would be a
// buffer of zeros wearing the look of a signal.
bool stft_inverse(stft_t* stft, const cnum_t* frames, uint32_t frame_count,
                  real_t* output, uint32_t room, real_t* weight);

// The frequency that a bin stands at, in the units of the sample rate.
real_t stft_bin_frequency(const stft_t* stft, uint32_t bin,
                          real_t sample_rate);

// The time that a frame stands at, in seconds, taken at the MIDDLE of its
// block.
//
// The middle and not the start, because a window weighs the middle of its
// block most heavily and that is where the answer of that frame really sits.
real_t stft_frame_time(const stft_t* stft, uint32_t frame, real_t sample_rate);

// Release the memory of a transform that came from stft_alloc. This does
// nothing for one that came from stft_static_alloc.
void stft_free(stft_t* stft);

#endif//STFT_H
