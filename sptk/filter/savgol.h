#ifndef SAVGOL_H
#define SAVGOL_H

#include <stdint.h>
#include <stdbool.h>
#ifndef TEST
#include <sptk/core/real.h>
#else
#include "real.h"
#endif

// The filter of Savitzky and Golay.
//
// The filter smooths a signal and keeps its shape. It takes a window of
// samples, lays a polynomial through them by the method of the least squares,
// and gives the value of that polynomial at the middle of the window.
//
// A plain mean of a window makes a peak lower and wider. This filter does not,
// because a polynomial can follow a peak. Thus the filter suits a signal where
// the height and the width of a peak carry the information, such as the result
// of a spectrometer or a chromatograph.
//
// The filter can also give a derivative of the signal. The derivative of the
// polynomial at the middle of the window is a much better answer than the
// plain difference of two samples, which noise disturbs strongly.
//
// The window must hold an odd number of samples, so that it has a middle. The
// order of the polynomial must be below the size of the window. A higher order
// follows the signal more closely and takes away less noise.
//
// The design uses the matrix module: it builds the matrix of the powers of the
// positions in the window, and it solves the normal equations of the least
// squares. That work happens one time, at savgol_design. The filter itself
// then multiplies and adds only.

typedef struct{
    uint32_t window;            // The number of samples of the window, odd
    uint32_t order;             // The order of the polynomial
    uint32_t derivative;        // Which derivative the filter gives
    real_t* coefficient;         // One coefficient for each sample of the window
    bool dynamic_alloc;         // True if the memory comes from the heap
}savgol_t;

// Give a filter for the given window. The memory comes from the heap. Give the
// filter to savgol_free when you no longer need it.
savgol_t savgol_alloc(uint32_t window);

// Give a filter that uses the memory at coefficient, which must hold as many
// float values as the window. This function takes no memory from the heap.
savgol_t savgol_static_alloc(uint32_t window, real_t* coefficient);

// True if the window and the order fit together: the window must be odd and
// larger than the order, and the derivative must not be above the order.
bool savgol_is_valid(uint32_t window, uint32_t order, uint32_t derivative);

// Build the coefficients of the filter.
//
// A derivative of 0 gives the smoothed signal. A derivative of 1 gives the
// first derivative, 2 the second one, and so on. The order must be below the
// size of the window, and the derivative must not be above the order.
//
// This function gets memory from the heap for the matrices of the least
// squares. It runs one time, before the filter reads any sample. The filter
// itself gets no memory.
//
// Give false if the window and the order do not fit together, or if the
// matrix of the least squares has no inverse.
bool savgol_design(savgol_t* savgol, uint32_t order, uint32_t derivative);

// Give one coefficient of the filter.
real_t savgol_get_coefficient(savgol_t* savgol, uint32_t index);

// Give the filtered value at the middle of the given window of samples. The
// list must hold as many samples as the window of the filter.
real_t savgol_apply(savgol_t* savgol, const real_t* window);

// Filter a whole signal.
//
// The function writes as many values as the signal holds. Near the two ends
// there are not enough samples for a whole window, thus the function repeats
// the first and the last sample to fill it. The input and the output must not
// be the same list.
//
// The result of a derivative is for one step of the sample. Divide it by the
// time between two samples to get a derivative for the time.
void savgol_process_block(savgol_t* savgol, const real_t* input, real_t* output,
                          uint32_t size);

// Release the memory of a filter that came from savgol_alloc. This function
// does nothing for a filter that came from savgol_static_alloc.
void savgol_free(savgol_t* savgol);

#endif//SAVGOL_H
