#include "unity.h"
#include "real_assert.h"
#include "eigen.h"
#include "matrix.h"
#include <math.h>

void setUp(void)
{

}

void tearDown(void)
{

}

// Build a symmetric matrix by turning a known set of values, so that the right
// answer is known exactly rather than worked out a second way.
static void build_from(matrix_t* matrix, const real_t* wanted, uint32_t order,
                       uint32_t turns)
{
    matrix_set_zero(matrix);

    for(uint32_t index = 0; index < order; index++)
    {
        matrix_add_element(matrix, index, index, wanted[index]);
    }

    for(uint32_t turn = 0; turn < turns; turn++)
    {
        uint32_t p = turn % order;
        uint32_t q = (turn + 1u + (turn / order)) % order;

        if(p == q)
        {
            continue;
        }

        real_t angle = REAL_C(0.4) + (REAL_C(0.3) * (real_t)turn);
        real_t cosine = REAL_COS(angle);
        real_t sine = REAL_SIN(angle);

        for(uint32_t index = 0; index < order; index++)
        {
            real_t at_p = matrix_get_element(matrix, index, p);
            real_t at_q = matrix_get_element(matrix, index, q);

            matrix_add_element(matrix, index, p,
                               (cosine * at_p) - (sine * at_q));
            matrix_add_element(matrix, index, q,
                               (sine * at_p) + (cosine * at_q));
        }

        for(uint32_t index = 0; index < order; index++)
        {
            real_t p_at = matrix_get_element(matrix, p, index);
            real_t q_at = matrix_get_element(matrix, q, index);

            matrix_add_element(matrix, p, index,
                               (cosine * p_at) - (sine * q_at));
            matrix_add_element(matrix, q, index,
                               (sine * p_at) + (cosine * q_at));
        }
    }
}

void test_eigen_takes_a_symmetric_matrix_and_nothing_else(void)
{
    matrix_t square = matrix_create_unit_matrix(3);

    TEST_ASSERT_EQUAL(true, eigen_is_valid_matrix(&square));

    // One element changed on one side of the diagonal only.
    matrix_add_element(&square, 0, 2, REAL_C(5.0));
    TEST_ASSERT_EQUAL(false, eigen_is_valid_matrix(&square));

    matrix_add_element(&square, 2, 0, REAL_C(5.0));
    TEST_ASSERT_EQUAL(true, eigen_is_valid_matrix(&square));

    matrix_free(&square);

    matrix_t oblong = matrix_create_zero_matrix(2, 3);
    TEST_ASSERT_EQUAL(false, eigen_is_valid_matrix(&oblong));
    matrix_free(&oblong);
}

void test_a_matrix_that_only_stretches_the_axes_gives_those_amounts(void)
{
    // A diagonal matrix is already solved: it stretches each axis by the
    // number on that axis and turns nothing.
    matrix_t matrix = matrix_create_zero_matrix(3, 3);
    matrix_t vectors = matrix_create_zero_matrix(3, 3);
    real_t values[3];

    matrix_add_element(&matrix, 0, 0, REAL_C(2.0));
    matrix_add_element(&matrix, 1, 1, REAL_C(7.0));
    matrix_add_element(&matrix, 2, 2, REAL_C(4.0));

    TEST_ASSERT_EQUAL(true, eigen_solve(&matrix, values, &vectors));

    // Largest first.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(7.0), values[0]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(4.0), values[1]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(2.0), values[2]);

    // And the direction of the largest is the axis it stood on.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0),
                            REAL_ABS(matrix_get_element(&vectors, 1, 0)));

    matrix_free(&matrix);
    matrix_free(&vectors);
}

void test_the_values_that_come_back_are_the_ones_that_went_in(void)
{
    const uint32_t order = 4u;
    real_t wanted[4] = {REAL_C(9.0), REAL_C(5.0), REAL_C(3.0), REAL_C(1.0)};
    matrix_t matrix = matrix_create_zero_matrix(order, order);
    matrix_t vectors = matrix_create_zero_matrix(order, order);
    real_t values[4];

    build_from(&matrix, wanted, order, 12u);

    TEST_ASSERT_EQUAL(true, eigen_solve(&matrix, values, &vectors));

    for(uint32_t index = 0; index < order; index++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), wanted[index], values[index]);
    }

    matrix_free(&matrix);
    matrix_free(&vectors);
}

