// Follow a state whose model is written as a RATE OF CHANGE.
//
// Every estimator in this library asks for a function that takes the state now
// and gives the state at the next sample. NOBODY WRITES A MODEL THAT WAY. A
// model of anything physical is written as how fast each thing is changing:
//
//   a temperature falls at a rate that follows how far above the room it is
//   a wheel slows at a rate that follows how fast it is turning
//   a pendulum turns back at a rate that follows how far over it leans
//
// Until the propagate module existed, every caller with a model of that kind
// had to turn it into a step by hand, and the usual way of doing that by hand
// is the method of Euler, which is the worst of the three the module offers.
//
// THIS EXAMPLE IS A PENDULUM, which is the plainest model that a straight line
// cannot follow: the rate at which it turns back follows the SINE of how far
// over it leans, and a sine is not a straight line once the swing is wide.
//
// The measurement is the angle only, and it is noisy. The rate is never
// measured at all, and the filter has to work it out from how the angle moves.
//
// WHAT TO LOOK AT. The same filter is run three times, differing only in how
// the model is carried forward between measurements. The method of Euler
// carries it badly, and the filter then spends its effort correcting a model
// that was wrong rather than a measurement that was noisy.
//
// TO PORT THIS: replace pendulum_rate with your own rates, and keep
// propagate_state_over between measurements with PROPAGATE_RUNGE.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_CONTINUOUS_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/estimate/propagate.h>
#include <ffitt/estimate/ukf.h>
#include <ffitt/linalg/matrix.h>
#include <math.h>
#include <stdio.h>

#define RATE            REAL_C(20.0)
#define STEP            (REAL_C(1.0) / RATE)
#define SAMPLES         200u
#define SPLIT           4u

// How much of the swing the model loses to friction each second.
#define DRAG            REAL_C(0.2)

// How fast the pendulum wants to swing, which sets the sine.
#define PULL            REAL_C(9.0)

static propagate_method_t chosen_method;

// THE MODEL, WRITTEN THE WAY A MODEL IS REALLY WRITTEN. The angle changes at
// the rate the pendulum is turning; the rate changes as the pull brings it
// back and the drag slows it.
static void pendulum_rate(real_t time, const real_t* state,
                          const real_t* input, real_t* rate, uint32_t count)
{
    (void)time;
    (void)input;
    (void)count;

    rate[0] = state[1];
    rate[1] = (-PULL * REAL_SIN(state[0])) - (DRAG * state[1]);
}

// What the filter is given: carry the state forward by one sample interval.
static void step_forward(const matrix_t* state, const matrix_t* input,
                         matrix_t* result)
{
    (void)input;

    real_t held[2];

    held[0] = matrix_get_element((matrix_t*)state, 0, 0);
    held[1] = matrix_get_element((matrix_t*)state, 1, 0);

    // ONE SAMPLE INTERVAL IS TOO FAR FOR ONE STEP, thus it is split. Splitting
    // costs the same as one step of the same total size would have cost had
    // the method been asked for it, and gives an answer worth having.
    propagate_state_over(chosen_method, pendulum_rate, REAL_C(0.0), STEP,
                         SPLIT, held, NULL, 2u);

    matrix_add_element(result, 0, 0, held[0]);
    matrix_add_element(result, 1, 0, held[1]);
}

// What is measured: the angle, and nothing else.
static void measure(const matrix_t* state, matrix_t* result)
{
    matrix_add_element(result, 0, 0,
                       matrix_get_element((matrix_t*)state, 0, 0));
}

static uint32_t seed = 5150u;

static real_t noise(void)
{
    seed = (seed * 1103515245u) + 12345u;

    return ((real_t)((seed >> 16u) % 20000u) / REAL_C(10000.0)) - REAL_C(1.0);
}

