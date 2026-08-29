#include "unity.h"
#include "real_assert.h"
#include "interp.h"
#include "cspline.h"
#include "binarysearch.h"
#include "matrix.h"
#include <stdlib.h>
#include <math.h>

#define TOLERANCE   REAL_C(0.0001)

void setUp(void)
{

}

void tearDown(void)
{

}

void test_interp_is_valid_kind(void)
{
    TEST_ASSERT_EQUAL(true, interp_is_valid_kind(INTERP_LINEAR));
    TEST_ASSERT_EQUAL(true, interp_is_valid_kind(INTERP_PCHIP));
    TEST_ASSERT_EQUAL(false, interp_is_valid_kind(
                          (interp_kind_t)(INTERP_PCHIP + 1)));
}

void test_interp_is_valid_table(void)
{
    real_t rising[4] = {REAL_C(0.0), REAL_C(1.0), REAL_C(2.5), REAL_C(4.0)};
    real_t falling[4] = {REAL_C(4.0), REAL_C(2.5), REAL_C(1.0), REAL_C(0.0)};
    real_t repeated[4] = {REAL_C(0.0), REAL_C(1.0), REAL_C(1.0), REAL_C(2.0)};
    real_t one[1] = {REAL_C(0.0)};

    TEST_ASSERT_EQUAL(true, interp_is_valid_table(rising, 4u));
    TEST_ASSERT_EQUAL(false, interp_is_valid_table(falling, 4u));
    // Two entries at one input would ask the curve to hold two values there.
    TEST_ASSERT_EQUAL(false, interp_is_valid_table(repeated, 4u));
    TEST_ASSERT_EQUAL(false, interp_is_valid_table(one, 1u));
}

void test_interp_passes_through_every_point_of_the_table(void)
{
    // Whatever the way of reading between the points, the points themselves
    // must come back exactly. A table is a measurement.
    real_t x[5] = {REAL_C(0.0), REAL_C(1.0), REAL_C(3.0), REAL_C(4.0),
                   REAL_C(7.0)};
    real_t y[5] = {REAL_C(2.0), REAL_C(5.0), REAL_C(1.0), REAL_C(8.0),
                   REAL_C(3.0)};
    real_t slopes[5];

    TEST_ASSERT_EQUAL(true, interp_pchip_slopes(x, y, 5u, slopes));

    for(uint32_t index = 0; index < 5u; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), y[index],
                                interp_linear(x, y, 5u, x[index]));
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), y[index],
                                interp_pchip(x, y, slopes, 5u, x[index]));
    }
}

