// A whole signal chain on a target where malloc does not exist.
//
// A microcontroller with a few kilobytes of memory and no allocator is the
// ordinary case in this trade, not the awkward one. Every module of this
// library that takes memory offers two roads to it: one from the heap, and one
// that uses memory the caller already holds. This example takes the second
// road for every stage of a real chain, and NOTHING BELOW CAN REACH THE HEAP.
//
// THE CHAIN. A vibration sensor is sampled at 8 kHz. The signal is filtered to
// keep the aliases out, taken down to 1 kHz where the interesting movement
// lives, held in a window so that a block can be looked at, and its level
// followed by a filter that knows the level cannot jump.
//
//     read -> fir -> resample by 8 -> ringbuf -> kalman
//
// WHAT IT COSTS IS KNOWN WHILE THE FILE IS COMPILED, and that is the point of
// the exercise. Every module says how much memory it needs through a macro, in
// values and not in bytes, and those macros are worked out by the compiler.
// The total printed below is a number a datasheet conversation can use, and
// nobody had to run the program to find it.
//
// HOW TO SEE THAT IT REALLY TAKES NOTHING. Every allocation below is a
// *_static_alloc, and grep will say so:
//
//     grep -n "_alloc" examples/noheap.c
//
// Every line it finds is a static one. There is no malloc, no free, and no
// call that reaches one.
//
// TO PORT THIS: replace read_sample with a read from your own input.

#include <examples/run_example.h>

#if (RUN_EXAMPLE == RUN_NOHEAP_EXAMPLE)

#include <ffitt/core/real.h>
#include <ffitt/core/ringbuf.h>
#include <ffitt/estimate/kalman.h>
#include <ffitt/filter/fir.h>
#include <ffitt/filter/resample.h>
#include <ffitt/linalg/matrix.h>

#include <math.h>
#include <stdio.h>

#define RATE            REAL_C(8000.0)
#define FACTOR          8u
#define FILTER_LENGTH   65u
#define WINDOW          128u
#define STATES          2u
#define MEASUREMENTS    1u
#define INPUTS          1u
#define SAMPLES         4096u
#define PI              REAL_C(3.14159265358979323846)

// ---------------------------------------------------------------------------
// EVERY PIECE OF MEMORY THE CHAIN USES, and all of it here where it can be
// counted. Each size comes from the module's own macro.
// ---------------------------------------------------------------------------
static real_t decimator_memory[RESAMPLE_DECIMATOR_MEMPOOL_SIZE(FILTER_LENGTH)];
static real_t window_memory[WINDOW];
static real_t kalman_memory[KALMAN_MEMPOOL_SIZE(INPUTS, STATES, MEASUREMENTS)];

// The matrices the filter is told about. A matrix on the caller's memory needs
// its own list, and these are the smallest in the chain.
static real_t state_memory[STATES * 1u];
static real_t transition_memory[STATES * STATES];
static real_t covariance_memory[STATES * STATES];
static real_t process_noise_memory[STATES * STATES];
static real_t measurement_noise_memory[MEASUREMENTS * MEASUREMENTS];
static real_t observation_memory[MEASUREMENTS * STATES];
static real_t control_memory[STATES * INPUTS];
static real_t input_memory[INPUTS * 1u];
static real_t measurement_memory[MEASUREMENTS * 1u];

#define TOTAL_VALUES                                                    \
    (RESAMPLE_DECIMATOR_MEMPOOL_SIZE(FILTER_LENGTH) + WINDOW            \
     + KALMAN_MEMPOOL_SIZE(INPUTS, STATES, MEASUREMENTS)                \
     + (STATES * 1u) + (STATES * STATES) + (STATES * STATES)            \
     + (STATES * STATES) + (MEASUREMENTS * MEASUREMENTS)                \
     + (MEASUREMENTS * STATES) + (STATES * INPUTS) + (INPUTS * 1u)      \
     + (MEASUREMENTS * 1u))

// ---------------------------------------------------------------------------
// Replace this function with a read from your own input.
//
// It stands for a machine whose vibration grows slowly over the run, with a
// tone at 120 hertz and noise on top.
// ---------------------------------------------------------------------------
static real_t read_sample(uint32_t index)
{
    real_t time = (real_t)index / RATE;
    real_t growing = REAL_C(1.0) + (REAL_C(2.0) * (real_t)index
                                    / (real_t)SAMPLES);

    return (growing * REAL_SIN(REAL_C(2.0) * PI * REAL_C(120.0) * time))
           + (REAL_C(0.2) * REAL_SIN((real_t)index * REAL_C(12.9898)));
}

