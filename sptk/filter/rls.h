#ifndef RLS_H
#define RLS_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#include <sptk/core/ringbuf.h>
#else
#include "real.h"
#include "ringbuf.h"
#endif

// A filter that finds its own coefficients, by least squares, at every sample.
//
// The adaptive module takes one small step downhill with each sample and
// arrives after thousands of them. THIS ONE SOLVES THE WHOLE LEAST SQUARES
// PROBLEM AT EVERY SAMPLE, over everything it has heard so far, and arrives in
// about twice as many samples as it has coefficients.
//
// Measured, on a filter of 16 coefficients learning an unknown response, the
// samples taken to bring the coefficients 40 dB towards the truth:
//
//     normalised least mean squares (adaptive)     163
//     this module                                   24
//
// Twenty-four samples for a filter of sixteen. That is what it is for.
//
// BUT READ THE OTHER HALF OF THAT MEASUREMENT. Left to run, the two settle at
// different places:
//
//                                   32 bits    64 bits
//     normalised least mean squares  -149 dB    -317 dB
//     this module                     -97 dB    -137 dB
//
// The adaptive module goes on improving long after this one has stopped,
// because this one is limited by the precision of the matrix it carries and
// that one is limited by nothing.
//
// SO THE CHOICE IS NOT WHICH IS BETTER. It is whether the answer is wanted
// quickly or wanted exactly. Where the thing being learned changes often, only
// the first matters. Where it stands still and there is time, the adaptive
// module ends up ahead and costs a hundredth of the memory.
//
// WHAT IT COSTS IN MEMORY, WHICH IS WHY THIS IS NOT A RULE OF THE ADAPTIVE
// MODULE
//
// The adaptive module holds a few numbers for each coefficient. This one holds
// a whole square matrix beside them, because solving a least squares problem
// needs the inverse of a correlation matrix and that is what it carries
// forward. At 32 bits:
//
//     length     adaptive     this module
//     16          0.1 kB        1.3 kB
//     64          0.5 kB       17 kB
//     256         2 kB        266 kB
//
// A length of 256 is ordinary for an echo canceller and 266 kB is not ordinary
// for a device. The cost had to be visible in the type rather than hidden
// behind an enumeration, and that is why this is a module of its own.
//
// The work for each sample grows the same way: with the square of the length
// where the adaptive module grows with the length itself.
//
// WHAT IT COSTS IN PRECISION, WHICH IS THE PART THAT SURPRISES PEOPLE
//
// THE FILTER CAN RUN CORRECTLY FOR THOUSANDS OF SAMPLES AND THEN FALL APART.
// The matrix it carries forward should stay symmetric and should describe a
// spread that is real in every direction. Nothing in the arithmetic holds it
// to that.
//
// This module writes the two halves of that matrix TOGETHER: one half is
// worked out and the same value is put in both places. Written the usual way,
// each half is worked out on its own and their roundings differ.
//
// Measured, on a filter of 16 over a million samples at 32 bits:
//
//     forgetting      halves drift apart by     what happens
//     worked out apart
//       1.000              1.03                 held
//       0.999              1.82                 FELL OVER at sample 7230
//       0.990              1.84                 FELL OVER at sample 987
//       0.950              1.31                 FELL OVER at sample 216
//     written together
//       every one          0.00                 held, all four
//
// READ THE FIRST COLUMN. The two halves come to differ by MORE THAN THE
// LARGEST ELEMENT of the matrix, and then one direction of the spread goes
// below nothing and the coefficients run away. Writing them together holds
// them exactly equal, for nothing, for ever. The ukf module holds its
// covariance together the same way, for the same reason.
//
// The module also WATCHES THE DIAGONAL. Where an element of it falls to
// nothing or below, the spread has stopped being real and rls_is_healthy gives
// false.
//
// ASK rls_is_healthy. A filter that has fallen apart still answers, and its
// answers are nonsense.
//
// AND THE FORGETTING FACTOR IS NOT A TUNING KNOB
//
// It says how much of the past to keep. At 1 the filter remembers everything
// and settles on the best answer for all of it, which is right where the thing
// being learned does not change. Below 1 the past fades, and the filter can
// follow something that moves.
//
// The fading is what makes the matrix lose its footing faster, and the table
// above is that in numbers: the same filter written the careless way lasts
// 7230 samples at 0.999 and 216 at 0.95. Roughly, the filter holds about
// 1/(1-factor) samples: 0.99 holds a hundred, 0.999 holds a thousand.

typedef struct{
    ringbuf_t history;          // The last samples of the reference
    real_t* coefficient;        // What the filter has learned so far
    real_t* inverse;            // The correlation matrix turned round
    real_t* gain;               // Working room, one for each coefficient
    real_t* carried;            // Working room, one for each coefficient
    uint32_t length;            // How many coefficients
    real_t forgetting;          // How much of the past to keep
    real_t doubt;               // What the matrix starts at
    bool healthy;               // False once the matrix has stopped being real
    bool dynamic_alloc;         // True if the memory comes from the heap
}rls_t;