void test_interp_linear_halfway_between_two_points(void)
{
    real_t x[2] = {REAL_C(0.0), REAL_C(10.0)};
    real_t y[2] = {REAL_C(100.0), REAL_C(200.0)};

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(150.0),
                            interp_linear(x, y, 2u, REAL_C(5.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(125.0),
                            interp_linear(x, y, 2u, REAL_C(2.5)));
}

void test_outside_the_table_the_answer_is_held_flat(void)
{
    // Carrying a line on past the end of a calibration says what the device
    // would read where it was never calibrated. Saying nothing is better.
    real_t x[3] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0)};
    real_t y[3] = {REAL_C(10.0), REAL_C(20.0), REAL_C(30.0)};
    real_t slopes[3];

    interp_pchip_slopes(x, y, 3u, slopes);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(10.0),
                            interp_linear(x, y, 3u, REAL_C(-100.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(30.0),
                            interp_linear(x, y, 3u, REAL_C(100.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(10.0),
                            interp_pchip(x, y, slopes, 3u, REAL_C(-100.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(30.0),
                            interp_pchip(x, y, slopes, 3u, REAL_C(100.0)));
}

void test_pchip_never_leaves_the_range_of_its_neighbours(void)
{
    // THE WHOLE REASON THIS MODULE EXISTS, measured against the spline that
    // was already here.
    //
    // A table that is flat, steps up from 0 to 10 once, and is flat again.
    // This is what a calibration of something with a threshold looks like.
    const uint32_t size = 7u;
    real_t x[7] = {REAL_C(0.0), REAL_C(1.0), REAL_C(2.0), REAL_C(3.0),
                   REAL_C(4.0), REAL_C(5.0), REAL_C(6.0)};
    real_t y[7] = {REAL_C(0.0), REAL_C(0.0), REAL_C(0.0), REAL_C(10.0),
                   REAL_C(10.0), REAL_C(10.0), REAL_C(10.0)};
    real_t slopes[7];

    interp_pchip_slopes(x, y, size, slopes);

    cspline_t spline = cspline_alloc(size);
    cspline_mempool_t pool = cspline_alloc_mempool(size);
    cspline_init(&spline, pool, x, y);

    real_t pchip_lowest = REAL_C(1000.0);
    real_t pchip_highest = -REAL_C(1000.0);
    real_t spline_lowest = REAL_C(1000.0);
    real_t spline_highest = -REAL_C(1000.0);

    for(uint32_t step = 0; step <= 600u; step++)
    {
        real_t place = (REAL_C(6.0) * (real_t)step) / REAL_C(600.0);

        real_t from_pchip = interp_pchip(x, y, slopes, size, place);
        real_t from_spline = cspline_get_interpolated_point(&spline, place);
        real_t from_linear = interp_linear(x, y, size, place);

        if(from_pchip < pchip_lowest) { pchip_lowest = from_pchip; }
        if(from_pchip > pchip_highest) { pchip_highest = from_pchip; }
        if(from_spline < spline_lowest) { spline_lowest = from_spline; }
        if(from_spline > spline_highest) { spline_highest = from_spline; }

        // A straight line can never leave the range either.
        TEST_ASSERT_TRUE(from_linear >= -TOLERANCE);
        TEST_ASSERT_TRUE(from_linear <= (REAL_C(10.0) + TOLERANCE));
    }

    // The table holds nothing below 0 and nothing above 10.
    TEST_ASSERT_TRUE(pchip_lowest >= -TOLERANCE);
    TEST_ASSERT_TRUE(pchip_highest <= (REAL_C(10.0) + TOLERANCE));

    // The spline leaves that range at both ends, and by a great deal: it
    // reports a value BELOW ZERO for a table that holds nothing below zero.
    TEST_ASSERT_TRUE(spline_lowest < -REAL_C(0.5));
    TEST_ASSERT_TRUE(spline_highest > REAL_C(10.5));

    cspline_free(spline);
    cspline_free_mempool(pool);
}

void test_pchip_never_falls_where_the_table_only_rises(void)
{
    // The other half of the same story, and the worse half. A device watching
    // for a fall would see one that is in the reading and not in the thing
    // being read.
    const uint32_t size = 7u;
    real_t x[7] = {REAL_C(0.0), REAL_C(1.0), REAL_C(2.0), REAL_C(3.0),
                   REAL_C(4.0), REAL_C(5.0), REAL_C(6.0)};
    real_t y[7] = {REAL_C(0.0), REAL_C(0.0), REAL_C(0.0), REAL_C(10.0),
                   REAL_C(10.0), REAL_C(10.0), REAL_C(10.0)};
    real_t slopes[7];

    interp_pchip_slopes(x, y, size, slopes);

    cspline_t spline = cspline_alloc(size);
    cspline_mempool_t pool = cspline_alloc_mempool(size);
    cspline_init(&spline, pool, x, y);

    uint32_t pchip_falls = 0;
    uint32_t spline_falls = 0;
    real_t last_pchip = interp_pchip(x, y, slopes, size, REAL_C(0.0));
    real_t last_spline = cspline_get_interpolated_point(&spline, REAL_C(0.0));

    for(uint32_t step = 1; step <= 600u; step++)
    {
        real_t place = (REAL_C(6.0) * (real_t)step) / REAL_C(600.0);

        real_t from_pchip = interp_pchip(x, y, slopes, size, place);
        real_t from_spline = cspline_get_interpolated_point(&spline, place);

        if(from_pchip < (last_pchip - REAL_C(0.000001))) { pchip_falls++; }
        if(from_spline < (last_spline - REAL_C(0.000001))) { spline_falls++; }

        last_pchip = from_pchip;
        last_spline = from_spline;
    }

    TEST_ASSERT_EQUAL(0, pchip_falls);
    TEST_ASSERT_TRUE(spline_falls > 100u);

    cspline_free(spline);
    cspline_free_mempool(pool);
}

void test_pchip_is_smooth_where_a_straight_line_is_not(void)
{
    // What pchip buys over a straight line: its slope has no corners. A
    // straight line changes slope at every point of the table, and a device
    // acting on a rate of change sees a step there that is not real.
    const uint32_t size = 5u;
    real_t x[5] = {REAL_C(0.0), REAL_C(1.0), REAL_C(2.0), REAL_C(3.0),
                   REAL_C(4.0)};
    real_t y[5] = {REAL_C(0.0), REAL_C(1.0), REAL_C(4.0), REAL_C(9.0),
                   REAL_C(16.0)};
    real_t slopes[5];

    interp_pchip_slopes(x, y, size, slopes);

    // The slope on each side of an ordinary point of the table.
    real_t step = REAL_C(0.001);
    real_t at = REAL_C(2.0);

    real_t before = (interp_pchip(x, y, slopes, size, at)
                     - interp_pchip(x, y, slopes, size, at - step)) / step;
    real_t after = (interp_pchip(x, y, slopes, size, at + step)
                    - interp_pchip(x, y, slopes, size, at)) / step;

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.02), before, after);

    // A straight line does have a corner there.
    real_t line_before = (interp_linear(x, y, size, at)
                          - interp_linear(x, y, size, at - step)) / step;
    real_t line_after = (interp_linear(x, y, size, at + step)
                         - interp_linear(x, y, size, at)) / step;

    TEST_ASSERT_TRUE(REAL_ABS(line_after - line_before) > REAL_C(1.0));
}

void test_pchip_of_a_straight_table_is_a_straight_line(void)
{
    // Where the table really is a straight line, pchip must give that line and
    // not something with a bend in it.
    real_t x[5] = {REAL_C(0.0), REAL_C(1.0), REAL_C(2.0), REAL_C(3.0),
                   REAL_C(4.0)};
    real_t y[5] = {REAL_C(0.0), REAL_C(2.0), REAL_C(4.0), REAL_C(6.0),
                   REAL_C(8.0)};
    real_t slopes[5];

    interp_pchip_slopes(x, y, 5u, slopes);

    for(uint32_t step = 0; step <= 40u; step++)
    {
        real_t place = (REAL_C(4.0) * (real_t)step) / REAL_C(40.0);

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(2.0) * place,
                                interp_pchip(x, y, slopes, 5u, place));
    }
}

void test_pchip_of_a_table_of_two_points(void)
{
    real_t x[2] = {REAL_C(0.0), REAL_C(4.0)};
    real_t y[2] = {REAL_C(1.0), REAL_C(9.0)};
    real_t slopes[2];

    TEST_ASSERT_EQUAL(true, interp_pchip_slopes(x, y, 2u, slopes));

    // Two points hold one straight line, thus all three ways agree.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0),
                            interp_pchip(x, y, slopes, 2u, REAL_C(2.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(5.0),
                            interp_linear(x, y, 2u, REAL_C(2.0)));
}

void test_interp_block(void)
{
    real_t x[4] = {REAL_C(0.0), REAL_C(1.0), REAL_C(2.0), REAL_C(3.0)};
    real_t y[4] = {REAL_C(0.0), REAL_C(1.0), REAL_C(4.0), REAL_C(9.0)};
    real_t slopes[4];
    real_t places[3] = {REAL_C(0.5), REAL_C(1.5), REAL_C(2.5)};
    real_t answers[3];

    interp_pchip_slopes(x, y, 4u, slopes);

    TEST_ASSERT_EQUAL(true, interp_block(x, y, slopes, 4u, INTERP_PCHIP,
                                         places, answers, 3u));
    TEST_ASSERT_TRUE(answers[0] > REAL_C(0.0));
    TEST_ASSERT_TRUE(answers[2] > answers[1]);

    // A straight line reads no slopes, thus none need be given.
    TEST_ASSERT_EQUAL(true, interp_block(x, y, NULL, 4u, INTERP_LINEAR,
                                         places, answers, 3u));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.5), answers[0]);

    // A curve without its slopes cannot be read.
    TEST_ASSERT_EQUAL(false, interp_block(x, y, NULL, 4u, INTERP_PCHIP,
                                          places, answers, 3u));
}

