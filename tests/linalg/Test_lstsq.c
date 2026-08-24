#include "unity.h"
#include "real_assert.h"
#include "lstsq.h"
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

void test_lstsq_is_valid_fit(void)
{
    // As many readings as numbers to find, at least.
    TEST_ASSERT_EQUAL(true, lstsq_is_valid_fit(10u, 3u));
    TEST_ASSERT_EQUAL(true, lstsq_is_valid_fit(4u, 3u));
    TEST_ASSERT_EQUAL(false, lstsq_is_valid_fit(3u, 3u));

    TEST_ASSERT_EQUAL(true, lstsq_is_valid_fit(100u, LSTSQ_HIGHEST_ORDER));
    TEST_ASSERT_EQUAL(false, lstsq_is_valid_fit(100u, LSTSQ_HIGHEST_ORDER + 1u));
}

void test_lstsq_coefficient_count(void)
{
    // A line is of the first order and holds two numbers.
    TEST_ASSERT_EQUAL(2, LSTSQ_COEFFICIENT_COUNT(1));
    TEST_ASSERT_EQUAL(4, LSTSQ_COEFFICIENT_COUNT(3));
}

void test_lstsq_finds_a_line_through_points_that_lie_on_one(void)
{
    // Where the readings really do lie on a line, the fit must give that line
    // to the last digit the width can hold.
    real_t x[5] = {REAL_C(0.0), REAL_C(1.0), REAL_C(2.0), REAL_C(3.0),
                   REAL_C(4.0)};
    real_t y[5];
    real_t coefficients[2];

    for(uint32_t index = 0; index < 5u; index++)
    {
        y[index] = REAL_C(7.0) + (REAL_C(3.0) * x[index]);
    }

    TEST_ASSERT_EQUAL(true, lstsq_polyfit(x, y, 5u, 1u, coefficients));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(7.0), coefficients[0]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(3.0), coefficients[1]);
}

void test_lstsq_finds_a_cubic_through_points_that_lie_on_one(void)
{
    real_t x[10];
    real_t y[10];
    real_t coefficients[4];

    for(uint32_t index = 0; index < 10u; index++)
    {
        real_t place = (real_t)index / REAL_C(9.0);
        x[index] = place;
        y[index] = REAL_C(2.0) + (REAL_C(3.0) * place)
                   - (REAL_C(4.0) * place * place)
                   + (REAL_C(1.5) * place * place * place);
    }

    TEST_ASSERT_EQUAL(true, lstsq_polyfit(x, y, 10u, 3u, coefficients));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(2.0), coefficients[0]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(3.0), coefficients[1]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), -REAL_C(4.0), coefficients[2]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.5), coefficients[3]);
}

void test_lstsq_finds_the_middle_of_readings_that_do_not_lie_on_a_line(void)
{
    // The point of a fit: there IS no line through these five, and the answer
    // is the one that leaves the smallest total error.
    real_t x[5] = {REAL_C(0.0), REAL_C(1.0), REAL_C(2.0), REAL_C(3.0),
                   REAL_C(4.0)};
    real_t y[5] = {REAL_C(1.0), REAL_C(3.0), REAL_C(2.0), REAL_C(5.0),
                   REAL_C(4.0)};
    real_t coefficients[2];

    TEST_ASSERT_EQUAL(true, lstsq_polyfit(x, y, 5u, 1u, coefficients));

    // Worked out by hand: the line through these is 1.4 + 0.8x.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(1.4), coefficients[0]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.8), coefficients[1]);
}

void test_lstsq_evaluate(void)
{
    // 2 + 3x - x squared, at x of 2, is 2 + 6 - 4 = 4.
    real_t coefficients[3] = {REAL_C(2.0), REAL_C(3.0), -REAL_C(1.0)};

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(4.0),
                            lstsq_evaluate(coefficients, 2u, REAL_C(2.0)));
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0),
                            lstsq_evaluate(coefficients, 2u, REAL_C(0.0)));
}

void test_lstsq_fit_quality_says_when_the_fit_follows_and_when_it_does_not(void)
{
    // The one number to look at after a fit.
    real_t x[10];
    real_t straight[10];
    real_t bent[10];
    real_t coefficients[2];

    for(uint32_t index = 0; index < 10u; index++)
    {
        x[index] = (real_t)index;
        straight[index] = REAL_C(2.0) + (REAL_C(0.5) * x[index]);
        // A shape that no straight line can follow at all.
        bent[index] = ((index % 2u) == 0u) ? REAL_C(0.0) : REAL_C(10.0);
    }

    lstsq_polyfit(x, straight, 10u, 1u, coefficients);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0),
                            lstsq_fit_quality(x, straight, 10u, coefficients,
                                              1u));

    lstsq_polyfit(x, bent, 10u, 1u, coefficients);
    TEST_ASSERT_TRUE(lstsq_fit_quality(x, bent, 10u, coefficients, 1u)
                     < REAL_C(0.1));
}

