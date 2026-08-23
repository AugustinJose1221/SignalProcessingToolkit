#ifndef DCBLOCK_H
#define DCBLOCK_H

#include <stdint.h>
#include <stdbool.h>
#ifndef TEST
#include <sptk/core/real.h>
#else
#include "real.h"
#endif

// A tracker that follows the level of a signal and takes it away.
//
// WHY THIS IS NOT A HIGH PASS FROM THE IIR MODULE
//
// Almost every reading arrives with a large constant part that carries
// nothing. A converter of 24 bits sitting near the middle of its range gives
// about eight million counts, and the signal on top of it may be a few
// thousand. The constant part must go before anything else can be measured.
//
// A high pass would do it, and for most signals it should. But at the default
// width a number holds about seven digits, and six of them are spent on a
// level of eight million.
// A high pass makes that worse rather than better: its poles lie at a radius
// near 0.9956 for a low cutoff, and a filter with such poles lifts whatever
// error reaches it by about a factor of two hundred. The rounding error of an
// eight million count input is about one count, thus two hundred counts of
// false signal come out where the true signal is a few thousand.
//
// Measured, on a wave of 1000 counts carried on a level, where the answer
// should not depend on the level at all. The error is what the filter added
// beyond its own shape, thus the row at a level of nothing is the measure of
// the shape and every row above it is the measure of the level:
//
//     level               0      1 000    100 000   8 300 000
//     iir, one section    0.0      0.0        0.2        98.9
//     iir, two sections   0.0      0.0        0.1       137.8
//     this module         0.0      0.0        0.0         0.0
//
// A hundred counts of false signal against a wave of a thousand is a tenth of
// the answer, and it comes from nothing but the size of the number. It grows
// with the order of the filter, because each section lifts the error of the
// one before it.
//
// This module is ONE POLE, and that is what saves it. A single pole holds no
// two nearly equal numbers to subtract, thus it has nothing to lose. Measured,
// what each filter ADDS when the level rises from nothing to eight million:
//
//                        32 bits    64 bits
//     iir, one section     98.9        0.0
//     this module           0.1        0.0
//
// At the default width this module is some eight hundred times better than a
// section, and the whole of that comes from its shape and not from any wider
// number. At 64 bits the section has digits to spare and the two are alike;
// there this module is simply the gentler filter of the two.
//
// WHAT IT IS AND IS NOT
//
// It is one pole, thus it falls away at 6 dB for each octave, which is gentle.
// It is meant to take the LEVEL away and nothing else. Where a whole band must
// go, put this first to bring the signal near zero, and then use the iir
// module, which now has the precision to do its work.
//
// It holds a cutoff far below what a section can hold, at either width: a
// thousand times lower than IIR_MIN_CUTOFF in both builds, because one pole
// has no cancelling sums in it. At 32 bits that is 0.000001, which at 32 kHz
// is a cutoff of 0.03 Hz.
//
// IT PRIMES ITSELF ON THE FIRST SAMPLE
//
// A filter that starts from zero sees the first sample as a step of the whole
// size of the signal. At eight million counts and a cutoff under one hertz,
// the answer to that step is larger than the signal for tens of seconds.
//
// This module sets its level to the first sample it is given. That says:
// assume the signal stood here for ever before now. There is then no step, and
// the tracker is settled from the first sample onwards.

typedef struct{
    real_t level;               // The level that the tracker follows now
    real_t pole;                // How fast it follows
    bool started;               // True once the first sample has set the level
}dcblock_t;

// The smallest cutoff that this module holds, as a part of the sample rate.
// It follows the width of the build, as the limit of a section does.
//
// A tracker of one pole moves its level by pole times the distance left to go.
// As the level nears the signal that distance shrinks, and the step with it.
// When the step becomes smaller than one step of the number itself, the level
// STOPS MOVING and stands short of where it belongs.
//
// Measured, the level reached after the tracker had settled, where the signal
// stands at 8 300 000:
//
//     cutoff      1e-5        1e-6        1e-7
//     32 bits   8296021     8260211     7902113
//     64 bits   8300000     8300000     8300000
//
// At 32 bits the tracker stops 0.5 percent short at 1e-6 and 5 percent short
// at 1e-7. At 64 bits it is exact, and it stays exact down to 1e-9, which was
// examined as far as the settling time allows: at 1e-9 it reached 7 627 678
// after two and a half time constants where the shape of the curve asks for
// 7 627 700, thus what stops it there is the waiting and not the digits.
//
// A very low cutoff is slow at either width. The tracker needs about one
// sample for each part of the cutoff, thus a cutoff of 1e-9 takes a thousand
// million samples to settle whatever the width.
#if defined(SPTK_REAL_64)
#define DCBLOCK_MIN_CUTOFF      REAL_C(0.000000001)
#else
#define DCBLOCK_MIN_CUTOFF      REAL_C(0.000001)
#endif

// True if the tracker can hold the given cutoff.
bool dcblock_is_valid_cutoff(real_t cutoff);

// Give a tracker for the given cutoff, which is a part of the sample rate.
//
// The cutoff decides how fast the tracker follows. Set it well below the
// slowest thing worth keeping: to keep breathing at 0.1 Hz, a cutoff of 0.01
// Hz follows the drift and leaves the breathing alone.
//
// This function takes no memory. A tracker whose cutoff cannot be held gives
// back a tracker that passes every sample through unchanged, which
// dcblock_is_valid_cutoff can tell the caller about first.
dcblock_t dcblock_init(real_t cutoff);

// Take the level away from one sample and give what is left.
real_t dcblock_process_sample(dcblock_t* dcblock, real_t sample);

// Take the level away from a whole block. The input and the output may be the
// same list.
void dcblock_process_block(dcblock_t* dcblock, const real_t* input, real_t* output,
                           uint32_t size);

// Give the level that the tracker holds now.
//
// This is worth reading on its own. It is the slow part of the signal, thus it
// carries the drift, the wander of a contact, and anything else that moves
// more slowly than the cutoff.
real_t dcblock_get_level(const dcblock_t* dcblock);

// Set the level directly.
//
// Use this where the level is already known, for example from a calibration.
// The tracker then does not have to find it, and it is settled at once.
void dcblock_set_level(dcblock_t* dcblock, real_t level);

// Forget the level. The next sample sets it again, as the first one did.
void dcblock_reset(dcblock_t* dcblock);

#endif//DCBLOCK_H
