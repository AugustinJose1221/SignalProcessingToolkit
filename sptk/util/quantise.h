#ifndef QUANTISE_H
#define QUANTISE_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#else
#include "real.h"
#endif

// Putting a signal into a fixed number of steps, and choosing what the error
// that makes will sound like.
//
// EVERY SIGNAL IN A DEVICE HAS BEEN THROUGH THIS. A converter of 12 bits holds
// 4096 steps and nothing between them. What falls between two steps has to go
// to one of them, and the difference is thrown away.
//
// THE ERROR IS THE SAME SIZE WHATEVER IS DONE. Nothing here makes it smaller.
// What this module chooses is WHAT SHAPE IT TAKES, and that decides whether it
// can be got rid of afterwards or not.
//
// WHY THAT CHOICE MATTERS MORE THAN THE SIZE
//
// Rounded plainly, the error follows the signal. A quiet sine crosses the same
// few steps over and over in the same pattern, thus the error repeats with the
// signal and becomes HARMONICS OF IT: false tones at two and three and four
// times the frequency, standing at fixed places in the answer. No averaging
// removes them, because they are not noise; they are a signal.
//
// Add a little noise before rounding and the pattern breaks. The error becomes
// noise spread evenly, and noise averages away: measure for four times as long
// and it falls by 6 dB, every time.
//
// Measured, a sine of 300 Hz at a hundredth of full scale put into 8 bits, at
// 8000 samples in a second. Everything is against the sine itself:
//
//                            worst false   noise below   noise above
//                            tone          1 kHz         1 kHz
//     rounded plainly        -15.6 dB      -15.2 dB       -8.0 dB
//     with dither            -30.9 dB       -7.6 dB       -2.9 dB
//     with dither and shape  -25.4 dB      -14.2 dB       +1.2 dB
//
// READ IT A COLUMN AT A TIME.
//
//   THE FIRST COLUMN is what plain rounding costs: a false tone only 15.6 dB
//   below the signal, which is a harmonic of it and which NO AMOUNT OF
//   AVERAGING REMOVES. Dither takes 15 dB off that and leaves noise in its
//   place, and noise averages away.
//
//   THE SECOND COLUMN is what the dither costs: the noise below 1 kHz rises
//   from -15.2 to -7.6, which is the price of breaking the pattern.
//
//   THE THIRD ROW IS WHY SHAPING EXISTS. It takes that noise back down to
//   -14.2, nearly where plain rounding had it, and pays for it in the third
//   column where the noise rises to +1.2. THE NOISE HAS NOT GONE ANYWHERE. It
//   has been moved out of the band the signal is in.
//
// So the three rows are three different bargains, and the last one is dither's
// freedom from false tones at nearly plain rounding's noise, for a signal that
// does not use the top of the band.
//
// AND THE THIRD ROW IS THE ONE TO BE CAREFUL WITH
//
// Noise shaping does not remove noise. It MOVES it. A signal that fills the
// whole band up to half the sample rate gains nothing at all and loses a
// little, since the total error is the worst of the three. A signal that sits
// low down, which most do, gains the whole of that 6.6 dB.
//
// WHEN TO USE WHICH
//
//   QUANTISE_PLAIN where the signal is loud and busy and fills the steps
//   anyway, or where nothing will look at the answer closely.
//   QUANTISE_DITHER where anything quiet must be measured, averaged, or
//   listened to. This is the safe answer.
//   QUANTISE_SHAPED where the signal of interest sits well below half the
//   sample rate, which is the usual case for anything sampled faster than it
//   needs to be.

// Which way the rounding is done.
typedef enum{
    // Rounded to the nearest step. The error follows the signal and becomes
    // harmonics of it.
    QUANTISE_PLAIN = 0,

    // A little noise added before rounding, which breaks the pattern and turns
    // the error into noise that averages away.
    QUANTISE_DITHER,

    // The same, and the error of each sample taken off the next, which moves
    // the noise up towards half the sample rate.
    QUANTISE_SHAPED
}quantise_way_t;

// The most steps a quantiser may have, which is what 24 bits holds.
//
// Beyond this the step is smaller than the smallest difference a 32 bit number
// can tell across the range, and the quantiser stops quantising.
#define QUANTISE_LARGEST_BITS       24u

typedef struct{
    quantise_way_t way;         // Which way the rounding is done
    real_t step;                // How far apart two steps stand
    real_t reach;               // The largest value that fits
    real_t carried;             // The error of the sample before, for shaping
    uint32_t seed;              // Where the dither stands
    bool designed;              // True once quantise_design has been called
}quantise_t;

// True if the way is one this module knows.
bool quantise_is_valid_way(quantise_way_t way);

// True if a quantiser of this many bits can be made.
bool quantise_is_valid_bits(uint32_t bits);

// Give a quantiser. It takes no memory at all, thus there is no free and one
// may be made on the stack.
quantise_t quantise_make(void);

// Choose how many steps there are and how far the signal reaches.
//
// The bits say how many steps: 8 bits is 256 of them. The reach is the largest
// value that fits, thus a signal running from -1 to 1 has a reach of 1. A
// value beyond the reach is held at it rather than wrapping round, because a
// signal that wraps does not sound loud, it sounds broken.
//
// Give false if the way or the number of bits is one the module cannot use, or
// if the reach is not above nothing.
bool quantise_design(quantise_t* quantise, quantise_way_t way, uint32_t bits,
                     real_t reach);

// Set where the dither starts, so that a run can be repeated exactly.
void quantise_set_seed(quantise_t* quantise, uint32_t seed);

// Put one sample into steps.
real_t quantise_sample(quantise_t* quantise, real_t sample);

// Put a list of samples into steps. The output may be the input.
//
// Give false if the quantiser has not been designed.
bool quantise_block(quantise_t* quantise, const real_t* input, real_t* output,
                    uint32_t count);

// How far apart two steps stand, which is the size of the error before
// anything is done about its shape.
real_t quantise_step_of(const quantise_t* quantise);

// How far down the noise of a quantiser of this many bits should lie, in
// decibels, against a signal that fills its whole reach.
//
// This is the number every converter is sold on: about 6 dB for each bit. Use
// it to see whether a measurement is meeting what the converter can do, or
// whether something else is in the way.
real_t quantise_noise_floor(uint32_t bits);

// Put the carried error back to nothing, without changing the design.
void quantise_reset(quantise_t* quantise);

#endif//QUANTISE_H
