#include "unity.h"
#include "real_assert.h"
#include "cepstrum.h"
#include "correlate.h"
#include "peakdetect.h"
#include "fft.h"
#include "window.h"
#include "cnum.h"
#include <math.h>

#define N       1024u
#define PI      REAL_C(3.14159265358979323846)
#define PERIOD  REAL_C(64.0)

static real_t given[N];
static real_t found[N];
static real_t work[N];
static uint32_t seed;

void setUp(void)
{
    seed = 1u;
}

void tearDown(void)
{

}

static real_t rough(void)
{
    seed = (seed * 1103515245u) + 12345u;

    return ((real_t)((seed >> 16u) % 2000u) / REAL_C(1000.0)) - REAL_C(1.0);
}

// A note of the given harmonics, whose true period is 64 samples.
static void build_note(uint32_t from, uint32_t upto, real_t noise)
{
    seed = 1u;

    for(uint32_t index = 0; index < N; index++)
    {
        given[index] = noise * rough();

        for(uint32_t harmonic = from; harmonic <= upto; harmonic++)
        {
            given[index] += (REAL_C(1.0) / (real_t)harmonic)
                            * REAL_SIN(REAL_C(2.0) * PI * (real_t)harmonic
                                       * (real_t)index / PERIOD);
        }
    }
}

void test_cepstrum_is_valid_size(void)
{
    // Whatever the transform can take, because it is taken twice.
    TEST_ASSERT_EQUAL(true, cepstrum_is_valid_size(256u));
    TEST_ASSERT_EQUAL(fft_is_valid_size(100u), cepstrum_is_valid_size(100u));
    TEST_ASSERT_EQUAL(false, cepstrum_is_valid_size(0u));
}

// THE REASON THE MODULE EXISTS. A row of evenly spaced peaks in the spectrum
// comes out as one peak here, and where it stands is the period of the note.
void test_cepstrum_finds_the_period_of_a_note(void)
{
    cepstrum_t cepstrum = cepstrum_alloc(N);
    real_t strength = REAL_C(0.0);

    build_note(1u, 12u, REAL_C(0.0));

    TEST_ASSERT_EQUAL(true, cepstrum_real(&cepstrum, given, found));

    TEST_ASSERT_EQUAL(64u, cepstrum_best_quefrency(found, N, 20u, 300u,
                                                   &strength));
    TEST_ASSERT_TRUE(strength > REAL_C(0.2));

    cepstrum_free(&cepstrum);
}

// THE ONE CORRELATION CANNOT DO. A small loudspeaker cannot make the lowest
// note it is asked for, thus the note arrives with its fundamental missing. The
// ear still hears it, and so does this.
void test_cepstrum_finds_a_note_whose_fundamental_is_missing(void)
{
    cepstrum_t cepstrum = cepstrum_alloc(N);
    real_t strength = REAL_C(0.0);
    real_t other = REAL_C(0.0);

    build_note(2u, 12u, REAL_C(0.0));

    TEST_ASSERT_EQUAL(true, cepstrum_real(&cepstrum, given, found));

    TEST_ASSERT_EQUAL(64u, cepstrum_best_quefrency(found, N, 20u, 300u,
                                                   &strength));
    TEST_ASSERT_TRUE(strength > REAL_C(0.2));

    // What correlation says about the same signal, which is not 64. It is not
    // wrong about the signal: a note built of harmonics really does repeat at a
    // multiple of its period as well. It is answering a different question,
    // and it says so with full confidence, which is the trap.
    uint32_t by_correlation = correlate_best_lag(given, N, work, 20u, 300u,
                                                 &other);

    TEST_ASSERT_TRUE(by_correlation != 64u);
    TEST_ASSERT_TRUE(other > REAL_C(0.9));

    cepstrum_free(&cepstrum);
}

// THE WINDOW IS WHAT MAKES IT WORK UNDER NOISE, and it was written once
// without one. A note whose period divides the block needs no window; the NOISE
// on that note does not divide the block, leaks across every bin, and its
// leakage has strong structure in the logarithm. Measured without a window, a
// note with no fundamental under a twentieth of noise came back at 255 where 64
// was right, and moved about with the floor and with the width of the build.
void test_cepstrum_holds_its_answer_under_noise(void)
{
    cepstrum_t cepstrum = cepstrum_alloc(N);
    real_t noises[3] = {REAL_C(0.0), REAL_C(0.01), REAL_C(0.05)};

    for(uint32_t which = 0; which < 3u; which++)
    {
        real_t strength = REAL_C(0.0);

        build_note(2u, 12u, noises[which]);

        TEST_ASSERT_EQUAL(true, cepstrum_real(&cepstrum, given, found));
        TEST_ASSERT_EQUAL(64u, cepstrum_best_quefrency(found, N, 20u, 300u,
                                                       &strength));
        TEST_ASSERT_TRUE(strength > REAL_C(0.15));
    }

    cepstrum_free(&cepstrum);
}

// THE OTHER THING IT FINDS. A sound and the same sound again a little later
// multiply the spectrum by a ripple, and a ripple in the spectrum is a peak.
void test_cepstrum_finds_an_echo(void)
{
    cepstrum_t cepstrum = cepstrum_alloc(N);
    real_t strength = REAL_C(0.0);
    static real_t burst[N];

    seed = 1u;

    for(uint32_t index = 0; index < N; index++)
    {
        burst[index] = rough();
    }

    for(uint32_t index = 0; index < N; index++)
    {
        given[index] = burst[index];

        if(index >= 100u)
        {
            given[index] += REAL_C(0.7) * burst[index - 100u];
        }
    }

    TEST_ASSERT_EQUAL(true, cepstrum_real(&cepstrum, given, found));

    TEST_ASSERT_EQUAL(100u, cepstrum_best_quefrency(found, N, 20u, 300u,
                                                    &strength));
    TEST_ASSERT_TRUE(strength > REAL_C(0.15));

    cepstrum_free(&cepstrum);
}

