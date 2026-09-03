#ifndef DEF_H
#define DEF_H

#include <stdlib.h>
#include <stdio.h>

#include <assert.h>

// WHAT AN ASSERTION IS FOR HERE, AND WHAT IT IS NOT FOR.
//
// An ASSERT states a thing the CALLER must have got right and that no header
// promises to answer: a pointer that is not NULL, a matrix that is square, a
// size that a module says it will not take. Breaking one is a fault in the
// program that calls, not an answer it is owed.
//
// WHERE A HEADER PROMISES AN ANSWER, THE CODE MUST GIVE THAT ANSWER AND NEVER
// ASSERT. A function whose header says it gives false for an even length must
// give false for an even length. An assertion there does not guard the caller;
// it takes away the answer the header sold.
//
// THIS USED TO BE SWITCHED OFF EXACTLY WHEN THE TESTS RAN.
//
// ASSERT was defined as nothing when TEST was defined, and ceedling defines
// TEST for every test build. Thus the tests exercised a library with every
// assertion removed while production shipped a library that carried them: the
// two were never the same code, and the tests could not see an assertion that
// was wrong.
//
// Two were. fir_design_band_stop asserted against an even length that its own
// header promises to refuse with false. cepstrum_alloc asked the transform for
// a size it was about to reject, and the transform asserts on such a size, thus
// the graceful handle that the cepstrum header promises could never be reached.
// Both are mended, and with the assertions live all 984 tests pass.
//
// TO SWITCH THEM OFF, define NDEBUG, which is the standard way and what a
// release build of a target with no console wants. An assertion calls abort
// through the standard library.
#define ASSERT(x) assert(x)

#endif//DEF_H
