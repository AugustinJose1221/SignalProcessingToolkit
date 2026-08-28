#include "unity.h"
#include "real_assert.h"
#include "farrow.h"
#include "ringbuf.h"
#include <math.h>

#define PI  REAL_C(3.14159265358979323846)

void setUp(void)
{

}

void tearDown(void)
{

}

void test_farrow_is_valid_order(void)
{
    TEST_ASSERT_EQUAL(false, farrow_is_valid_order(0u));
    TEST_ASSERT_EQUAL(true, farrow_is_valid_order(1u));
    TEST_ASSERT_EQUAL(true, farrow_is_valid_order(FARROW_LARGEST_ORDER));
    TEST_ASSERT_EQUAL(false,
                      farrow_is_valid_order(FARROW_LARGEST_ORDER + 1u));
}

// The delay it can apply runs from half its order to one more than that, and a
// caller wanting more takes the whole samples elsewhere.
void test_farrow_the_delay_it_takes_is_one_sample_wide(void)
{
    farrow_t filter = farrow_alloc(3u);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(1.5),
                            farrow_smallest_delay(3u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(2.5),
                            farrow_largest_delay(3u));

    TEST_ASSERT_EQUAL(true, farrow_set_delay(&filter, REAL_C(1.5)));
    TEST_ASSERT_EQUAL(true, farrow_set_delay(&filter, REAL_C(2.5)));
    TEST_ASSERT_EQUAL(true, farrow_set_delay(&filter, REAL_C(2.0)));

    TEST_ASSERT_EQUAL(false, farrow_set_delay(&filter, REAL_C(1.4)));
    TEST_ASSERT_EQUAL(false, farrow_set_delay(&filter, REAL_C(2.6)));

    // Refused and left as it was.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(2.0),
                            farrow_get_delay(&filter));

    farrow_free(&filter);
}

// A DELAY OF A WHOLE NUMBER OF SAMPLES HAS NOTHING TO WORK OUT, thus the answer
// must be the sample itself and not nearly it. Anything else would mean the
// weights are wrong.
void test_farrow_a_whole_delay_gives_the_sample_back(void)
{
    uint32_t orders[3] = {1u, 3u, 5u};

    for(uint32_t which = 0; which < 3u; which++)
    {
        uint32_t order = orders[which];
        farrow_t filter = farrow_alloc(order);

        // Half the order rounded up is the one whole delay inside the range
        // the filter takes.
        real_t whole = (real_t)((order + 1u) / 2u);

        TEST_ASSERT_EQUAL(true, farrow_set_delay(&filter, whole));

        real_t given[10] = {REAL_C(1.0), REAL_C(-0.5), REAL_C(0.25),
                            REAL_C(2.0), REAL_C(-1.5), REAL_C(0.75),
                            REAL_C(0.0), REAL_C(-2.0), REAL_C(1.25),
                            REAL_C(0.5)};
        real_t out[10];

        TEST_ASSERT_EQUAL(true, farrow_process_block(&filter, given, out,
                                                     10u));

        uint32_t shift = (uint32_t)whole;

        for(uint32_t index = shift; index < 10u; index++)
        {
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), given[index - shift],
                                    out[index]);
        }

        farrow_free(&filter);
    }
}

// The weights are the ones Lagrange wrote down, and for the two smallest orders
// they can be written out by hand and compared.
void test_farrow_the_weights_are_the_ones_lagrange_wrote_down(void)
{
    // Order 1 at half a sample is the average of the two samples.
    farrow_t line = farrow_alloc(1u);

    TEST_ASSERT_EQUAL(true, farrow_set_delay(&line, REAL_C(0.5)));

    real_t impulse[4] = {REAL_C(1.0), REAL_C(0.0), REAL_C(0.0), REAL_C(0.0)};
    real_t out[4];

    TEST_ASSERT_EQUAL(true, farrow_process_block(&line, impulse, out, 4u));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.5), out[0]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.5), out[1]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0), out[2]);

    farrow_free(&line);

    // Order 3 halfway between two samples is the well known set of four.
    farrow_t cubic = farrow_alloc(3u);

    TEST_ASSERT_EQUAL(true, farrow_set_delay(&cubic, REAL_C(1.5)));
    TEST_ASSERT_EQUAL(true, farrow_process_block(&cubic, impulse, out, 4u));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), -REAL_C(0.0625), out[0]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.5625), out[1]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.5625), out[2]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), -REAL_C(0.0625), out[3]);

    farrow_free(&cubic);
}

// THE WEIGHTS MUST ADD UP TO ONE AT EVERY DELAY. A set that did not would
// change the level of a signal that is not changing at all, which is the one
// thing an interpolator must never do.
void test_farrow_a_signal_that_does_not_change_comes_through_unchanged(void)
{
    for(uint32_t order = 1u; order <= FARROW_LARGEST_ORDER; order++)
    {
        farrow_t filter = farrow_alloc(order);

        for(uint32_t step = 0; step <= 10u; step++)
        {
            real_t delay = farrow_smallest_delay(order)
                           + ((real_t)step / REAL_C(10.0));

            TEST_ASSERT_EQUAL(true, farrow_set_delay(&filter, delay));
            farrow_reset(&filter);

            real_t last = REAL_C(0.0);

            for(uint32_t index = 0; index < 40u; index++)
            {
                last = farrow_process_sample(&filter, REAL_C(3.5));
            }

            // THE ROOM FOLLOWS THE WIDTH AND THE ORDER, and the header says
            // why: the weights are worked out from products and divisions that
            // grow quickly with the order, thus at 32 bits they add up to one
            // less and less exactly. Measured, the worst level error on a
            // steady signal is a part in six hundred at an order of 8 at 32
            // bits and 4.1e-12 at 64. The bound below is that with room to
            // spare.
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(3.5), last);
        }

        farrow_free(&filter);
    }

    // The two smallest orders are exact at either width, because their weights
    // are worked out from nothing that can lose a digit.
    for(uint32_t order = 1u; order <= 2u; order++)
    {
        farrow_t filter = farrow_alloc(order);

        TEST_ASSERT_EQUAL(true,
                          farrow_set_delay(&filter,
                                           farrow_smallest_delay(order)
                                           + REAL_C(0.3)));

        real_t last = REAL_C(0.0);

        for(uint32_t index = 0; index < 40u; index++)
        {
            last = farrow_process_sample(&filter, REAL_C(3.5));
        }

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(3.5), last);

        farrow_free(&filter);
    }
}