// THE ANSWER IS A WHOLE NUMBER OF SAMPLES AND THE TRUE PERIOD RARELY IS. That
// is the quefrency axis being sampled and not an error of the method, and
// peakdetect_refine is what gets between the samples.
void test_cepstrum_a_period_between_samples_comes_back_within_one(void)
{
    cepstrum_t cepstrum = cepstrum_alloc(N);
    real_t periods[2] = {REAL_C(100.0), REAL_C(40.0)};

    for(uint32_t which = 0; which < 2u; which++)
    {
        real_t strength = REAL_C(0.0);

        seed = 1u;

        for(uint32_t index = 0; index < N; index++)
        {
            given[index] = REAL_C(0.05) * rough();

            for(uint32_t harmonic = 1u; harmonic <= 12u; harmonic++)
            {
                given[index] += (REAL_C(1.0) / (real_t)harmonic)
                                * REAL_SIN(REAL_C(2.0) * PI * (real_t)harmonic
                                           * (real_t)index / periods[which]);
            }
        }

        TEST_ASSERT_EQUAL(true, cepstrum_real(&cepstrum, given, found));

        uint32_t said = cepstrum_best_quefrency(found, N, 20u, 300u,
                                                &strength);

        // Within one sample of the truth, which is all a whole number can be.
        TEST_ASSERT_TRUE(REAL_ABS((real_t)said - periods[which])
                         <= REAL_C(1.0));

        // And refining the peak gets between the samples.
        real_t within = peakdetect_refine(found, N, said);

        TEST_ASSERT_TRUE(REAL_ABS(within) <= REAL_C(0.5));
    }

    cepstrum_free(&cepstrum);
}

// THE STRENGTH TELLS STRUCTURE FROM NOISE, and the header is honest that it
// does not tell a single tone from a note.
void test_cepstrum_the_strength_tells_structure_from_noise(void)
{
    cepstrum_t cepstrum = cepstrum_alloc(N);
    real_t note_strength = REAL_C(0.0);
    real_t noise_strength = REAL_C(0.0);

    build_note(1u, 12u, REAL_C(0.0));
    TEST_ASSERT_EQUAL(true, cepstrum_real(&cepstrum, given, found));
    cepstrum_best_quefrency(found, N, 20u, 300u, &note_strength);

    seed = 3u;

    for(uint32_t index = 0; index < N; index++)
    {
        given[index] = rough();
    }

    TEST_ASSERT_EQUAL(true, cepstrum_real(&cepstrum, given, found));
    cepstrum_best_quefrency(found, N, 20u, 300u, &noise_strength);

    // A block of noise still gives an answer, and the strength says not to
    // believe it: about 0.06 against about 0.7 for a real note.
    TEST_ASSERT_TRUE(noise_strength < REAL_C(0.15));
    TEST_ASSERT_TRUE(note_strength > (REAL_C(3.0) * noise_strength));

    cepstrum_free(&cepstrum);
}

// The search starts past the first few places on purpose: they hold the SHAPE
// of the spectrum, which is always large and always there.
void test_cepstrum_best_quefrency_refuses_a_range_that_does_not_fit(void)
{
    real_t strength = REAL_C(7.0);

    for(uint32_t index = 0; index < N; index++)
    {
        found[index] = (real_t)index;
    }

    TEST_ASSERT_EQUAL(0u, cepstrum_best_quefrency(found, N, 0u, 100u,
                                                  &strength));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0), strength);

    // The second half is the mirror of the first and holds nothing new.
    TEST_ASSERT_EQUAL(0u, cepstrum_best_quefrency(found, N, 10u, N, NULL));
    TEST_ASSERT_EQUAL(0u, cepstrum_best_quefrency(found, N, 100u, 50u, NULL));
}

void test_cepstrum_static_alloc_takes_no_memory_from_the_heap(void)
{
    static cnum_t work_room[256u];
    static real_t window_room[256u];
    static real_t windowed_room[256u];
    static real_t small[256u];
    static real_t answer[256u];

    fft_t fft = fft_alloc(256u);

    cepstrum_t cepstrum = cepstrum_static_alloc(256u, work_room, window_room,
                                                windowed_room, fft);

    TEST_ASSERT_EQUAL(false, cepstrum.dynamic_alloc);

    for(uint32_t index = 0; index < 256u; index++)
    {
        small[index] = REAL_C(0.0);

        for(uint32_t harmonic = 1u; harmonic <= 6u; harmonic++)
        {
            small[index] += (REAL_C(1.0) / (real_t)harmonic)
                            * REAL_SIN(REAL_C(2.0) * PI * (real_t)harmonic
                                       * (real_t)index / REAL_C(32.0));
        }
    }

    TEST_ASSERT_EQUAL(true, cepstrum_real(&cepstrum, small, answer));
    TEST_ASSERT_EQUAL(32u, cepstrum_best_quefrency(answer, 256u, 8u, 100u,
                                                   NULL));

    cepstrum_free(&cepstrum);
    fft_free(&fft);
}
