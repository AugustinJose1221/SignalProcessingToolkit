#ifndef LATTICE_H
#define LATTICE_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#else
#include "real.h"
#endif

// A filter built as a ladder of stages, each of which takes away what the
// stages before it already explained.
//
// The adaptive and rls modules both hold a straight list of coefficients and
// move all of them at once. THIS ONE IS BUILT DIFFERENTLY. Each stage holds
// one number, and its job is to take away from the signal whatever the stage
// before it could already account for. What comes out of the last stage is
// what none of them could explain.
//
// WHY THAT SHAPE IS WORTH HAVING, AND WHERE IT IS NOT
//
// A straight filter learns slowly when its input is not white, because the
// parts of the input that move together pull its coefficients about as a
// group. A ladder does not have that trouble: each stage sees only what the
// ones before it left behind, and each is divided by its own loudness, thus
// every stage learns at its own pace.
//
// THE MEASUREMENT IS MORE INTERESTING THAN THAT SUMMARY, AND IT IS RECORDED
// RATHER THAN THE SUMMARY. Twelve stages learning a response of three taps
// from an input that leans very heavily on itself, showing how far the error
// stands below the wanted signal:
//
//     samples          100     300    1000    3000   10000   30000
//     this module    -10.5   -23.0   -34.9   -43.0   -43.7   -40.0  dB
//     adaptive       -14.4   -21.3   -25.2   -63.6  -139.7  -139.5  dB
//     rls            -14.7   -24.4   -30.8   -41.9   -52.2   -58.1  dB
//
// READ IT ACROSS, NOT DOWN. There is a window, from about 300 samples to about
// 3000, where the ladder is ahead of both. Before it the ladder is still
// finding its stages. AFTER IT THE LADDER STOPS IMPROVING AND THE OTHERS DO
// NOT: it settles near -43 dB while the adaptive module walks on down past
// -139 dB.
//
// SO TAKE A LADDER WHERE THE MIDDLE IS ALL THERE IS. Where the thing being
// learned changes every few thousand samples, no filter ever reaches its floor
// and only that window matters. Where the thing stands still and there is
// time, the adaptive module ends up far ahead and costs less.
//
// WHY IT SETTLES WHERE IT DOES, AND HOW TO MOVE IT
//
// The stages never stop moving, and their movement keeps stirring what the
// weights are trying to settle on. The rate says how much they move, thus it
// trades how fast the filter settles against how low. Measured, on the same
// arrangement:
//
//     rate            1.00    0.50    0.20    0.05    0.01
//     at 1000        +24.5   -25.2   -27.0   -17.5   -13.2  dB
//     at 10000        +1.9   -42.0   -45.4   -34.5   -18.2  dB
//     at 100000      -38.5   -42.1   -45.9   -53.2   -60.3  dB
//
// A rate of 1 does not settle at all in any useful time. About 0.2 is the best
// of both for most work, and it is what to start from. Below that the floor
// keeps falling and the filter takes longer than most callers have.
//
// AND WHAT IT COSTS AGAINST rls
//
// rls arrives faster still, and it carries a whole square matrix to do it. A
// ladder carries a handful of numbers for each stage:
//
//     length     adaptive    this module    rls        at 32 bits
//     16          0.1 kB       0.4 kB       1.3 kB
//     64          0.5 kB       1.6 kB      17 kB
//     256         2 kB         6 kB       266 kB
//
// AND IT CANNOT FALL APART THE WAY rls CAN. There is no matrix to lose its
// footing: every stage holds one number, and the arithmetic holds that number
// between -1 and 1 rather than trusting it to stay there. An rls filter
// written carelessly runs away after a few thousand samples; a ladder has
// nothing to run away with.
//
// THE TWO ERRORS, AND WHY BOTH ARE OFFERED
//
// A stage can be asked what was left over BEFORE it changed itself for this
// sample, or AFTER. The two are the error a priori and the error a posteriori,
// and the difference is not a nicety:
//
//   THE ERROR A PRIORI is what the filter would have said if it had not been
//   about to learn. It is the honest measure of how well the filter is doing,
//   and it is the one to watch, to record, and to compare against another
//   filter.
//   THE ERROR A POSTERIORI is what is left after this sample has been learned
//   from. It is always the smaller of the two, and it is the one to USE where
//   the filter is taking something away, because it has already accounted for
//   the sample in hand.
//
// USING THE SECOND WHERE THE FIRST BELONGS IS HOW AN ADAPTIVE FILTER COMES TO
// LOOK BETTER THAN IT IS. A filter measured on its error a posteriori always
// reports a smaller error, because it has been told the answer first.

