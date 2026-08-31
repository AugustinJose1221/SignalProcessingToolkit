#ifndef PROPAGATE_H
#define PROPAGATE_H

#include <stdint.h>
#include <stdbool.h>

#ifndef TEST
#include <ffitt/core/real.h>
#else
#include "real.h"
#endif

// Carry a state forward through a model that is written as a RATE OF CHANGE.
//
// THE GAP THIS FILLS
//
// The kalman, ekf and ukf modules all ask for a function that takes the state
// now and gives the state at the next sample. But nobody writes a model that
// way. A model of anything physical is written as how fast each thing is
// changing: a temperature falls at a rate that follows how far above the room
// it is, a wheel slows at a rate that follows how fast it is turning, a
// pendulum turns back at a rate that follows how far over it leans.
//
// Turning one into the other is what this module does, and until now every
// caller with a model of that kind had to do it by hand.
//
// WHICH METHOD TO USE, AND WHY IT IS NOT A MATTER OF TASTE
//
// All three take the rate of change and step forward with it. They differ in
// how many times they ask for the rate along the way, and that decides how the
// error falls as the step is made smaller:
//
//   PROPAGATE_EULER      one ask.    Halve the step and the error HALVES.
//   PROPAGATE_MIDPOINT   two asks.   Halve the step and the error QUARTERS.
//   PROPAGATE_RUNGE      four asks.  Halve the step and the error falls to a
//                                    SIXTEENTH.
//
// Measured, on a turning that has a known answer, the worst the state is out
// by across one second, at 64 bits:
//
//     step            0.1        0.05       0.025      0.0125
//     euler        5.1e-02     2.5e-02     1.3e-02     6.3e-03
//     midpoint     1.7e-03     4.2e-04     1.0e-04     2.6e-05
//     runge        8.3e-07     5.2e-08     3.3e-09     2.0e-10
//
// READ ALONG EACH ROW. Euler halves, midpoint quarters, Runge falls to a
// sixteenth, exactly. That is what the order of a method MEANS, and it is why
// the four asks of Runge are not four times the cost of Euler but a million
// times the accuracy.
//
// AND AT 32 BITS THE METHOD OUTRUNS THE WIDTH, which is worth knowing before
// choosing a step. The same measurement:
//
//     step            0.1        0.05       0.025      0.0125
//     euler        5.1e-02     2.5e-02     1.3e-02     6.3e-03
//     midpoint     1.7e-03     4.2e-04     1.0e-04     2.6e-05
//     runge        8.5e-07     1.4e-07     2.3e-07     1.2e-07
//
// Euler and midpoint fall exactly as before. RUNGE STOPS AT ABOUT A PART IN
// TEN MILLION AND GOES NO FURTHER, because by then the error of the method is
// below the rounding of the state itself and halving the step only adds more
// roundings. There is nothing to be gained by a smaller step than that, and a
// little to be lost.
//
// TAKE PROPAGATE_RUNGE unless there is a reason not to. Its four asks cost
// four times as much for each step, and it reaches a given accuracy with such
// larger steps that it is cheaper in the end for anything but the roughest
// work.
//
// TAKE PROPAGATE_EULER where the rate is very cheap to work out, the step is
// already small because the sample rate is high, and the model is nearly
// straight anyway. A filter running at 1000 samples in a second is taking
// steps of a millisecond, and at that size Euler is often enough.
//
// WHAT THIS MODULE DOES NOT DO
//
// IT DOES NOT CHOOSE THE STEP FOR YOU. Every method here takes the step it is
// given and takes it once. A model whose rate changes sharply within one step
// will be carried badly however good the method, and no warning is given
// because none can be: the module never sees the true answer.
//
// Where the sample rate is fixed by the measurements, as it is for every
// filter in the estimate area, split one sample interval into several steps
// with propagate_state_over rather than taking one large step.

// The most states a model may hold.
//
// The methods keep a few copies of the state on the stack, so that no memory
// is taken and none is asked of the caller. This is what bounds those copies.
#ifndef PROPAGATE_LARGEST_STATE
#define PROPAGATE_LARGEST_STATE     16u
#endif

// Which method carries the state forward.
typedef enum{
    // One ask for the rate. The error halves when the step halves.
    PROPAGATE_EULER = 0,

    // Two asks. The error quarters when the step halves.
    PROPAGATE_MIDPOINT,

    // Four asks, by the method of Runge and Kutta. The error falls to a
    // sixteenth when the step halves, and this is what to use.
    PROPAGATE_RUNGE
}propagate_method_t;

// How fast each state is changing, given where the state is now and what is
// being put in.
//
// The rate list holds one value for each state and the function fills it. The
// input may be NULL where the model has nothing put into it.
//
// THE TIME IS GIVEN because the methods ask for the rate part way through a
// step, and a model that follows the clock must know where in the step it is
// being asked about.
typedef void (*propagate_rate_t)(real_t time, const real_t* state,
                                 const real_t* input, real_t* rate,
                                 uint32_t count);

// True if the method is one this module knows.
bool propagate_is_valid_method(propagate_method_t method);

// True if a model of this many states can be carried.
bool propagate_is_valid_count(uint32_t count);

// Carry the state forward by one step.
//
// The state holds count values and is written over with where it has got to.
// The time is where the step begins, and the step is how far to go.
//
// Give false if the method or the count is one the module cannot take, or if
// the step is not above nothing.
bool propagate_state(propagate_method_t method, propagate_rate_t rate,
                     real_t time, real_t step, real_t* state,
                     const real_t* input, uint32_t count);

// Carry the state forward across a stretch, in several steps of equal size.
//
// USE THIS TO REACH THE NEXT MEASUREMENT. The sample rate of a filter fixes
// how far apart the measurements are, and that distance is often far too large
// for one step. Splitting it costs the same as one step of the same total size
// would have cost had the method been asked for it, and gives an answer worth
// having.
//
// Give false for the same reasons as propagate_state, or if the number of
// steps is nothing.
bool propagate_state_over(propagate_method_t method, propagate_rate_t rate,
                          real_t time, real_t across, uint32_t steps,
                          real_t* state, const real_t* input, uint32_t count);

// How many asks for the rate a method makes for each step.
//
// Use it to weigh one method against another: a method of four asks at a step
// of 0.1 costs the same as one of two asks at a step of 0.05, and the table in
// the header says which of those two gives the better answer.
uint32_t propagate_asks_for_each_step(propagate_method_t method);

#endif//PROPAGATE_H