// Run the filter with one method and give how far the rate it worked out
// stands from the true rate, which is the part never measured at all.
static real_t follow_with(propagate_method_t method, const real_t* true_angle,
                          const real_t* true_rate, const real_t* measured)
{
    chosen_method = method;

    ukf_t ukf = ukf_alloc(1, 2, 1);

    matrix_t state = matrix_create_zero_matrix(2, 1);
    matrix_add_element(&state, 0, 0, REAL_C(0.5));

    matrix_t covariance = matrix_create_unit_matrix(2);
    matrix_multiply_scalar_into(&covariance, REAL_C(0.5), &covariance);

    matrix_t process = matrix_create_unit_matrix(2);
    matrix_multiply_scalar_into(&process, REAL_C(0.0001), &process);

    matrix_t measurement = matrix_create_unit_matrix(1);
    matrix_multiply_scalar_into(&measurement, REAL_C(0.01), &measurement);

    ukf_set_state_matrix(&ukf, &state);
    ukf_set_covariance_matrix(&ukf, &covariance);
    ukf_set_process_noise_covariance_matrix(&ukf, &process);
    ukf_set_measurement_covariance_matrix(&ukf, &measurement);
    ukf_set_state_function(&ukf, step_forward);
    ukf_set_measurement_function(&ukf, measure);

    matrix_t reading = matrix_create_zero_matrix(1, 1);
    matrix_t nothing = matrix_create_zero_matrix(1, 1);

    real_t worst = REAL_C(0.0);

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        matrix_add_element(&reading, 0, 0, measured[index]);

        ukf_step(&ukf, &nothing, &reading);

        const matrix_t* now = ukf_get_state_matrix(&ukf);

        if(index > (SAMPLES / 4u))
        {
            real_t apart = REAL_ABS(matrix_get_element((matrix_t*)now, 1, 0)
                                    - true_rate[index]);

            if(apart > worst) { worst = apart; }
        }
    }

    (void)true_angle;

    matrix_free(&state);
    matrix_free(&covariance);
    matrix_free(&process);
    matrix_free(&measurement);
    matrix_free(&reading);
    matrix_free(&nothing);
    ukf_free(&ukf);

    return worst;
}

int main(void)
{
    static real_t true_angle[SAMPLES];
    static real_t true_rate[SAMPLES];
    static real_t measured[SAMPLES];

    // The truth, carried forward finely enough that it IS the truth.
    real_t state[2] = {REAL_C(0.8), REAL_C(0.0)};

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        true_angle[index] = state[0];
        true_rate[index] = state[1];
        measured[index] = state[0] + (REAL_C(0.1) * noise());

        propagate_state_over(PROPAGATE_RUNGE, pendulum_rate, REAL_C(0.0),
                             STEP, 100u, state, NULL, 2u);
    }

    printf("A pendulum, swinging wide enough that a straight line cannot\n"
           "follow it. The angle is measured %.0f times a second and is\n"
           "noisy; the rate it turns at is never measured at all.\n\n",
           (double)RATE);

    printf("The same filter three times, differing only in how the model is\n"
           "carried between measurements. Each sample interval is split into\n"
           "%u steps.\n\n", SPLIT);

    printf("  %-12s %8s %20s\n", "method", "asks", "worst the rate is out by");

    const char* names[3] = {"euler", "midpoint", "runge"};

    for(uint32_t which = 0; which < 3u; which++)
    {
        propagate_method_t method = (propagate_method_t)which;

        printf("  %-12s %8u %20.5f\n", names[which],
               propagate_asks_for_each_step(method) * SPLIT,
               (double)follow_with(method, true_angle, true_rate, measured));
    }

    printf("\nThe rate is the part that is never measured. A filter can only\n"
           "work it out from how the angle moves AND from what the model says\n"
           "should happen next, thus a model carried badly shows up there\n"
           "first and worst. The method of Euler is what a caller writes by\n"
           "hand when this module is not there, and it is twice as far out.\n");

    printf("\nBUT READ THE LAST TWO ROWS AGAIN. Midpoint and Runge are the\n"
           "same here, though Runge asks for the rate twice as often. That is\n"
           "not a fault in the measurement: at this step the error of the\n"
           "midpoint method has already fallen below the noise on the\n"
           "measurements, and NOTHING BELOW THAT NOISE CAN HELP.\n");
    printf("\nSo the rule is not to reach for the best method every time. It\n"
           "is to carry the model well enough that the model is not the worst\n"
           "thing in the answer, and then to stop. Make the step smaller or\n"
           "the method better until the number above stops improving, and use\n"
           "the cheapest setting that got there.\n");

    return 0;
}

#endif
