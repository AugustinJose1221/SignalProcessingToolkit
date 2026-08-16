#ifndef FIR_H
#define FIR_H

#include <stdint.h>
#include <stdbool.h>

// A filter with a finite impulse response.
//
// The filter multiplies the last few samples of the signal by a set of
// coefficients and adds the products. It holds no feedback, thus it is always
// stable, and it moves every frequency by the same time. That second point
// matters when the shape of the signal must stay as it is.
//
// The cost is the length: such a filter needs many more coefficients than an
// IIR filter for the same sharpness. Use the iir module when the number of
// operations for each sample matters more than the shape.
//
// The design functions build the coefficients with the method of the windowed
// sinc, with the window of Hamming. Give the cutoff as a part of the sample
// rate, thus 0.25 means one quarter of the sample rate. The value must lie
// between 0 and 0.5, because half the sample rate is the highest frequency
// that a sampled signal can hold.
//
// A longer filter gives a sharper edge between the band that passes and the
// band that stops. A length of about 4/width gives an edge of that width,
// where the width is also a part of the sample rate.

typedef struct{
    uint32_t length;            // The number of coefficients
    float* coefficient;         // The coefficients
    float* history;             // The last samples, length of them
    uint32_t position;          // Where the next sample goes in the history
    bool dynamic_alloc;         // True if the memory comes from the heap
}fir_t;

// Give a filter with the given number of coefficients. The memory comes from
// the heap, and every coefficient and every sample of the history holds zero.
// Give the filter to fir_free when you no longer need it.
fir_t fir_alloc(uint32_t length);

// Give a filter that uses the memory that the caller holds. Both lists must
// hold as many float values as the given length. This function takes no
// memory from the heap.
fir_t fir_static_alloc(uint32_t length, float* coefficient, float* history);

// Build the coefficients of a filter that lets the low frequencies pass. The
// cutoff is a part of the sample rate, and it must lie between 0 and 0.5.
void fir_design_low_pass(fir_t* fir, float cutoff);

// Build the coefficients of a filter that lets the high frequencies pass.
void fir_design_high_pass(fir_t* fir, float cutoff);

// Build the coefficients of a filter that lets a band of frequencies pass. The
// low cutoff must be smaller than the high cutoff, and both must lie between 0
// and 0.5.
void fir_design_band_pass(fir_t* fir, float low_cutoff, float high_cutoff);

// Write one coefficient. Use this function to give the filter a set of
// coefficients that another program calculated.
void fir_set_coefficient(fir_t* fir, uint32_t index, float value);

// Give one coefficient.
float fir_get_coefficient(fir_t* fir, uint32_t index);

// Give the filtered value of one sample. The filter keeps the sample in its
// history, thus the next call sees it.
float fir_process_sample(fir_t* fir, float sample);

// Filter a block of samples. The input and the output may be the same list.
void fir_process_block(fir_t* fir, const float* input, float* output, uint32_t size);

// Set every sample of the history to zero. The filter then behaves as a filter
// that has seen no sample yet.
void fir_reset(fir_t* fir);

// Give the size of the answer of the filter at the given frequency, which is a
// part of the sample rate. A value of 1 says that the frequency passes
// unchanged, and a value of 0 says that the filter stops it.
float fir_get_gain(fir_t* fir, float frequency);

// Release the memory of a filter that came from fir_alloc. This function does
// nothing for a filter that came from fir_static_alloc.
void fir_free(fir_t* fir);

#endif//FIR_H
