#ifndef GENERATE_H
#define GENERATE_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#else
#include "real.h"
#endif

// Making the signals to test with, without making the faults that come free
// with them.
//
// Every test and every example in this library used to write its own sine
// wave. That is fine for a sine, and it is a trap for anything else.
//
// WHY A SQUARE WAVE IS NOT A ROW OF ONES AND MINUS ONES
//
// Write a square wave the obvious way, by taking the sign of a sine, and it
// holds every odd harmonic of its frequency, out to infinity. A sampled signal
// cannot hold anything above half the sample rate, so every harmonic above
// that FOLDS BACK and lands somewhere below it. Where it lands has nothing to
// do with the note being played.
//
// Measured, a square wave at 8000 samples in a second: the loudest thing in
// the answer that is NOT a harmonic of the tone, against the tone itself.
//
//     tone Hz        100     300     700    1300    1900    3100
//     samples a turn  80      27      11     6.2     4.2     2.6
//     naive        -39.3   -23.9   -17.3   -13.9    -9.2    -9.2  dB
//     this module  -49.3   -33.7   -29.6   -39.6   -25.7   -39.6  dB
//
// READ THE NAIVE ROW ACROSS. The fewer samples there are to a turn, the worse
// it gets, until at 1900 Hz the loudest false tone is only 9 dB below the one
// that was asked for. A filter tested with that wave is being tested against a
// signal nobody meant to make.
//
// This module holds the folding between 26 and 50 dB down across the whole
// range, which is 10 to 26 dB better than the naive one at every frequency.
//
// HOW IT IS HELD DOWN
//
// The fold comes from the corner. A square wave steps from one value to the
// other between two samples, and a step between samples is a thing a sampled
// signal cannot hold. The module works out WHERE BETWEEN THE TWO SAMPLES the
// step really falls and smooths the corner across them by that much, which is
// the method of the polynomial band-limited step.
//
// It costs a handful of operations at each corner and nothing anywhere else,
// thus a square wave costs about what the naive one costs.
//
// IT DOES NOT REMOVE THE FOLDING ALTOGETHER, and the table above is honest
// about that: the best it reaches is about 50 dB down and the worst about 26.
// Nothing that runs in constant time does better. A TEST THAT NEEDS BETTER
// THAN THAT WANTS A SINE, which folds nothing because it holds one frequency
// and no other.
//
// THE PHASE IS CARRIED AND NOT WORKED OUT FROM THE SAMPLE NUMBER
//
// Working out sin(2*pi*f*n/rate) from the sample number n looks simpler and
// goes wrong in two ways. The angle grows without bound, so a long run loses
// its digits exactly as the bluestein module records. And a frequency that
// changes cannot be written that way at all: the phase would jump every time
// the frequency did.
//
// This module carries the phase from one sample to the next and folds it into
// one turn each time, thus it runs for ever without losing digits and its
// frequency may be changed at any sample without a jump.

// Which shape to make.
typedef enum{
    // A sine. It holds one frequency and nothing else, thus it needs no
    // band-limiting and gets none.
    GENERATE_SINE = 0,

    // A square wave, band-limited at its corners.
    GENERATE_SQUARE,

    // A sawtooth, band-limited at its one corner in each turn.
    GENERATE_SAWTOOTH,

    // A triangle. It has no step, only a change of slope, thus it folds far
    // less than the other two even when written naively.
    GENERATE_TRIANGLE,

    // Random values spread evenly, holding every frequency alike.
    GENERATE_WHITE_NOISE,

    // Random values holding twice the power in each halving of frequency,
    // which is what most natural noise does.
    GENERATE_PINK_NOISE
}generate_kind_t;

// How many running parts the pink noise is made from.
//
// Each part changes half as often as the one before it, and together they give
// a slope of about 3 dB for each doubling of frequency. Seven parts hold that
// slope across about seven octaves, which covers any sample rate this library
// is used at.
#define GENERATE_PINK_PARTS     7u

typedef struct{
    generate_kind_t kind;       // Which shape
    real_t phase;               // Where in the turn, from 0 to 1
    real_t step;                // How far the phase moves each sample
    real_t sweep;               // How far the step moves each sample
    real_t last_step;           // What the step was, for the sweep to end on
    uint32_t seed;              // Where the random values stand
    real_t pink[GENERATE_PINK_PARTS];   // The running parts of the pink noise
    uint32_t counted;           // How many samples have been made
    bool designed;              // True once generate_design has been called
}generate_t;

// True if the kind is one this module knows.
bool generate_is_valid_kind(generate_kind_t kind);

// True if this frequency can be made at this sample rate, which means above
// nothing and below half the rate.
//
// A frequency at or above half the sample rate cannot be told from a lower
// one, and asking for it gives an answer about a frequency nobody wanted.
bool generate_is_valid_frequency(real_t frequency, real_t sample_rate);

// Give a maker of the given shape, standing at the start of its turn.
//
// This takes no memory at all: everything it holds is in the type. Thus there
// is no free, and one may be made on the stack.
generate_t generate_make(generate_kind_t kind);

// Choose the frequency and the sample rate.
//
// The phase is left where it stands, thus the frequency of a running maker may
// be changed at any sample and the wave carries on from where it was. That is
// what makes it possible to follow something.
//
// The two noises ignore the frequency. Give false if the kind is unknown, or
// if the frequency cannot be made at this rate and the kind is not a noise.
bool generate_design(generate_t* generate, real_t frequency,
                     real_t sample_rate);

// Set the frequency to move steadily from one to another across a number of
// samples, which makes a chirp.
//
// A CHIRP IS THE MOST USEFUL TEST SIGNAL THERE IS, because it visits every
// frequency in one run. One chirp through a filter shows the whole of what the
// filter does, where a set of tones shows only the frequencies that were
// chosen.
//
// Give false if either frequency cannot be made at this rate, or the number of
// samples is nothing.
bool generate_design_sweep(generate_t* generate, real_t from, real_t to,
                           real_t sample_rate, uint32_t samples);

// Set where the random values start, so that a run can be repeated exactly.
//
// A TEST THAT CANNOT BE REPEATED IS NOT A TEST. The same seed gives the same
// values on every machine and at either width, thus a fault found once can be
// found again.
void generate_set_seed(generate_t* generate, uint32_t seed);

// Make the next sample.
real_t generate_sample(generate_t* generate);

// Fill a list with the next samples.
//
// Give false if the maker has not been designed.
bool generate_block(generate_t* generate, real_t* output, uint32_t count);

// Put the maker back to the start of its turn, without changing the frequency.
void generate_reset(generate_t* generate);

// Give where in the turn the maker stands, from 0 to 1.
//
// Use it to make two shapes that keep step with each other: set one from the
// other after each sample.
real_t generate_get_phase(const generate_t* generate);

// Set where in the turn the maker stands, from 0 to 1.
void generate_set_phase(generate_t* generate, real_t phase);

#endif//GENERATE_H
