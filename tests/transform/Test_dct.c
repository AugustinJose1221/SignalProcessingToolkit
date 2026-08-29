#include "unity.h"
#include "real_assert.h"
#include "dct.h"
#include <math.h>

#define PI  REAL_C(3.14159265358979323846)

void setUp(void)
{

}

void tearDown(void)
{

}

void test_dct_is_valid_size(void)
{
    TEST_ASSERT_EQUAL(false, dct_is_valid_size(0u));
    TEST_ASSERT_EQUAL(true, dct_is_valid_size(1u));
    // Any size at all and not only a power of two, which is what this offers
    // over the transform.
    TEST_ASSERT_EQUAL(true, dct_is_valid_size(37u));
    TEST_ASSERT_EQUAL(true, dct_is_valid_size(DCT_LARGEST_SIZE));
    TEST_ASSERT_EQUAL(false, dct_is_valid_size(DCT_LARGEST_SIZE + 1u));
}

// THE RULE THAT MAKES IT A TRANSFORM. Taking it and undoing it must give back
// what went in, whatever went in.
void test_dct_undoing_it_gives_back_what_went_in(void)
{
    uint32_t sizes[4] = {1u, 2u, 37u, 64u};

    for(uint32_t which = 0; which < 4u; which++)
    {
        uint32_t size = sizes[which];
        real_t given[64];
        real_t cosines[64];
        real_t back[64];

        uint32_t seed = 7u;

        for(uint32_t index = 0; index < size; index++)
        {
            seed = (seed * 1103515245u) + 12345u;
            given[index] = ((real_t)((seed >> 16u) % 2000u)
                            / REAL_C(1000.0)) - REAL_C(1.0);
        }

        TEST_ASSERT_EQUAL(true, dct_forward(given, cosines, size));
        TEST_ASSERT_EQUAL(true, dct_inverse(cosines, back, size));

        for(uint32_t index = 0; index < size; index++)
        {
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), given[index], back[index]);
        }
    }
}

// A signal that does not change is one cosine of no frequency and nothing else,
// thus everything but the first number must be nothing.
void test_dct_a_steady_signal_is_one_number(void)
{
    real_t given[32];
    real_t cosines[32];

    for(uint32_t index = 0; index < 32u; index++)
    {
        given[index] = REAL_C(2.5);
    }

    TEST_ASSERT_EQUAL(true, dct_forward(given, cosines, 32u));

    // The first holds the whole signal, scaled by the root of the size.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001),
                            REAL_C(2.5) * REAL_SQRT(REAL_C(32.0)),
                            cosines[0]);

    for(uint32_t index = 1; index < 32u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(0.0), cosines[index]);
    }

    // And one number is enough to hold the whole of it.
    TEST_ASSERT_EQUAL(1u, dct_count_for_share(cosines, 32u, REAL_C(0.999)));
}

// THE REASON THE MODULE EXISTS. A signal that changes slowly comes out as a
// handful of large numbers and a long tail of nearly nothing.
void test_dct_gathers_a_slow_signal_into_its_first_few_numbers(void)
{
    real_t slow[64];
    real_t noise[64];
    real_t cosines[64];

    uint32_t seed = 1u;

    for(uint32_t index = 0; index < 64u; index++)
    {
        real_t part = (real_t)index / REAL_C(64.0);

        // A curve that does NOT come back to where it started, which is the
        // case the transform handles badly and this one handles well.
        slow[index] = part + (REAL_C(0.3) * REAL_SIN(REAL_C(2.0) * PI * part));

        seed = (seed * 1103515245u) + 12345u;
        noise[index] = ((real_t)((seed >> 16u) % 2000u) / REAL_C(1000.0))
                       - REAL_C(1.0);
    }

    TEST_ASSERT_EQUAL(true, dct_forward(slow, cosines, 64u));

    // Four numbers of sixty four hold 0.99 of it.
    TEST_ASSERT_EQUAL(4u, dct_count_for_share(cosines, 64u, REAL_C(0.99)));
    TEST_ASSERT_EQUAL(8u, dct_count_for_share(cosines, 64u, REAL_C(0.999)));

    // And a signal of noise has nothing to gather: it needs nearly all of them.
    TEST_ASSERT_EQUAL(true, dct_forward(noise, cosines, 64u));
    TEST_ASSERT_TRUE(dct_count_for_share(cosines, 64u, REAL_C(0.99)) > 50u);
}

// The share is a share of the whole, thus asking for all of it gives every
// number and asking for none of it is refused.
void test_dct_count_for_share_refuses_what_it_cannot_answer(void)
{
    real_t given[16];
    real_t cosines[16];

    for(uint32_t index = 0; index < 16u; index++)
    {
        given[index] = (real_t)index;
    }

    TEST_ASSERT_EQUAL(true, dct_forward(given, cosines, 16u));

    TEST_ASSERT_EQUAL(16u, dct_count_for_share(cosines, 16u, REAL_C(1.0)));
    TEST_ASSERT_EQUAL(0u, dct_count_for_share(cosines, 16u, REAL_C(0.0)));
    TEST_ASSERT_EQUAL(0u, dct_count_for_share(cosines, 16u, REAL_C(1.5)));
    TEST_ASSERT_EQUAL(0u, dct_count_for_share(cosines, 16u, -REAL_C(0.5)));

    // A signal holding nothing has no share to find.
    real_t empty[8] = {REAL_C(0.0)};

    TEST_ASSERT_EQUAL(0u, dct_count_for_share(empty, 8u, REAL_C(0.5)));
}

void test_dct_refuses_a_size_it_cannot_take(void)
{
    real_t given[8] = {REAL_C(1.0)};
    real_t out[8];

    TEST_ASSERT_EQUAL(false, dct_forward(given, out, 0u));
    TEST_ASSERT_EQUAL(false, dct_inverse(given, out, 0u));
    TEST_ASSERT_EQUAL(false, dct_forward(given, out, DCT_LARGEST_SIZE + 1u));
}

void test_asking_for_the_whole_of_a_signal_keeps_the_whole_of_it(void)
{
    // The count for a share says how many cosines carry that share of the
    // signal. Asking for all of it must give all of them: a signal is never
    // wholly held by fewer cosines than it has, and the running total that
    // gathers them must not stop short through rounding.
    uint32_t size = 32u;
    real_t signal[32];
    real_t cosines[32];

    for(uint32_t index = 0; index < size; index++)
    {
        signal[index] = (real_t)sin(0.3 * (double)index)
                        + (real_t)cos(1.1 * (double)index);
    }

    TEST_ASSERT_TRUE(dct_forward(signal, cosines, size));

    TEST_ASSERT_EQUAL(size, dct_count_for_share(cosines, size, REAL_C(1.0)));

    // And a share of almost none is carried by very few of them.
    uint32_t few = dct_count_for_share(cosines, size, REAL_C(0.01));
    TEST_ASSERT_TRUE(few >= 1u);
    TEST_ASSERT_TRUE(few < size);

    // A share that means nothing is refused.
    TEST_ASSERT_EQUAL(0, dct_count_for_share(cosines, size, REAL_C(0.0)));
    TEST_ASSERT_EQUAL(0, dct_count_for_share(cosines, size, REAL_C(1.5)));
}