void test_the_matrix_really_stretches_each_direction_by_its_value(void)
{
    // THE TEST THAT SAYS THE ANSWER IS AN ANSWER.
    //
    // A value and a direction mean nothing apart. What they claim together is
    // that the matrix multiplied by the direction is the direction multiplied
    // by the value, and this holds them to it.
    const uint32_t order = 4u;
    real_t wanted[4] = {REAL_C(6.0), REAL_C(4.0), REAL_C(2.5), REAL_C(0.5)};
    matrix_t matrix = matrix_create_zero_matrix(order, order);
    matrix_t kept = matrix_create_zero_matrix(order, order);
    matrix_t vectors = matrix_create_zero_matrix(order, order);
    real_t values[4];

    build_from(&matrix, wanted, order, 9u);
    matrix_copy(&matrix, &kept);

    TEST_ASSERT_EQUAL(true, eigen_solve(&matrix, values, &vectors));

    for(uint32_t which = 0; which < order; which++)
    {
        for(uint32_t row = 0; row < order; row++)
        {
            real_t stretched = REAL_C(0.0);

            for(uint32_t column = 0; column < order; column++)
            {
                stretched += matrix_get_element(&kept, row, column)
                             * matrix_get_element(&vectors, column, which);
            }

            real_t scaled = values[which]
                            * matrix_get_element(&vectors, row, which);

            TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), scaled, stretched);
        }
    }

    matrix_free(&matrix);
    matrix_free(&kept);
    matrix_free(&vectors);
}

void test_every_direction_is_of_unit_length_and_at_right_angles(void)
{
    const uint32_t order = 4u;
    real_t wanted[4] = {REAL_C(8.0), REAL_C(5.0), REAL_C(2.0), REAL_C(1.0)};
    matrix_t matrix = matrix_create_zero_matrix(order, order);
    matrix_t vectors = matrix_create_zero_matrix(order, order);
    real_t values[4];

    build_from(&matrix, wanted, order, 7u);
    eigen_solve(&matrix, values, &vectors);

    for(uint32_t first = 0; first < order; first++)
    {
        for(uint32_t second = 0; second < order; second++)
        {
            real_t together = REAL_C(0.0);

            for(uint32_t row = 0; row < order; row++)
            {
                together += matrix_get_element(&vectors, row, first)
                            * matrix_get_element(&vectors, row, second);
            }

            // A direction with itself is 1, and with any other is nothing.
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.001),
                                    (first == second) ? REAL_C(1.0)
                                                      : REAL_C(0.0),
                                    together);
        }
    }

    matrix_free(&matrix);
    matrix_free(&vectors);
}

void test_the_values_add_up_to_the_trace(void)
{
    // What a matrix stretches by, added up, is what stands on its diagonal
    // added up. That holds however the matrix is turned, thus it is a check
    // that costs nothing and catches a great deal.
    const uint32_t order = 4u;
    real_t wanted[4] = {REAL_C(3.0), REAL_C(2.0), REAL_C(1.5), REAL_C(0.25)};
    matrix_t matrix = matrix_create_zero_matrix(order, order);
    real_t values[4];

    build_from(&matrix, wanted, order, 11u);

    real_t trace = matrix_trace(&matrix);

    TEST_ASSERT_EQUAL(true, eigen_solve(&matrix, values, NULL));

    real_t total = REAL_C(0.0);

    for(uint32_t index = 0; index < order; index++)
    {
        total += values[index];
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), trace, total);

    matrix_free(&matrix);
}

void test_eigen_solve_answers_without_the_directions(void)
{
    matrix_t matrix = matrix_create_zero_matrix(2, 2);
    real_t values[2];

    matrix_add_element(&matrix, 0, 0, REAL_C(4.0));
    matrix_add_element(&matrix, 0, 1, REAL_C(1.0));
    matrix_add_element(&matrix, 1, 0, REAL_C(1.0));
    matrix_add_element(&matrix, 1, 1, REAL_C(4.0));

    TEST_ASSERT_EQUAL(true, eigen_solve(&matrix, values, NULL));

    // A two by two of this shape stretches by 5 one way and 3 the other.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(5.0), values[0]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(3.0), values[1]);

    matrix_free(&matrix);
}