void test_lstsq_fit_quality_of_readings_that_never_move(void)
{
    real_t x[5] = {REAL_C(0.0), REAL_C(1.0), REAL_C(2.0), REAL_C(3.0),
                   REAL_C(4.0)};
    real_t y[5] = {REAL_C(3.0), REAL_C(3.0), REAL_C(3.0), REAL_C(3.0),
                   REAL_C(3.0)};
    real_t coefficients[2];

    lstsq_polyfit(x, y, 5u, 1u, coefficients);

    // There is no movement to account for, thus a flat line accounts for all
    // of it.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0),
                            lstsq_fit_quality(x, y, 5u, coefficients, 1u));
}

void test_lstsq_refuses_the_order_where_the_answer_stops_being_worth_having(void)
{
    // The table in the header, held true.
    //
    // The module does not cap the order and hope. It looks at the diagonal of
    // the factor and refuses where the answer would be made of rounding, and
    // that place is one order past the last one that follows the readings.
#if defined(SPTK_REAL_64)
    const uint32_t last_good = 11u;
#else
    const uint32_t last_good = 5u;
#endif
    real_t x[60];
    real_t y[60];
    real_t coefficients[40];

    for(uint32_t index = 0; index < 60u; index++)
    {
        x[index] = (real_t)index / REAL_C(59.0);
        y[index] = REAL_SIN(REAL_C(3.0) * x[index]);
    }

    TEST_ASSERT_EQUAL(true, lstsq_polyfit(x, y, 60u, last_good, coefficients));
    TEST_ASSERT_EQUAL(false, lstsq_polyfit(x, y, 60u, last_good + 1u,
                                           coefficients));
}

void test_moving_x_to_minus_one_and_one_more_than_doubles_the_order(void)
{
    // The other half of the table, and the point of the whole scaled family.
    // The same readings, the same width, only the place x sits is different.
#if defined(SPTK_REAL_64)
    const uint32_t last_good = 23u;
#else
    const uint32_t last_good = 10u;
#endif
    real_t x[60];
    real_t y[60];
    real_t coefficients[40];

    for(uint32_t index = 0; index < 60u; index++)
    {
        real_t place = (real_t)index / REAL_C(59.0);

        x[index] = -REAL_C(1.0) + (REAL_C(2.0) * place);
        y[index] = REAL_SIN(REAL_C(3.0) * place);
    }

    TEST_ASSERT_EQUAL(true, lstsq_polyfit(x, y, 60u, last_good, coefficients));
}

void test_lstsq_refuses_fewer_readings_than_numbers_to_find(void)
{
    real_t x[3] = {REAL_C(0.0), REAL_C(1.0), REAL_C(2.0)};
    real_t y[3] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0)};
    real_t coefficients[8];

    TEST_ASSERT_EQUAL(false, lstsq_polyfit(x, y, 3u, 5u, coefficients));
}

void test_where_x_sits_matters_more_than_the_order(void)
{
    // THE TRAP THAT THE SCALED FIT EXISTS FOR.
    //
    // The same fifty readings, the same cubic, the same order. Only the place
    // the x sits is different, and the plain fit fails completely.
    const uint32_t size = 50u;
    real_t near_zero[50];
    real_t far_away[50];
    real_t y[50];
    real_t coefficients[8];
    real_t centre;
    real_t width;

    for(uint32_t index = 0; index < size; index++)
    {
        real_t place = (real_t)index / (real_t)(size - 1u);

        near_zero[index] = place;
        far_away[index] = REAL_C(1000.0) + place;
        y[index] = REAL_C(2.0) + (REAL_C(3.0) * place)
                   - (REAL_C(4.0) * place * place)
                   + (REAL_C(1.5) * place * place * place);
    }

    // Near zero the plain fit is fine.
    TEST_ASSERT_EQUAL(true, lstsq_polyfit(near_zero, y, size, 3u,
                                          coefficients));

    // A thousand away it fails, on the same readings, at the same order.
    TEST_ASSERT_EQUAL(false, lstsq_polyfit(far_away, y, size, 3u,
                                           coefficients));

    // The scaled fit brings x back to a range it can hold, and answers.
    TEST_ASSERT_EQUAL(true, lstsq_polyfit_scaled(far_away, y, size, 3u,
                                                 coefficients, &centre,
                                                 &width));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0),
                            lstsq_fit_quality_scaled(far_away, y, size,
                                                     coefficients, 3u, centre,
                                                     width));
}

void test_lstsq_scaling(void)
{
    real_t x[5] = {REAL_C(1000.0), REAL_C(1002.0), REAL_C(1004.0),
                   REAL_C(1006.0), REAL_C(1008.0)};
    real_t centre;
    real_t width;

    lstsq_scaling(x, 5u, &centre, &width);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1004.0), centre);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(4.0), width);

    // Readings that all share one x have no width. Giving 1 changes nothing.
    real_t same[3] = {REAL_C(5.0), REAL_C(5.0), REAL_C(5.0)};
    lstsq_scaling(same, 3u, &centre, &width);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0), width);
}

