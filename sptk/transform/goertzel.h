#ifndef GOERTZEL_H
#define GOERTZEL_H

#include <stdint.h>
#include <stdbool.h>

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
    float coefficient;          // Comes from the frequency and the block size
    float sine;                 // Holds the phase of the result
    float cosine;               // Holds the phase of the result
    float first;                // The state of one sample ago
    float second;               // The state of two samples ago
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
goertzel_t goertzel_init(float frequency, float sample_rate, uint32_t block_size);

// Give one sample to the detector.
void goertzel_process_sample(goertzel_t* goertzel, float sample);

// Give a block of samples to the detector.
void goertzel_process_block(goertzel_t* goertzel, const float* input, uint32_t size);

// True when the detector has read a whole block. Read the result then, and
// call goertzel_reset before the next block.
bool goertzel_is_block_complete(goertzel_t* goertzel);

// Give the square of the size of the answer at the frequency of the detector.
// This function takes no square root, thus it is faster than
// goertzel_magnitude. Use it to compare the strength of two frequencies.
float goertzel_magnitude_squared(goertzel_t* goertzel);

// Give the size of the answer at the frequency of the detector.
float goertzel_magnitude(goertzel_t* goertzel);

// Give the phase of the answer in radians, between -pi and pi.
float goertzel_phase(goertzel_t* goertzel);

// Set the state to zero, so that the detector can read a new block. The
// frequency and the block size do not change.
void goertzel_reset(goertzel_t* goertzel);

#endif//GOERTZEL_H
