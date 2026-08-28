#ifndef ADAPTIVE_H
#define ADAPTIVE_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <sptk/core/real.h>
#include <sptk/core/ringbuf.h>
#else
#include "real.h"
#include "ringbuf.h"
#endif

// A filter that finds its own coefficients while it runs.
//
// Every other filter in this library is designed once and then applied. This
// one is given no design at all. It is given an answer to aim at, and it
// changes its own coefficients a little with every sample until it hits it.
//
// WHAT IT IS FOR
//
// TAKING AWAY NOISE THAT IS MEASURED SOMEWHERE ELSE. This is the use that
// matters most and the one a fixed filter cannot serve. A microphone near an
// engine, a coil near a transformer, a lead near a motor: in each case a
// second sensor sees the noise ALONE, without the signal. The noise reaches
// the first sensor changed in size and delayed, and by an amount nobody knows
// and which does not stay still.
//
// Give the noisy signal as what to aim at and the second sensor as the
// reference. The filter learns whatever turns one into the other and takes it
// away. WHAT IS LEFT OVER IS THE ANSWER, not what the filter gives out: the
// output is the noise it has learned, and the error is the signal with that
// noise gone.
//
// This works when no filter of frequency can, because the noise and the signal
// may hold exactly the same frequencies. What parts them is that the reference
// holds one and not the other.
//
// FOLLOWING SOMETHING THAT CHANGES. A fixed filter is right for the conditions
// it was designed for. This one follows.
//
// WHAT IT NEEDS, AND WHAT GOES WRONG WITHOUT IT
//
// THE REFERENCE MUST NOT HOLD THE SIGNAL. If it does, the filter learns to
// take the signal away as well, because that also makes the error smaller.
// This is the one way to use it that fails quietly: the error falls, everything
// looks well, and the answer has had the signal removed from it.
//
// THE RATE DECIDES EVERYTHING. Too high and the filter never settles but
// rattles around the answer, or runs away to nothing at all. Too low and it
// takes for ever to learn and cannot follow a change. adaptive_normalised is
// the answer to that, and the reason it usually wins.
//
// WHY adaptive_normalised IS THE ONE TO REACH FOR
//
// The plain rule moves each coefficient by the rate times the error times the
// reference. Thus how far it moves follows how LARGE the reference is, and a
// rate that settles for a quiet reference makes the filter run away for a loud
// one. The safe rate therefore depends on a signal that the designer has not
// heard yet.
//
// The normalised rule divides by the energy of what is in the filter now. The
// step then does not follow how loud the reference is, and a rate between 0
// and 2 is stable FOR ANY SIGNAL. Take 0.1 to 0.5 and it will work.

typedef enum{
    ADAPTIVE_PLAIN = 0,         // The rule of least mean squares
    ADAPTIVE_NORMALISED,        // The same, divided by the energy in the filter
    ADAPTIVE_SIGN               // Only the sign of the error is used
}adaptive_rule_t;

typedef struct{
    ringbuf_t history;          // The last samples of the reference
    real_t* coefficient;        // What the filter has learned so far
    uint32_t length;            // How many coefficients
    adaptive_rule_t rule;       // Which rule moves them
    real_t rate;                // How far each step moves
    real_t leak;                // How fast a coefficient falls back to nothing
    real_t energy;              // The energy of what is in the filter now
    bool dynamic_alloc;         // True if the memory comes from the heap
}adaptive_t;

// The smallest energy that the normalised rule will divide by, so that a
// silent reference cannot make the step run away.
#define ADAPTIVE_FLOOR      REAL_C(1.0e-10)

// True if the module knows this rule.
bool adaptive_is_valid_rule(adaptive_rule_t rule);

// Give a filter with the given number of coefficients, all of them nothing.
// The memory comes from the heap. Give it to adaptive_free when done.
//
// The length must cover the delay between the reference and the noise in the
// signal. A filter shorter than that delay can never learn the answer, however
// long it runs.
adaptive_t adaptive_alloc(uint32_t length);

// Give a filter that uses the memory of the caller. Both lists must hold as
// many values as the length. This function takes no memory from the heap.
adaptive_t adaptive_static_alloc(uint32_t length, real_t* coefficient,
                                 real_t* history);

// Choose the rule and the rate.
//
// For ADAPTIVE_NORMALISED the rate must lie between 0 and 2, and 0.1 to 0.5
// suits most work. For ADAPTIVE_PLAIN the rate that is safe depends on how
// large the reference is, and there is no number that suits every signal;
// that is why the normalised rule exists.
//
// Give false if the rule is unknown or the rate is not above zero.
bool adaptive_design(adaptive_t* adaptive, adaptive_rule_t rule, real_t rate);

// Set how fast a coefficient falls back towards nothing, from 0 to 1.
//
// With no leak, a coefficient that the reference never drives can drift for
// ever and hold whatever it drifted to. A small leak, say 0.0001, pulls every
// coefficient gently back to nothing, thus only what the reference keeps
// driving stays. Set it where the reference is quiet for long stretches.
//
// Give false if the leak is outside 0 to 1.
bool adaptive_set_leak(adaptive_t* adaptive, real_t leak);

// Forget everything that has been learned.
void adaptive_reset(adaptive_t* adaptive);

// Put one pair in and give what the filter makes of the reference.
//
// THIS IS THE NOISE, NOT THE ANSWER. Take adaptive_error for the answer.
real_t adaptive_process_sample(adaptive_t* adaptive, real_t reference,
                               real_t wanted);

// Put one pair in and give what is left when the filter has taken away what it
// could. THIS IS THE ANSWER for taking noise away.
real_t adaptive_error(adaptive_t* adaptive, real_t reference, real_t wanted);

// Run a whole block through, learning from every sample of it.
//
// The output takes what the filter made of the reference and the error takes
// what is left when that has been taken away. EITHER MAY BE NULL, and a caller
// that wants only one should give NULL for the other rather than a list it
// throws away.
//
// THE ERROR IS ALMOST ALWAYS THE ANSWER. The output is the interference as the
// filter learned it; the error is what remains, which is the thing that was
// wanted. A caller taking the output has taken the noise.
//
// The lists may not overlap the two inputs. Give false if the filter holds no
// coefficients.
bool adaptive_process_block(adaptive_t* adaptive, const real_t* reference,
                            const real_t* wanted, real_t* output,
                            real_t* error, uint32_t count);

// Give one coefficient that the filter has learned.
//
// Worth reading. The coefficients are the answer to what the path between the
// two sensors does, and where the largest one stands is the delay between them
// in samples.
real_t adaptive_get_coefficient(const adaptive_t* adaptive, uint32_t index);

// Give how much of the reference is in the filter now.
//
// This says whether the filter can learn at all. An energy near nothing means
// the reference is silent, thus there is nothing to learn from and whatever
// the filter holds is what it learned earlier.
real_t adaptive_get_energy(const adaptive_t* adaptive);

// Release the memory of a filter that came from adaptive_alloc. This function
// does nothing for one that came from adaptive_static_alloc.
void adaptive_free(adaptive_t* adaptive);

#endif//ADAPTIVE_H
