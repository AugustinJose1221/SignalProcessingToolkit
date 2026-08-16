#ifndef IMF_H
#define IMF_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/callback.h>
#else
#include "callback.h"
#endif

// An intrinsic mode function.
//
// The empirical mode decomposition takes a signal apart into such functions.
// Each one holds a part of the signal at one range of frequency. The module
// emd makes them, and this module holds one of them and writes it out.
typedef struct{
    float* x;                   // The position of each point
    float* y;                   // The value of each point
    uint32_t size;              // The number of points
    bool dynamic_alloc;         // True if the memory comes from the heap
}imf_t;

// Give a function that holds the given number of points. The memory comes from
// the heap. Give the function to imf_free when you no longer need it.
imf_t imf_alloc(uint32_t size);

// Give a function that uses the memory at x and at y. Both must hold as many
// float values as the given size. This function takes no memory from the heap.
imf_t imf_static_alloc(uint32_t size, float* x, float* y);

// Write the function, one point for each line, as "x, y". Give NULL as the
// print function to write with printf.
void imf_printf(imf_t* imf, print_t func);

// Write several functions beside each other, one point for each line and one
// column for each function. The list must hold as many functions as num_of_imf
// says, and each of them must hold as many points as size says. Give NULL as
// the print function to write with printf.
void imf_print_all(imf_t* imf, uint32_t size, uint32_t num_of_imf, print_t func);

// Release the memory of a function that came from imf_alloc. This function
// does nothing for a function that came from imf_static_alloc.
//
// The parameter is the structure itself and not a pointer to it, thus the
// caller must not use the structure after the call.
void imf_free(imf_t imf);

#endif//IMF_H
