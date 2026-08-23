#include "unity.h"
#include "real_assert.h"
#include "resample.h"
#include "fir.h"
#include "ringbuf.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.0001)
#define PI          REAL_C(3.14159265358979323846)

void setUp(void)
{

}

void tearDown(void)
{

}

// The largest output of a decimator once its filter has settled, for a tone at
// the given frequency as a part of the INPUT rate.
static real_t decimated_size(uint32_t factor, uint32_t length, real_t frequency)
{
    resample_t resample = resample_alloc_decimator(factor, length);
    real_t largest = REAL_C(0.0);
    uint32_t made = 0;

    for(uint32_t index = 0; index < 40000u; index++)
    {
        real_t sample = REAL_SIN(REAL_C(2.0) * PI * frequency * (real_t)index);
        real_t output;

        if(resample_decimate(&resample, sample, &output))
        {
            made++;
            if((made > ((length / factor) + 200u))
               && (REAL_ABS(output) > largest))
            {
                largest = REAL_ABS(output);
            }
        }
    }

    resample_free(&resample);

    return largest;
}

void test_resample_is_valid_factor(void)
{
    TEST_ASSERT_EQUAL(true, resample_is_valid_factor(2));
    TEST_ASSERT_EQUAL(true, resample_is_valid_factor(64));
    // A factor of one changes nothing and a factor of nothing means nothing.
    TEST_ASSERT_EQUAL(false, resample_is_valid_factor(1));
    TEST_ASSERT_EQUAL(false, resample_is_valid_factor(0));
}

void test_resample_advised_length_is_always_odd(void)
{
    // An odd length gives the filter a middle coefficient, thus it delays
    // every frequency by the same time.
    for(uint32_t factor = 2; factor <= 20u; factor++)
    {
        uint32_t length = resample_advised_length(factor);
        TEST_ASSERT_EQUAL(1, length % 2u);
        // A larger factor puts the two edges closer together, thus it needs a
        // longer filter.
        TEST_ASSERT_TRUE(length > resample_advised_length(factor - 1u));
    }

    TEST_ASSERT_EQUAL(0, resample_advised_length(1));
    TEST_ASSERT_EQUAL(0, resample_advised_length(0));
}

void test_a_decimator_keeps_what_is_below_the_new_rate(void)
{
    uint32_t length = resample_advised_length(4);

    // Well inside the new band, the size comes through whole.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(1.0),
                            decimated_size(4, length, REAL_C(0.02)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(1.0),
                            decimated_size(4, length, REAL_C(0.05)));
    // Near the edge it has begun to fall, but not by much.
    TEST_ASSERT_TRUE(decimated_size(4, length, REAL_C(0.10)) > REAL_C(0.9));
}

void test_a_decimator_stops_what_would_come_back_as_a_false_tone(void)
{
    // THE WHOLE REASON THIS MODULE EXISTS.
    //
    // Decimating by 4 makes the new Nyquist 0.125 of the input rate. A tone at
    // 0.3 is far above that. If the samples were simply thrown away it would
    // not disappear: it would come back at 0.2 of the new rate, looking
    // exactly like a signal, and nothing afterwards could find out.
    uint32_t length = resample_advised_length(4);

    TEST_ASSERT_TRUE(decimated_size(4, length, REAL_C(0.125)) < REAL_C(0.01));
    TEST_ASSERT_TRUE(decimated_size(4, length, REAL_C(0.15)) < REAL_C(0.01));
    TEST_ASSERT_TRUE(decimated_size(4, length, REAL_C(0.20)) < REAL_C(0.01));
    TEST_ASSERT_TRUE(decimated_size(4, length, REAL_C(0.30)) < REAL_C(0.01));
}

void test_throwing_the_samples_away_without_a_filter_makes_a_false_tone(void)
{
    // The other half of the same story, to show that the danger is real and
    // not a claim in a comment. The same tone at 0.3, kept every fourth sample
    // with no filter at all, comes back at full size as a tone at 0.2 of the
    // new rate.
    real_t largest = REAL_C(0.0);

    for(uint32_t index = 0; index < 4000u; index++)
    {
        if((index % 4u) == 0u)
        {
            real_t kept = REAL_SIN(REAL_C(2.0) * PI * REAL_C(0.30)
                                   * (real_t)index);
            if(REAL_ABS(kept) > largest) { largest = REAL_ABS(kept); }
        }
    }

    // It is all still there, and it is now at a frequency it never had.
    TEST_ASSERT_TRUE(largest > REAL_C(0.9));
}

void test_a_decimator_gives_one_sample_for_each_factor(void)
{
    resample_t resample = resample_alloc_decimator(5, resample_advised_length(5));
    uint32_t made = 0;
    real_t output;

    for(uint32_t index = 0; index < 100u; index++)
    {
        if(resample_decimate(&resample, REAL_C(1.0), &output))
        {
            made++;
        }
    }

    TEST_ASSERT_EQUAL(20, made);

    resample_free(&resample);
}

