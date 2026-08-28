#ifndef DCT_H
#define DCT_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#else
#include "real.h"
#endif

// Turn a signal into cosines, and back.
//
// The transform in fft turns a signal into sines AND cosines, which is what is
// needed to say where in its turn each frequency stands. WHERE THE PHASE IS NOT
// WANTED, half of that is wasted: a real signal of n samples becomes n complex
// numbers holding 2n numbers, of which n are the mirror of the others.
//
// This turns a signal of n samples into n cosines and nothing else. It is the
// transform behind every compression of a picture or a sound that anybody uses,
// and the reason is one property:
//
// IT GATHERS A SMOOTH SIGNAL INTO ITS FIRST FEW NUMBERS. A signal that changes
// slowly comes out as a handful of large numbers followed by a long tail of
// nearly nothing, thus the tail can be thrown away and the signal rebuilt from
// what is left. Measured on a slow curve of 64 samples that does not come back
// to where it started, how many numbers are needed to hold each share of it:
//
//   share kept       this      the transform
//   ----------      -----      -------------
//   0.99                4                 20
//   0.999               8                  -
//   0.99999            30                  -
//
// THE TRANSFORM COLUMN COUNTS NUMBERS AND NOT BINS. Ten of its bins carry 0.99
// of that curve and each bin is a complex number, thus twenty numbers against
// four. The same curve, five times the room.
//
// And a signal of noise needs all 64 either way, which is the point: there is
// nothing to gather.
//
// WHY IT BEATS THE TRANSFORM AT THIS, and it is not the arithmetic. A transform
// treats the block as one turn of something that repeats, thus a signal that
// starts low and ends high has a STEP in it where the end meets the beginning
// again, and a step needs every frequency there is. This treats the block as
// half of a turn of something mirrored, thus the end meets its own mirror and
// there is no step. That is the whole of the difference.
//
// WHAT IT CANNOT DO. It says nothing about phase, thus it cannot be used to
// filter by multiplying and transforming back, and it cannot say where in its
// turn a tone stands. Reach for fft for those.
//
// WHAT IT COSTS. This works in time proportional to the SQUARE of the size,
// where fft works in time proportional to the size multiplied by its logarithm.
// At a size of 64 that is about four times as much work; at 1024 it is about a
// hundred times. The size is capped below for that reason. Against it, this
// takes any size at all rather than a power of two, and it needs no memory
// beyond what the caller gives.

#ifndef DCT_LARGEST_SIZE
#define DCT_LARGEST_SIZE    1024u
#endif

// True if this is a size the transform can be taken at. It must be at least one
// and no more than the bound above, which is where the cost of working in the
// square of the size stops being worth paying.
bool dct_is_valid_size(uint32_t size);

// Turn a signal into cosines.
//
// The output holds as many numbers as the input. The first of them is the level
// of the signal, and the rest say how much of each cosine it holds, from the
// slowest upwards.
//
// The input and the output must be different lists.
//
// Give false if the size is not one dct_is_valid_size accepts.
bool dct_forward(const real_t* input, real_t* output, uint32_t size);

// Turn cosines back into a signal.
//
// This undoes dct_forward exactly, up to the rounding of the width. The input
// and the output must be different lists.
//
// Give false if the size is not one dct_is_valid_size accepts.
bool dct_inverse(const real_t* input, real_t* output, uint32_t size);

// How many of the first numbers are needed to hold the given share of a signal.
//
// THIS IS THE NUMBER COMPRESSION IS CHOSEN BY. Give the cosines of a signal and
// the share to keep, and this says how many of them carry that share. A slow
// signal of 64 samples holds 0.99999 of itself in its first eight; a signal of
// noise needs nearly all 64.
//
// Give 0 if the size is not one dct_is_valid_size accepts, if the share is not
// above nothing and at most one, or if the signal holds nothing at all.
uint32_t dct_count_for_share(const real_t* cosines, uint32_t size,
                             real_t share);

#endif//DCT_H