void test_interp_refuses_a_table_it_cannot_read(void)
{
    real_t falling[3] = {REAL_C(3.0), REAL_C(2.0), REAL_C(1.0)};
    real_t y[3] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0)};
    real_t slopes[3];
    real_t places[1] = {REAL_C(2.0)};
    real_t answers[1];

    TEST_ASSERT_EQUAL(false, interp_pchip_slopes(falling, y, 3u, slopes));
    TEST_ASSERT_EQUAL(false, interp_block(falling, y, slopes, 3u,
                                          INTERP_LINEAR, places, answers, 1u));
}

// THE EDGES OF A TABLE, AND THE TABLES THAT ARE BARELY TABLES.
//
// A table of calibration is read by code that does not always know what is in
// it. It may hold no points at all because the device was never calibrated, or
// one point because it was calibrated once, or two points at the same place
// because somebody wrote the same row twice.
//
// None of those is an error the caller can be told about, because the reading
// is a value and not a status. The module must therefore answer something
// sensible for every one of them, and these tests fix what that something is.

void test_a_table_with_no_points_reads_as_nothing(void)
{
    real_t input[1] = {REAL_C(0.0)};
    real_t output[1] = {REAL_C(0.0)};
    real_t slopes[1] = {REAL_C(0.0)};

    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0),
                           interp_linear(input, output, 0, REAL_C(1.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0),
                           interp_pchip(input, output, slopes, 0,
                                        REAL_C(1.0)));
}