// THE REASON THE MODULE EXISTS. A tone delayed by a part of a sample must line
// up with the same tone worked out at that delay directly.
void test_farrow_delays_a_tone_by_a_part_of_a_sample(void)
{
    const real_t frequency = REAL_C(0.05);
    real_t fractions[5] = {REAL_C(0.0), REAL_C(0.25), REAL_C(0.5),
                           REAL_C(0.75), REAL_C(1.0)};

    for(uint32_t which = 0; which < 5u; which++)
    {
        farrow_t filter = farrow_alloc(5u);
        real_t delay = farrow_smallest_delay(5u) + fractions[which];

        TEST_ASSERT_EQUAL(true, farrow_set_delay(&filter, delay));

        real_t worst = REAL_C(0.0);

        for(uint32_t index = 0; index < 400u; index++)
        {
            real_t in = REAL_SIN(REAL_C(2.0) * PI * frequency
                                 * (real_t)index);
            real_t got = farrow_process_sample(&filter, in);

            if(index < 40u)
            {
                continue;
            }

            real_t wanted = REAL_SIN(REAL_C(2.0) * PI * frequency
                                     * ((real_t)index - delay));
            real_t off = REAL_ABS(got - wanted);

            if(off > worst) { worst = off; }
        }

        // At a twentieth of the sample rate an order of 5 is very close.
        TEST_ASSERT_TRUE(worst < REAL_C(0.002));

        farrow_free(&filter);
    }
}

// A HIGHER ORDER KEEPS MORE OF THE SIGNAL, which is the whole reason to pay for
// one. Measured at the delay halfway between two samples, which is the worst
// place there is.
void test_farrow_a_higher_order_keeps_more_of_a_fast_signal(void)
{
    const real_t frequency = REAL_C(0.3);
    real_t kept[4];
    uint32_t orders[4] = {1u, 3u, 5u, 7u};

    for(uint32_t which = 0; which < 4u; which++)
    {
        farrow_t filter = farrow_alloc(orders[which]);

        TEST_ASSERT_EQUAL(true,
                          farrow_set_delay(&filter,
                                           farrow_smallest_delay(
                                               orders[which])));

        real_t loudest = REAL_C(0.0);

        for(uint32_t index = 0; index < 600u; index++)
        {
            real_t in = REAL_SIN(REAL_C(2.0) * PI * frequency
                                 * (real_t)index);
            real_t got = farrow_process_sample(&filter, in);

            if((index > 50u) && (REAL_ABS(got) > loudest))
            {
                loudest = REAL_ABS(got);
            }
        }

        kept[which] = loudest;
        farrow_free(&filter);
    }

    // Each order keeps more than the one below it, and the table in the header
    // says roughly how much.
    TEST_ASSERT_TRUE(kept[1] > kept[0]);
    TEST_ASSERT_TRUE(kept[2] > kept[1]);
    TEST_ASSERT_TRUE(kept[3] > kept[2]);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(0.588), kept[0]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), REAL_C(0.926), kept[3]);
}

void test_farrow_static_alloc_takes_no_memory_from_the_heap(void)
{
    static real_t history[FARROW_TAP_COUNT(3u)];
    static real_t weight[FARROW_WEIGHT_COUNT(3u)];
    static real_t working[FARROW_TAP_COUNT(3u)];

    farrow_t filter = farrow_static_alloc(3u, history, weight, working);

    TEST_ASSERT_EQUAL(false, filter.dynamic_alloc);
    TEST_ASSERT_EQUAL(true, farrow_set_delay(&filter, REAL_C(2.0)));

    real_t impulse[6] = {REAL_C(1.0), REAL_C(0.0), REAL_C(0.0), REAL_C(0.0),
                         REAL_C(0.0), REAL_C(0.0)};
    real_t out[6];

    TEST_ASSERT_EQUAL(true, farrow_process_block(&filter, impulse, out, 6u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(1.0), out[2]);

    farrow_free(&filter);
}

void test_farrow_reset_forgets_the_samples_and_keeps_the_delay(void)
{
    farrow_t filter = farrow_alloc(3u);

    TEST_ASSERT_EQUAL(true, farrow_set_delay(&filter, REAL_C(1.75)));

    for(uint32_t index = 0; index < 20u; index++)
    {
        farrow_process_sample(&filter, REAL_C(5.0));
    }

    farrow_reset(&filter);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(1.75),
                            farrow_get_delay(&filter));

    // The first sample after a reset is worked out from nothing but itself.
    farrow_t fresh = farrow_alloc(3u);

    TEST_ASSERT_EQUAL(true, farrow_set_delay(&fresh, REAL_C(1.75)));

    for(uint32_t index = 0; index < 10u; index++)
    {
        real_t one = farrow_process_sample(&filter, (real_t)index);
        real_t other = farrow_process_sample(&fresh, (real_t)index);

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), other, one);
    }

    farrow_free(&filter);
    farrow_free(&fresh);
}