typedef struct{
    real_t* reflection;         // One number for each stage
    real_t* forward;            // What each stage could not explain, going on
    real_t* backward;           // The same, held back by one sample
    real_t* held;               // The backward errors of the sample before
    real_t* energy;             // How loud each stage has been
    real_t* weight;             // What each stage contributes to the answer
    uint32_t stages;            // How many stages
    real_t rate;                // How far each step moves
    real_t forgetting;          // How much of the past the energies keep
    real_t before;              // What was left over before this sample
    real_t after;               // What is left over after it
    bool designed;              // True once lattice_design has been called
    bool dynamic_alloc;         // True if the memory comes from the heap
}lattice_t;

// The smallest energy a stage will divide by, so that a silent input cannot
// make a step run away.
#define LATTICE_FLOOR           REAL_C(1.0e-10)

// The largest rate worth asking for.
//
// The steps of a ladder are already divided by the energy of what reaches
// them, thus a rate near 1 is not the runaway it would be in a plain filter.
// Above this the stages chase each other rather than settling.
#ifndef LATTICE_LARGEST_RATE
#define LATTICE_LARGEST_RATE    REAL_C(1.0)
#endif

// How far a reflection number may reach.
//
// A number outside this describes a stage giving out more than it was given,
// which no stage of a ladder can do. Holding it here rather than trusting it
// is why a ladder cannot run away as an rls filter can.
#ifndef LATTICE_LARGEST_REFLECTION
#define LATTICE_LARGEST_REFLECTION  REAL_C(0.99)
#endif

// True if this rate can be used, which is above nothing and not above
// LATTICE_LARGEST_RATE.
bool lattice_is_valid_rate(real_t rate);

// True if this forgetting factor can be used by the energies, which is above
// nothing and not above 1.
bool lattice_is_valid_forgetting(real_t forgetting);

// Give a ladder of the given number of stages. The memory comes from the heap.
// Give it to lattice_free when it is no longer needed.
lattice_t lattice_alloc(uint32_t stages);

// Give a ladder that uses the memory the caller holds, taking nothing from the
// heap. Each of the six lists must hold stages plus one values.
lattice_t lattice_static_alloc(uint32_t stages, real_t* reflection,
                               real_t* forward, real_t* backward,
                               real_t* held, real_t* energy, real_t* weight);

// Choose how far each step moves and how much of the past the energies keep.
//
// A rate of about 0.5 and a forgetting factor of about 0.99 suit most work.
// This also clears the ladder, thus it is where a run begins.
//
// Give false if either number is one the module cannot use.
bool lattice_design(lattice_t* lattice, real_t rate, real_t forgetting);

// Put one sample through the ladder and let every stage learn from it.
//
// The reference is what the ladder is given and the wanted value is what it
// should have produced. The answer is the error A PRIORI: what the ladder
// would have said before it learned anything from this sample. That is the
// honest measure, and it is what lattice_error gives as well.
real_t lattice_process_sample(lattice_t* lattice, real_t reference,
                              real_t wanted);

// What was left over before this sample was learned from.
//
// THIS IS THE ONE TO WATCH AND TO RECORD. It says how the filter is doing
// without being told the answer first.
real_t lattice_error_before(const lattice_t* lattice);

// What is left over after this sample has been learned from.
//
// THIS IS THE ONE TO USE where the filter is taking something away, because it
// has already accounted for the sample in hand. It is always the smaller of
// the two, thus it must never be reported as how well the filter is doing.
real_t lattice_error_after(const lattice_t* lattice);

// Give the reflection number of one stage, which says how much that stage
// found in common between what came forward and what was held back.
//
// Every one of them lies between -1 and 1. A stage whose number is near either
// end has found a great deal; one near nothing has found nothing, and the
// stages beyond it are doing no work.
real_t lattice_get_reflection(const lattice_t* lattice, uint32_t stage);

// Clear everything the ladder has learned.
void lattice_reset(lattice_t* lattice);

// Release the memory of a ladder that came from lattice_alloc. This does
// nothing for one that came from lattice_static_alloc.
void lattice_free(lattice_t* lattice);

#endif//LATTICE_H
