#ifndef EMD_H
#define EMD_H

#include <stdint.h>
#include <stdbool.h>
#ifndef TEST
#include <cspline/cspline.h>
#include <imf/imf.h>
#else
#include "cspline.h"
#include "imf.h"
#endif
// A signal with fewer than three samples holds no peak and no valley, thus
// the decomposition cannot take anything out of it.
#define EMD_MINIMUM_SIZE    3u

// The empirical mode decomposition.
//
// The decomposition takes a signal apart into intrinsic mode functions and a
// residue. Each function holds one range of frequency of the signal. The sum
// of all the functions and the residue gives the signal again.
//
// The method works in steps. It finds the peaks and the valleys of the signal,
// draws a spline through the peaks and a spline through the valleys, and takes
// the mean of the two curves away from the signal. It repeats this until the
// result is an intrinsic mode function. The rest is the residue, and the
// method starts again with it.
typedef struct{
    float* x;
    float* y;
    uint32_t size;
    cspline_t cspline;
    cspline_mempool_t cspline_mempool;
    float* peak_buffer;
    float* peak_index_buffer;
    float* valley_buffer;
    float* valley_index_buffer;
    imf_t* imf;
    uint32_t imf_count;
    float* residue;
    float* working_buffer;
    bool dynamic_alloc;
}emd_t;

// Give a decomposition for a signal of the given number of samples. The memory
// comes from the heap. Give it to emd_free when you no longer need it.
emd_t emd_alloc(uint32_t size);

// Give a decomposition that uses the memory that the caller holds. The
// parameters membank and mempool are lists of five pointers for the spline and
// for its memory pool. The two buffers must each hold room for as many float
// values as the number of samples. This function takes no memory from the heap.
emd_t emd_static_alloc(uint32_t size, float** membank, float** mempool, float* peak_buffer, float* valley_buffer);

// Give the decomposition the signal and the memory that it needs while it
// runs.
//
// The parameter imf is a list of intrinsic mode functions, and num_of_imf says
// how many it holds. That number is the largest number of functions that the
// decomposition can give. The lists x and y hold the signal. The other four
// lists must each hold room for as many float values as the number of samples.
void emd_initialize(emd_t* emd, uint32_t num_of_imf, imf_t* imf, float* x, float* y, float* residue, float* working_buffer, float* peak_index_buffer, float* valley_index_buffer);

// Take one intrinsic mode function out of the residue and give a pointer to
// it. The pointer shows into the list that emd_initialize took.
//
// The parameter stopping_threshold says how many times the method may repeat
// its step. The status becomes 1 if the method did at least one step, and 0 if
// it did none. A status of 0 says that the residue holds no more function.
//
// A signal with fewer than EMD_MINIMUM_SIZE samples holds no peak and no
// valley. The function then gives a function with the value zero and the
// status 0.
imf_t* emd_get_imf(emd_t* emd, uint32_t imf_index, uint32_t stopping_threshold, uint32_t* status);

// Take the signal apart and give the number of intrinsic mode functions that
// the method found.
//
// The function fills the list that emd_initialize took, and it leaves the rest
// of the signal in the residue. The sum of all the functions and the residue
// gives the signal again. The method stops when it has as many functions as
// emd_initialize allowed, or when the residue holds no more function.
uint32_t emd_sift(emd_t* emd, uint32_t stopping_threshold);

// Release the memory of a decomposition that came from emd_alloc. This
// function does nothing for one that came from emd_static_alloc. It does not
// touch the memory that emd_initialize took.
//
// The parameter is the structure itself and not a pointer to it, thus the
// caller must not use the structure after the call.
void emd_free(emd_t emd);

#endif//EMD_H