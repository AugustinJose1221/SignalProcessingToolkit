#include "unity.h"
#include "ringbuf.h"
#include <stdlib.h>

#define TOLERANCE   0.0001f

void setUp(void)
{

}

void tearDown(void)
{

}

void test_ringbuf_alloc(void)
{
    ringbuf_t ringbuf = ringbuf_alloc(8);

    TEST_ASSERT_EQUAL(8, ringbuf.size);
    TEST_ASSERT_EQUAL(0, ringbuf_count(&ringbuf));
    TEST_ASSERT_EQUAL(true, ringbuf.dynamic_alloc);
    TEST_ASSERT_NOT_NULL(ringbuf.data);

    ringbuf_free(&ringbuf);
}

void test_ringbuf_static_alloc(void)
{
    float data[4];
    ringbuf_t ringbuf = ringbuf_static_alloc(4, data);

    TEST_ASSERT_EQUAL(4, ringbuf.size);
    TEST_ASSERT_EQUAL(false, ringbuf.dynamic_alloc);
    TEST_ASSERT_EQUAL_PTR(data, ringbuf.data);

    // A free must leave the memory of the caller alone.
    ringbuf_free(&ringbuf);
    TEST_ASSERT_EQUAL_PTR(data, ringbuf.data);
}

void test_ringbuf_gives_the_newest_sample_at_the_age_of_nothing(void)
{
    ringbuf_t ringbuf = ringbuf_alloc(4);

    ringbuf_put(&ringbuf, 1.0f);
    ringbuf_put(&ringbuf, 2.0f);
    ringbuf_put(&ringbuf, 3.0f);

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, ringbuf_get(&ringbuf, 0));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 2.0f, ringbuf_get(&ringbuf, 1));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, ringbuf_get(&ringbuf, 2));

    ringbuf_free(&ringbuf);
}

void test_ringbuf_counts_up_to_its_size_and_stops(void)
{
    ringbuf_t ringbuf = ringbuf_alloc(3);

    TEST_ASSERT_EQUAL(0, ringbuf_count(&ringbuf));
    ringbuf_put(&ringbuf, 1.0f);
    TEST_ASSERT_EQUAL(1, ringbuf_count(&ringbuf));
    ringbuf_put(&ringbuf, 2.0f);
    ringbuf_put(&ringbuf, 3.0f);
    TEST_ASSERT_EQUAL(3, ringbuf_count(&ringbuf));

    ringbuf_put(&ringbuf, 4.0f);
    TEST_ASSERT_EQUAL(3, ringbuf_count(&ringbuf));

    ringbuf_free(&ringbuf);
}

void test_ringbuf_is_full(void)
{
    ringbuf_t ringbuf = ringbuf_alloc(2);

    TEST_ASSERT_EQUAL(false, ringbuf_is_full(&ringbuf));
    ringbuf_put(&ringbuf, 1.0f);
    TEST_ASSERT_EQUAL(false, ringbuf_is_full(&ringbuf));
    ringbuf_put(&ringbuf, 2.0f);
    TEST_ASSERT_EQUAL(true, ringbuf_is_full(&ringbuf));

    ringbuf_free(&ringbuf);
}

void test_ringbuf_forgets_the_oldest_sample_when_it_is_full(void)
{
    ringbuf_t ringbuf = ringbuf_alloc(3);

    for(uint32_t index = 1; index <= 5u; index++)
    {
        ringbuf_put(&ringbuf, (float)index);
    }

    // The buffer holds 3, thus 1 and 2 are gone and 3, 4, 5 remain.
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 5.0f, ringbuf_get(&ringbuf, 0));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 4.0f, ringbuf_get(&ringbuf, 1));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, ringbuf_get(&ringbuf, 2));

    ringbuf_free(&ringbuf);
}