int main(void)
{
    // Every one of these is a static road. None of them can fail for want of
    // memory, because none of them asks for any.
    resample_t decimator = resample_static_alloc_decimator(FACTOR,
                                                           FILTER_LENGTH,
                                                           decimator_memory);
    ringbuf_t window = ringbuf_static_alloc(WINDOW, window_memory);
    kalman_t kalman = kalman_static_alloc(INPUTS, STATES, MEASUREMENTS,
                                          kalman_memory);

    matrix_t state = matrix_static_alloc(STATES, 1u, state_memory);
    matrix_t transition = matrix_static_alloc(STATES, STATES,
                                              transition_memory);
    matrix_t covariance = matrix_static_alloc(STATES, STATES,
                                              covariance_memory);
    matrix_t process_noise = matrix_static_alloc(STATES, STATES,
                                                 process_noise_memory);
    matrix_t measurement_noise = matrix_static_alloc(MEASUREMENTS,
                                                     MEASUREMENTS,
                                                     measurement_noise_memory);
    matrix_t observation = matrix_static_alloc(MEASUREMENTS, STATES,
                                               observation_memory);
    matrix_t control = matrix_static_alloc(STATES, INPUTS, control_memory);
    matrix_t input = matrix_static_alloc(INPUTS, 1u, input_memory);
    matrix_t measurement = matrix_static_alloc(MEASUREMENTS, 1u,
                                               measurement_memory);

    // A level that walks, which is what a machine warming up or wearing does.
    // The state is the level and how fast it is moving.
    matrix_set_zero(&state);
    matrix_set_zero(&control);
    matrix_set_zero(&input);
    matrix_set_unit(&transition);
    matrix_add_element(&transition, 0u, 0u, REAL_C(1.0));
    matrix_add_element(&transition, 0u, 1u, REAL_C(1.0));
    matrix_set_unit(&covariance);
    matrix_set_zero(&process_noise);
    matrix_add_element(&process_noise, 1u, 1u, REAL_C(0.0001));
    matrix_set_unit(&measurement_noise);
    matrix_set_zero(&observation);
    matrix_add_element(&observation, 0u, 0u, REAL_C(1.0));

    kalman_set_state_matrix(&kalman, &state);
    kalman_set_state_transition_matrix(&kalman, &transition);
    kalman_set_control_matrix(&kalman, &control);
    kalman_set_input_matrix(&kalman, &input);
    kalman_set_covariance_matrix(&kalman, &covariance);
    kalman_set_process_noise_covariance_matrix(&kalman, &process_noise);
    kalman_set_measurement_covariance_matrix(&kalman, &measurement_noise);
    kalman_set_observation_matrix(&kalman, &observation);

    printf("A chain on a target with no allocator.\n\n");
    printf("  read at %.0f Hz\n", (double)RATE);
    printf("  -> a filter of %u coefficients\n", FILTER_LENGTH);
    printf("  -> down by %u, to %.0f Hz\n", FACTOR, (double)(RATE / FACTOR));
    printf("  -> a window of %u samples\n", WINDOW);
    printf("  -> a filter of Kalman over %u states\n\n", STATES);

    printf("WHAT IT HOLDS, worked out while this file was compiled:\n\n");
    printf("  the decimator        %5u values\n",
           (unsigned)RESAMPLE_DECIMATOR_MEMPOOL_SIZE(FILTER_LENGTH));
    printf("  the window           %5u values\n", (unsigned)WINDOW);
    printf("  the filter of Kalman %5u values\n",
           (unsigned)KALMAN_MEMPOOL_SIZE(INPUTS, STATES, MEASUREMENTS));
    printf("  its matrices         %5u values\n",
           (unsigned)(TOTAL_VALUES
                      - RESAMPLE_DECIMATOR_MEMPOOL_SIZE(FILTER_LENGTH)
                      - WINDOW
                      - KALMAN_MEMPOOL_SIZE(INPUTS, STATES, MEASUREMENTS)));
    printf("  ----------------------------------\n");
    printf("  in all               %5u values, which is %u bytes at this\n",
           (unsigned)TOTAL_VALUES,
           (unsigned)(TOTAL_VALUES * sizeof(real_t)));
    printf("                             width, and %u at the other.\n\n",
           (unsigned)(TOTAL_VALUES * ((sizeof(real_t) == 4u) ? 8u : 4u)));

    // The chain itself.
    uint32_t out_count = 0;
    real_t decimated = REAL_C(0.0);
    real_t first_level = REAL_C(0.0);
    real_t last_level = REAL_C(0.0);

    for(uint32_t index = 0; index < SAMPLES; index++)
    {
        if(resample_decimate(&decimator, read_sample(index), &decimated))
        {
            ringbuf_put(&window, decimated);

            // The level the filter follows is the size of the swing, thus the
            // reading it is given is the size and not the sample.
            matrix_add_element(&measurement, 0u, 0u,
                               REAL_ABS(decimated));

            (void)kalman_step(&kalman, NULL, &measurement);

            matrix_t* held = kalman_get_state_matrix(&kalman);
            last_level = matrix_get_element(held, 0u, 0u);

            if(out_count == 0u)
            {
                first_level = last_level;
            }

            out_count++;
        }
    }

    printf("%u samples in, %u out at the lower rate.\n", SAMPLES, out_count);
    printf("The level the filter follows moved from %.3f to %.3f, which is\n",
           (double)first_level, (double)last_level);
    printf("the machine getting louder over the run.\n\n");
    printf("The window holds the last %u of those, ready for a transform or a\n",
           ringbuf_count(&window));
    printf("median, and it too came from memory declared above.\n");

    return 0;
}

#endif//RUN_EXAMPLE