void test_a_scaled_fit_read_the_wrong_way_gives_nonsense(void)
{
    // The header warns that the coefficients of a scaled fit are NOT a
    // polynomial in x. This holds the warning true, so that nobody has to
    // find it out on a device.
    const uint32_t size = 20u;
    real_t x[20];
    real_t y[20];
    real_t coefficients[4];
    real_t centre;
    real_t width;

    for(uint32_t index = 0; index < size; index++)
    {
        x[index] = REAL_C(100.0) + (real_t)index;
        y[index] = REAL_C(5.0) + (REAL_C(2.0) * (real_t)index);
    }

    TEST_ASSERT_EQUAL(true, lstsq_polyfit_scaled(x, y, size, 2u, coefficients,
                                                 &centre, &width));

    // Read the right way, it follows the readings.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), y[10],
                            lstsq_evaluate_scaled(coefficients, 2u, centre,
                                                  width, x[10]));

    // Read as a plain polynomial in x, it does not.
    TEST_ASSERT_TRUE(REAL_ABS(lstsq_evaluate(coefficients, 2u, x[10]) - y[10])
                     > REAL_C(1.0));
}

void test_lstsq_solve_of_a_model_that_is_not_a_polynomial(void)
{
    // The general form. Here the reading is so much of one thing plus so much
    // of another, and the two amounts are wanted.
    matrix_t model = matrix_alloc(4, 2);
    matrix_t readings = matrix_alloc(4, 1);
    matrix_t answer = matrix_alloc(2, 1);
    matrix_t square = matrix_alloc(2, 2);
    matrix_t factor = matrix_alloc(2, 2);
    matrix_t column = matrix_alloc(2, 1);

    // Two things, measured four times in different amounts.
    real_t first[4] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(4.0)};
    real_t second[4] = {REAL_C(1.0), REAL_C(0.0), REAL_C(2.0), REAL_C(1.0)};

    for(uint32_t row = 0; row < 4u; row++)
    {
        matrix_add_element(&model, row, 0, first[row]);
        matrix_add_element(&model, row, 1, second[row]);
        // Three of the first and minus two of the second.
        matrix_add_element(&readings, row, 0,
                           (REAL_C(3.0) * first[row])
                           - (REAL_C(2.0) * second[row]));
    }

    TEST_ASSERT_EQUAL(true, lstsq_solve(&model, &readings, &answer, &square,
                                        &factor, &column));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(3.0),
                            matrix_get_element(&answer, 0, 0));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), -REAL_C(2.0),
                            matrix_get_element(&answer, 1, 0));

    matrix_free(&model);
    matrix_free(&readings);
    matrix_free(&answer);
    matrix_free(&square);
    matrix_free(&factor);
    matrix_free(&column);
}

void test_lstsq_solve_refuses_two_columns_that_say_the_same_thing(void)
{
    // Where two columns of the model are the same, no single answer exists:
    // any amount of one can be traded for the other. The module says so.
    matrix_t model = matrix_alloc(4, 2);
    matrix_t readings = matrix_alloc(4, 1);
    matrix_t answer = matrix_alloc(2, 1);
    matrix_t square = matrix_alloc(2, 2);
    matrix_t factor = matrix_alloc(2, 2);
    matrix_t column = matrix_alloc(2, 1);

    for(uint32_t row = 0; row < 4u; row++)
    {
        matrix_add_element(&model, row, 0, (real_t)(row + 1u));
        matrix_add_element(&model, row, 1, (real_t)(row + 1u));
        matrix_add_element(&readings, row, 0, (real_t)(row + 1u));
    }

    TEST_ASSERT_EQUAL(false, lstsq_solve(&model, &readings, &answer, &square,
                                         &factor, &column));

    matrix_free(&model);
    matrix_free(&readings);
    matrix_free(&answer);
    matrix_free(&square);
    matrix_free(&factor);
    matrix_free(&column);
}

void test_lstsq_solve_refuses_shapes_that_do_not_fit(void)
{
    matrix_t model = matrix_alloc(4, 2);
    matrix_t readings = matrix_alloc(3, 1);
    matrix_t answer = matrix_alloc(2, 1);
    matrix_t square = matrix_alloc(2, 2);
    matrix_t factor = matrix_alloc(2, 2);
    matrix_t column = matrix_alloc(2, 1);

    TEST_ASSERT_EQUAL(false, lstsq_solve(&model, &readings, &answer, &square,
                                         &factor, &column));

    matrix_free(&model);
    matrix_free(&readings);
    matrix_free(&answer);
    matrix_free(&square);
    matrix_free(&factor);
    matrix_free(&column);
}