void test_an_interpolator_keeps_the_size_of_the_signal(void)
{
    // Putting zeros between the samples makes each output factor times too
    // small. The module puts that right in the coefficients, once, rather than
    // multiplying every answer.
    uint32_t length = resample_advised_length(4);
    resample_t resample = resample_alloc_interpolator(4, length);
    real_t output[4];
    real_t largest = REAL_C(0.0);
    uint32_t made = 0;

    for(uint32_t index = 0; index < 4000u; index++)
    {
        real_t sample = REAL_SIN(REAL_C(2.0) * PI * REAL_C(0.05)
                                 * (real_t)index);
        resample_interpolate(&resample, sample, output);
        made++;

        if(made > ((length / 4u) + 100u))
        {
            for(uint32_t place = 0; place < 4u; place++)
            {
                if(REAL_ABS(output[place]) > largest)
                {
                    largest = REAL_ABS(output[place]);
                }
            }
        }
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.03), REAL_C(1.0), largest);

    resample_free(&resample);
}

void test_an_interpolator_gives_factor_samples_for_each_one(void)
{
    resample_t resample = resample_alloc_interpolator(3,
                              resample_advised_length(3));
    real_t output[3];

    TEST_ASSERT_EQUAL(3, resample_interpolate(&resample, REAL_C(1.0), output));

    resample_free(&resample);
}

void test_going_up_and_then_down_again_gives_back_the_signal(void)
{
    // The two are each other's opposite. A signal that goes up by 4 and back
    // down by 4 must come out as it went in, delayed by the two filters.
    const uint32_t size = 2000u;
    uint32_t length = resample_advised_length(4);
    resample_t up = resample_alloc_interpolator(4, length);
    resample_t down = resample_alloc_decimator(4, length);

    real_t middle[4];
    real_t* result = (real_t*)malloc(sizeof(real_t) * size);
    uint32_t written = 0;

    for(uint32_t index = 0; index < size; index++)
    {
        real_t sample = REAL_SIN(REAL_C(2.0) * PI * REAL_C(0.03)
                                 * (real_t)index);
        resample_interpolate(&up, sample, middle);

        for(uint32_t place = 0; place < 4u; place++)
        {
            real_t out;
            if(resample_decimate(&down, middle[place], &out)
               && (written < size))
            {
                result[written] = out;
                written++;
            }
        }
    }

    TEST_ASSERT_EQUAL(size, written);

    // Both filters have a middle, thus the delay is the sum of their halves at
    // the input rate.
    uint32_t delay = (length / 2u) / 4u + (length / 2u) / 4u;
    real_t largest = REAL_C(0.0);

    for(uint32_t index = delay + 200u; index < (size - 10u); index++)
    {
        real_t wanted = REAL_SIN(REAL_C(2.0) * PI * REAL_C(0.03)
                                 * (real_t)(index - delay));
        real_t error = REAL_ABS(result[index] - wanted);
        if(error > largest) { largest = error; }
    }

    TEST_ASSERT_TRUE(largest < REAL_C(0.05));

    free(result);
    resample_free(&up);
    resample_free(&down);
}

void test_resample_decimate_block(void)
{
    resample_t resample = resample_alloc_decimator(4,
                              resample_advised_length(4));
    real_t input[400];
    real_t output[100];

    for(uint32_t index = 0; index < 400u; index++)
    {
        input[index] = REAL_C(1.0);
    }

    uint32_t written = resample_decimate_block(&resample, input, output, 400u);

    TEST_ASSERT_EQUAL(100, written);

    resample_free(&resample);
}

void test_resample_interpolate_block(void)
{
    resample_t resample = resample_alloc_interpolator(4,
                              resample_advised_length(4));
    real_t input[50];
    real_t output[200];

    for(uint32_t index = 0; index < 50u; index++)
    {
        input[index] = REAL_C(1.0);
    }

    uint32_t written = resample_interpolate_block(&resample, input, output, 50u);

    TEST_ASSERT_EQUAL(200, written);

    resample_free(&resample);
}

void test_resample_delay(void)
{
    uint32_t length = resample_advised_length(4);
    resample_t resample = resample_alloc_decimator(4, length);

    // Half the length at the input rate, which is that divided by the factor
    // at the output rate.
    TEST_ASSERT_EQUAL((length / 2u) / 4u, resample_delay(&resample));

    resample_free(&resample);
}

void test_resample_reset(void)
{
    resample_t resample = resample_alloc_decimator(4,
                              resample_advised_length(4));
    real_t output;

    // Two samples in, thus the next output is two steps away.
    resample_decimate(&resample, REAL_C(1.0), &output);
    resample_decimate(&resample, REAL_C(1.0), &output);

    resample_reset(&resample);

    // After a reset the count starts again, thus four more samples are needed.
    TEST_ASSERT_EQUAL(false, resample_decimate(&resample, REAL_C(1.0), &output));
    TEST_ASSERT_EQUAL(false, resample_decimate(&resample, REAL_C(1.0), &output));
    TEST_ASSERT_EQUAL(false, resample_decimate(&resample, REAL_C(1.0), &output));
    TEST_ASSERT_EQUAL(true, resample_decimate(&resample, REAL_C(1.0), &output));

    resample_free(&resample);
}

void test_a_longer_filter_stops_more(void)
{
    // The length is a trade and not a law. A longer filter makes a sharper
    // turn, thus it lets less of the false tone through.
    real_t with_short = decimated_size(4, 41u, REAL_C(0.14));
    real_t with_long = decimated_size(4, 401u, REAL_C(0.14));

    TEST_ASSERT_TRUE(with_long < with_short);
}