void test_eigen_condition(void)
{
    real_t even[3] = {REAL_C(2.0), REAL_C(2.0), REAL_C(2.0)};
    real_t uneven[3] = {REAL_C(100.0), REAL_C(10.0), REAL_C(1.0)};
    real_t squashed[3] = {REAL_C(5.0), REAL_C(1.0), REAL_C(0.0)};

    // A matrix that stretches everything alike is as good as one gets.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(1.0),
                            eigen_condition(even, 3u));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(100.0),
                            eigen_condition(uneven, 3u));

    // A direction squashed to nothing cannot be undone at all.
    TEST_ASSERT_TRUE(eigen_condition(squashed, 3u) >= REAL_LARGEST);
    TEST_ASSERT_TRUE(eigen_condition(even, 0u) >= REAL_LARGEST);

    // A negative value counts by how far it stands from nothing.
    real_t negative[2] = {-REAL_C(8.0), REAL_C(2.0)};
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(4.0),
                            eigen_condition(negative, 2u));
}

void test_eigen_rank(void)
{
    // How many directions the matrix really stretches.
    real_t values[4] = {REAL_C(10.0), REAL_C(5.0), REAL_C(0.0000001),
                        REAL_C(0.0)};

    TEST_ASSERT_EQUAL(2, eigen_rank(values, 4u, REAL_C(0.0001)));

    // A part of nothing counts every direction that is not exactly nothing,
    // and the last of these is exactly nothing.
    TEST_ASSERT_EQUAL(3, eigen_rank(values, 4u, REAL_C(0.0)));

    real_t full[3] = {REAL_C(3.0), REAL_C(2.0), REAL_C(1.0)};
    TEST_ASSERT_EQUAL(3, eigen_rank(full, 3u, REAL_C(0.0001)));
}

void test_eigen_part_held(void)
{
    // THE NUMBER THAT SAYS HOW MANY DIMENSIONS A MEASUREMENT REALLY HAS.
    real_t values[4] = {REAL_C(90.0), REAL_C(8.0), REAL_C(1.0), REAL_C(1.0)};

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.9),
                            eigen_part_held(values, 4u, 1u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.98),
                            eigen_part_held(values, 4u, 2u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0),
                            eigen_part_held(values, 4u, 4u));

    // Asking for more than there are gives all of them and not more.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0),
                            eigen_part_held(values, 4u, 9u));

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0),
                            eigen_part_held(values, 4u, 0u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0),
                            eigen_part_held(values, 0u, 1u));
}

void test_eigen_solve_refuses_what_it_cannot_answer(void)
{
    matrix_t matrix = matrix_create_unit_matrix(3);
    matrix_t wrong_order = matrix_create_zero_matrix(2, 2);
    real_t values[3];

    TEST_ASSERT_EQUAL(false, eigen_solve(&matrix, values, &wrong_order));

    matrix_add_element(&matrix, 0, 1, REAL_C(9.0));
    TEST_ASSERT_EQUAL(false, eigen_solve(&matrix, values, NULL));

    matrix_free(&matrix);
    matrix_free(&wrong_order);
}

void test_a_covariance_of_one_direction_shows_one_direction(void)
{
    // THE USE THAT MATTERS MOST. A sensor of three axes watching something
    // that moves along one line gives a covariance with one large value and
    // two small ones, and the direction beside the large one is the line.
    matrix_t covariance = matrix_create_zero_matrix(3, 3);
    matrix_t vectors = matrix_create_zero_matrix(3, 3);
    real_t values[3];

    // Movement along the direction 1, 1, 0, and a little noise everywhere.
    real_t along[3] = {REAL_C(1.0), REAL_C(1.0), REAL_C(0.0)};

    for(uint32_t row = 0; row < 3u; row++)
    {
        for(uint32_t column = 0; column < 3u; column++)
        {
            real_t value = REAL_C(100.0) * along[row] * along[column];

            if(row == column)
            {
                value += REAL_C(0.01);
            }

            matrix_add_element(&covariance, row, column, value);
        }
    }

    TEST_ASSERT_EQUAL(true, eigen_solve(&covariance, values, &vectors));

    // Nearly all of the spread lies in one direction.
    TEST_ASSERT_TRUE(eigen_part_held(values, 3u, 1u) > REAL_C(0.99));

    // And that direction is the line the movement lay along, which for a unit
    // length is 0.707 along each of the first two axes.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.7071),
                            REAL_ABS(matrix_get_element(&vectors, 0, 0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.7071),
                            REAL_ABS(matrix_get_element(&vectors, 1, 0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.0),
                            REAL_ABS(matrix_get_element(&vectors, 2, 0)));

    matrix_free(&covariance);
    matrix_free(&vectors);
}