// How many real values the matrix of a filter of this length needs.
#define RLS_MATRIX_SIZE(length)     ((length) * (length))

// The largest forgetting factor, which is to forget nothing at all.
#define RLS_LARGEST_FORGETTING      REAL_C(1.0)

// The smallest forgetting factor worth asking for.
//
// Below this the matrix is being rebuilt from too few samples to stay real,
// and the filter falls apart quickly whatever else is done.
#ifndef RLS_SMALLEST_FORGETTING
#define RLS_SMALLEST_FORGETTING     REAL_C(0.9)
#endif

// What the matrix starts at, as a multiple of the unit matrix.
//
// A large value says the filter knows nothing yet and should believe the first
// samples strongly, which is what makes it settle quickly. Too large a value
// makes the first few steps enormous and can throw the matrix off before it
// has begun.
#ifndef RLS_DEFAULT_DOUBT
#define RLS_DEFAULT_DOUBT           REAL_C(100.0)
#endif

// True if this forgetting factor can be used.
bool rls_is_valid_forgetting(real_t forgetting);

// Give a filter of the given length. The memory comes from the heap, and there
// is a great deal of it: read the table in the header before choosing a
// length. Give the filter to rls_free when it is no longer needed.
rls_t rls_alloc(uint32_t length);

// Give a filter that uses the memory the caller holds, taking nothing from the
// heap.
//
// The coefficient, gain and carried lists must each hold length values, the
// history must hold length values, and the inverse must hold
// RLS_MATRIX_SIZE(length) of them.
rls_t rls_static_alloc(uint32_t length, real_t* coefficient, real_t* inverse,
                       real_t* gain, real_t* carried, real_t* history);

// Choose how much of the past to keep, and how strongly to believe the first
// samples.
//
// Give RLS_DEFAULT_DOUBT for the doubt unless there is a reason not to. A
// forgetting factor of 1 suits anything that does not change; below 1 the past
// fades and the filter can follow something that moves.
//
// This also clears the filter, thus it is where a run begins.
//
// Give false if the forgetting factor is outside RLS_SMALLEST_FORGETTING to
// RLS_LARGEST_FORGETTING, or the doubt is not above nothing.
bool rls_design(rls_t* rls, real_t forgetting, real_t doubt);

// Put one sample through the filter and let it learn from what it got wrong.
//
// The reference is what the filter is given and the wanted value is what it
// should have produced. The answer is what the filter DID produce, before it
// learned.
//
// AS WITH THE ADAPTIVE MODULE, THE ERROR IS OFTEN THE ANSWER: where the filter
// is learning noise so that it can be taken away, what is left over is the
// signal. rls_error gives it.
//
// WHAT IS LEFT OVER IS NOT THE SIGNAL EXACTLY, and it is worth knowing by how
// much. The filter is estimating a response from measurements that hold the
// signal too, and the signal it cannot see acts as noise on that estimate. The
// more of the past it keeps, the better the estimate and the less is left.
//
// Measured, a filter of 8 taking away interference whose size is 0.35 beside a
// signal of size 1:
//
//     forgetting        1.000      0.999      0.990
//     what is left      0.030      0.033      0.117
//     which is         -21 dB     -20 dB      -9 dB of the interference
//
// A FADING PAST COSTS CANCELLATION. Keep as much of it as the thing being
// learned allows: fade only as fast as that thing really moves.
real_t rls_process_sample(rls_t* rls, real_t reference, real_t wanted);

// The same, giving what is left over rather than what the filter produced.
real_t rls_error(rls_t* rls, real_t reference, real_t wanted);

// Run a whole block through, learning from every sample of it.
//
// The output takes what the filter made of the reference and the error takes
// what is left when that has been taken away. Either may be NULL.
//
// THE ERROR IS ALMOST ALWAYS THE ANSWER, for the reason adaptive gives: the
// output is the interference as the filter learned it and the error is what
// remains.
//
// Give false if the filter holds no memory, or if the matrix has stopped being
// a real one part way through. A block that fails part way has still changed
// the filter, thus rls_is_healthy is the thing to read afterwards.
bool rls_process_block(rls_t* rls, const real_t* reference,
                       const real_t* wanted, real_t* output, real_t* error,
                       uint32_t count);

// True while the matrix the filter carries still describes a real spread.
//
// ASK THIS. A filter that has fallen apart still answers, and its answers are
// nonsense. Once it gives false the filter cannot recover on its own; call
// rls_design again to begin afresh.
bool rls_is_healthy(const rls_t* rls);

// Give one coefficient of what the filter has learned.
real_t rls_get_coefficient(const rls_t* rls, uint32_t index);

// Clear everything the filter has learned and set the matrix back to the doubt
// it was designed with.
void rls_reset(rls_t* rls);

// Release the memory of a filter that came from rls_alloc. This does nothing
// for one that came from rls_static_alloc.
void rls_free(rls_t* rls);

#endif//RLS_H
