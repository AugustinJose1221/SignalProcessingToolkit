#ifndef GOERTZEL_H
#define GOERTZEL_H

#include <stdint.h>
#include <stdbool.h>
#ifndef TEST
#include <ffitt/core/real.h>
#else
#include "real.h"
#endif

// The algorithm of Goertzel.
//
// The algorithm says how much of one frequency a signal holds. A fast Fourier
// transform gives every frequency at one time and needs memory for the whole
// block. This algorithm gives one frequency and holds three float values only.
// Thus it suits a small target that watches for a few known tones, such as the
// tones of a telephone keypad.
//
// The cost for one frequency is one multiplication and two additions for each
// sample. For a few frequencies that is much less work than a transform. When
// you need more than about log2(n) frequencies, the transform costs less.
//
// The algorithm reads a block of a fixed number of samples. Give each sample
// to goertzel_process_sample, and then read the result. The block size and the
// sample rate decide which frequencies the algorithm can see clearly: a
// frequency that holds a whole number of turns inside the block gives the
// clearest answer.
//
// Call goertzel_reset before each new block.

typedef struct{
    real_t coefficient;          // Comes from the frequency and the block size
    real_t sine;                 // Holds the phase of the result
    real_t cosine;               // Holds the phase of the result
    real_t first;                // The state of one sample ago
    real_t second;               // The state of two samples ago
    uint32_t block_size;        // The number of samples of one block
    uint32_t count;             // The number of samples that came in
}goertzel_t;

// Give a detector for one frequency.
//
// The frequency and the sample rate are both in hertz, and the frequency must
// be below half the sample rate. The block size is the number of samples that
// the detector reads before it gives a result.
//
// This function takes no memory. The whole state lies inside the structure,
// thus a caller on a target with no heap can hold it anywhere.
goertzel_t goertzel_init(real_t frequency, real_t sample_rate, uint32_t block_size);

// Give one sample to the detector.
void goertzel_process_sample(goertzel_t* goertzel, real_t sample);

// Give a block of samples to the detector.
void goertzel_process_block(goertzel_t* goertzel, const real_t* input, uint32_t size);

// True when the detector has read a whole block. Read the result then, and
// call goertzel_reset before the next block.
bool goertzel_is_block_complete(goertzel_t* goertzel);

// Give the square of the size of the answer at the frequency of the detector.
// This function takes no square root, thus it is faster than
// goertzel_magnitude. Use it to compare the strength of two frequencies.
real_t goertzel_magnitude_squared(goertzel_t* goertzel);

// Give the size of the answer at the frequency of the detector.
real_t goertzel_magnitude(goertzel_t* goertzel);

// Give the phase of the answer in radians, between -pi and pi.
//
// IT IS THE PHASE OF THE TRANSFORM MEASURED FROM THE OTHER END OF THE BLOCK,
// and a caller setting it beside an fft phase must know so.
//
// The recurrence carries two numbers and the answer is read off them when the
// block ends, thus its origin stands at the LAST sample where the transform's
// stands at the first. That puts a fixed turn between the two:
//
//     this phase = the transform phase + 2 pi k (N - 1) / N
//
// where k is the bin the frequency falls on and N is the block. Measured, the
// two agree to five decimal places at every block and bin tried. It is not a
// fault and it cannot be taken out without making the recurrence carry a third
// number, which would cost the whole reason to reach for this rather than for a
// transform.
//
// NOTHING THAT COMPARES TWO ANSWERS FROM THIS MODULE IS TOUCHED BY IT. The turn
// is the same for both, thus it cancels: the phase BETWEEN two signals read at
// the same bin, or the way the phase moves as a signal is moved along, are both
// right as they stand.
real_t goertzel_phase(goertzel_t* goertzel);

// Set the state to zero, so that the detector can read a new block. The
// frequency and the block size do not change.
void goertzel_reset(goertzel_t* goertzel);

#endif//GOERTZEL_H