void test_a_table_with_one_point_reads_as_that_point_everywhere(void)
{
    // One point says what the device reads at one place and nothing about the
    // slope. Holding the answer flat is the only honest reading.
    real_t input[1] = {REAL_C(5.0)};
    real_t output[1] = {REAL_C(7.0)};
    real_t slopes[1] = {REAL_C(0.0)};

    TEST_ASSERT_EQUAL_REAL(REAL_C(7.0),
                           interp_linear(input, output, 1, REAL_C(-100.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(7.0),
                           interp_linear(input, output, 1, REAL_C(5.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(7.0),
                           interp_linear(input, output, 1, REAL_C(100.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(7.0),
                           interp_pchip(input, output, slopes, 1,
                                        REAL_C(100.0)));
}

void test_two_rows_at_the_same_place_give_the_first_of_them(void)
{
    // A table that holds the same place twice has no slope between those two
    // rows, and working one out would divide by nothing. The module gives the
    // value of the row below rather than an answer that is not a number.
    real_t input[4] = {REAL_C(0.0), REAL_C(1.0), REAL_C(1.0), REAL_C(2.0)};
    real_t output[4] = {REAL_C(0.0), REAL_C(10.0), REAL_C(20.0),
                        REAL_C(30.0)};
    real_t slopes[4] = {REAL_C(1.0), REAL_C(1.0), REAL_C(1.0), REAL_C(1.0)};

    TEST_ASSERT_EQUAL_REAL(REAL_C(10.0),
                           interp_linear(input, output, 4, REAL_C(1.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(10.0),
                           interp_pchip(input, output, slopes, 4,
                                        REAL_C(1.0)));
}

void test_a_place_outside_the_table_is_held_flat_and_not_carried_on(void)
{
    // Carrying a straight line on past the end of a calibration says what the
    // device would read where it was never calibrated. Saying nothing is
    // better than saying that.
    real_t input[3] = {REAL_C(0.0), REAL_C(1.0), REAL_C(2.0)};
    real_t output[3] = {REAL_C(0.0), REAL_C(10.0), REAL_C(20.0)};
    real_t slopes[3] = {REAL_C(10.0), REAL_C(10.0), REAL_C(10.0)};

    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0),
                           interp_linear(input, output, 3, REAL_C(-50.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(20.0),
                           interp_linear(input, output, 3, REAL_C(50.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0),
                           interp_pchip(input, output, slopes, 3,
                                        REAL_C(-50.0)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(20.0),
                           interp_pchip(input, output, slopes, 3,
                                        REAL_C(50.0)));
}