void test_ringbuf_gives_nothing_for_an_age_it_does_not_hold(void)
{
    ringbuf_t ringbuf = ringbuf_alloc(4);

    ringbuf_put(&ringbuf, 7.0f);

    // Only one sample has arrived, thus every older age is empty.
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, ringbuf_get(&ringbuf, 1));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, ringbuf_get(&ringbuf, 3));
    // An age at or above the size can never be held.
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, ringbuf_get(&ringbuf, 4));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, ringbuf_get(&ringbuf, 1000));

    ringbuf_free(&ringbuf);
}

void test_ringbuf_works_as_a_delay_line(void)
{
    // This is the use that the header names first. Put the sample that
    // arrived, then take the one from a fixed number of steps ago.
    ringbuf_t line = ringbuf_alloc(4);
    float input[8] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    float output[8];

    for(uint32_t index = 0; index < 8u; index++)
    {
        ringbuf_put(&line, input[index]);
        output[index] = ringbuf_get(&line, 3u);
    }

    // The first three steps have no sample that old, thus they give nothing.
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, output[0]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, output[2]);
    // From then on the output follows the input by three steps.
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, output[3]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 5.0f, output[7]);

    ringbuf_free(&line);
}

void test_ringbuf_copy_writes_the_oldest_first(void)
{
    ringbuf_t ringbuf = ringbuf_alloc(4);
    float block[4];

    for(uint32_t index = 1; index <= 6u; index++)
    {
        ringbuf_put(&ringbuf, (float)index);
    }

    uint32_t written = ringbuf_copy(&ringbuf, block);

    TEST_ASSERT_EQUAL(4, written);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 3.0f, block[0]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 4.0f, block[1]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 5.0f, block[2]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 6.0f, block[3]);

    ringbuf_free(&ringbuf);
}

void test_ringbuf_copy_writes_only_what_it_holds(void)
{
    ringbuf_t ringbuf = ringbuf_alloc(4);
    float block[4] = {9.0f, 9.0f, 9.0f, 9.0f};

    ringbuf_put(&ringbuf, 1.0f);
    ringbuf_put(&ringbuf, 2.0f);

    uint32_t written = ringbuf_copy(&ringbuf, block);

    TEST_ASSERT_EQUAL(2, written);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, block[0]);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 2.0f, block[1]);
    // What was not written must be left alone.
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 9.0f, block[2]);

    ringbuf_free(&ringbuf);
}

void test_ringbuf_reset_forgets_every_sample(void)
{
    ringbuf_t ringbuf = ringbuf_alloc(4);

    ringbuf_put(&ringbuf, 1.0f);
    ringbuf_put(&ringbuf, 2.0f);
    ringbuf_reset(&ringbuf);

    TEST_ASSERT_EQUAL(0, ringbuf_count(&ringbuf));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, ringbuf_get(&ringbuf, 0));

    ringbuf_put(&ringbuf, 5.0f);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 5.0f, ringbuf_get(&ringbuf, 0));

    ringbuf_free(&ringbuf);
}

void test_ringbuf_of_one_sample(void)
{
    // The smallest buffer that means anything. It always holds the newest
    // sample and nothing else.
    ringbuf_t ringbuf = ringbuf_alloc(1);

    ringbuf_put(&ringbuf, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 1.0f, ringbuf_get(&ringbuf, 0));
    ringbuf_put(&ringbuf, 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 2.0f, ringbuf_get(&ringbuf, 0));
    TEST_ASSERT_EQUAL(1, ringbuf_count(&ringbuf));

    ringbuf_free(&ringbuf);
}

void test_ringbuf_of_a_size_that_is_not_a_power_of_two(void)
{
    // The header says the size need not be a power of two. This holds it, over
    // enough samples that the head turns back to the start many times.
    ringbuf_t ringbuf = ringbuf_alloc(5);

    for(uint32_t index = 0; index < 103u; index++)
    {
        ringbuf_put(&ringbuf, (float)index);
    }

    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 102.0f, ringbuf_get(&ringbuf, 0));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 98.0f, ringbuf_get(&ringbuf, 4));
    TEST_ASSERT_FLOAT_WITHIN(TOLERANCE, 0.0f, ringbuf_get(&ringbuf, 5));

    ringbuf_free(&ringbuf);
}
