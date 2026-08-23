#include "unity.h"
#include "real_assert.h"
#include "quaternion.h"
#include "matrix.h"
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

void test_quaternion_identity_turns_nothing(void)
{
    quaternion_t q = quaternion_identity();
    real_t x;
    real_t y;
    real_t z;

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), quaternion_magnitude(q));

    quaternion_rotate(q, REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), &x, &y, &z);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), x);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(2.0), y);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(3.0), z);
}

void test_a_quarter_turn_about_z_takes_x_to_y(void)
{
    quaternion_t q = quaternion_from_axis_angle(REAL_C(0.0), REAL_C(0.0),
                                                REAL_C(1.0), PI / REAL_C(2.0));
    real_t x;
    real_t y;
    real_t z;

    quaternion_rotate(q, REAL_C(1.0), REAL_C(0.0), REAL_C(0.0), &x, &y, &z);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), x);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), y);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), z);
}

void test_the_axis_and_the_angle_come_back_out(void)
{
    quaternion_t q = quaternion_from_axis_angle(REAL_C(1.0), REAL_C(1.0),
                                                REAL_C(0.0), REAL_C(0.7));
    real_t x;
    real_t y;
    real_t z;
    real_t angle;

    quaternion_to_axis_angle(q, &x, &y, &z, &angle);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.7), angle);
    // The axis comes back of unit length, thus 1,1,0 becomes 0.707,0.707,0.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.70710678), x);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.70710678), y);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), z);
}

void test_a_turn_of_nothing_still_gives_an_axis_that_can_be_used(void)
{
    quaternion_t q = quaternion_identity();
    real_t x;
    real_t y;
    real_t z;
    real_t angle;

    quaternion_to_axis_angle(q, &x, &y, &z, &angle);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), angle);
    // There is no axis to speak of, and a caller must still get numbers it can
    // use rather than something that is not a number.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0),
                            REAL_SQRT((x * x) + (y * y) + (z * z)));
}

void test_an_axis_of_no_length_turns_nothing(void)
{
    quaternion_t q = quaternion_from_axis_angle(REAL_C(0.0), REAL_C(0.0),
                                                REAL_C(0.0), REAL_C(1.5));

    TEST_ASSERT_EQUAL(true, quaternion_is_same_attitude(q, quaternion_identity(),
                                                        TOLERANCE));
}

void test_a_quaternion_and_its_negative_are_the_same_attitude(void)
{
    // The half angle means a whole circle gives -1 and not 1. Every piece of
    // code that compares two attitudes must allow for that, and a great deal
    // of trouble comes from code that does not.
    quaternion_t q = quaternion_from_axis_angle(REAL_C(0.0), REAL_C(1.0),
                                                REAL_C(0.0), REAL_C(1.1));
    quaternion_t other_way = quaternion_scale(q, REAL_C(-1.0));

    TEST_ASSERT_EQUAL(true, quaternion_is_same_attitude(q, other_way, TOLERANCE));

    // The four numbers themselves are nothing alike.
    TEST_ASSERT_TRUE(REAL_ABS(q.w - other_way.w) > REAL_C(0.5));

    // And they turn a vector to the same place.
    real_t ax;
    real_t ay;
    real_t az;
    real_t bx;
    real_t by;
    real_t bz;

    quaternion_rotate(q, REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), &ax, &ay, &az);
    quaternion_rotate(other_way, REAL_C(1.0), REAL_C(2.0), REAL_C(3.0),
                      &bx, &by, &bz);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, ax, bx);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, ay, by);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, az, bz);
}

void test_a_turn_of_a_whole_circle_comes_back_to_where_it_started(void)
{
    quaternion_t q = quaternion_from_axis_angle(REAL_C(0.3), REAL_C(0.4),
                                                REAL_C(0.5),
                                                REAL_C(2.0) * PI);

    TEST_ASSERT_EQUAL(true, quaternion_is_same_attitude(q, quaternion_identity(),
                                                        TOLERANCE));
    // And its w is -1, not 1, which is what the half angle does.
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(-1.0), q.w);
}

void test_the_order_of_two_turns_matters(void)
{
    quaternion_t about_x = quaternion_from_axis_angle(REAL_C(1.0), REAL_C(0.0),
                                                      REAL_C(0.0),
                                                      PI / REAL_C(2.0));
    quaternion_t about_y = quaternion_from_axis_angle(REAL_C(0.0), REAL_C(1.0),
                                                      REAL_C(0.0),
                                                      PI / REAL_C(2.0));

    quaternion_t one_way = quaternion_multiply(about_x, about_y);
    quaternion_t other_way = quaternion_multiply(about_y, about_x);

    TEST_ASSERT_EQUAL(false, quaternion_is_same_attitude(one_way, other_way,
                                                         TOLERANCE));
}

void test_multiplying_is_one_turn_after_the_other(void)
{
    // Two quarter turns about z make a half turn about z.
    quaternion_t quarter = quaternion_from_axis_angle(REAL_C(0.0), REAL_C(0.0),
                                                      REAL_C(1.0),
                                                      PI / REAL_C(2.0));
    quaternion_t twice = quaternion_multiply(quarter, quarter);
    quaternion_t half = quaternion_from_axis_angle(REAL_C(0.0), REAL_C(0.0),
                                                   REAL_C(1.0), PI);

    TEST_ASSERT_EQUAL(true, quaternion_is_same_attitude(twice, half, TOLERANCE));
}

void test_the_conjugate_undoes_the_turn(void)
{
    quaternion_t q = quaternion_from_axis_angle(REAL_C(0.3), REAL_C(-0.5),
                                                REAL_C(0.8), REAL_C(1.3));
    quaternion_t back = quaternion_conjugate(q);

    quaternion_t nothing = quaternion_multiply(q, back);

    TEST_ASSERT_EQUAL(true, quaternion_is_same_attitude(nothing,
                                                        quaternion_identity(),
                                                        TOLERANCE));

    // A vector turned and then turned back is where it started.
    real_t x;
    real_t y;
    real_t z;

    quaternion_rotate(q, REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), &x, &y, &z);
    quaternion_rotate(back, x, y, z, &x, &y, &z);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0), x);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(2.0), y);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(3.0), z);
}

void test_rotating_keeps_the_length_of_a_vector(void)
{
    // A rotation turns a thing and does not stretch it. A quaternion that has
    // drifted from unit length WOULD stretch it, which is why the length must
    // be put back from time to time.
    quaternion_t q = quaternion_from_axis_angle(REAL_C(1.0), REAL_C(2.0),
                                                REAL_C(-1.0), REAL_C(0.9));
    real_t x;
    real_t y;
    real_t z;

    quaternion_rotate(q, REAL_C(3.0), REAL_C(-4.0), REAL_C(12.0), &x, &y, &z);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(13.0),
                            REAL_SQRT((x * x) + (y * y) + (z * z)));
}

void test_quaternion_rotate_may_write_over_its_input(void)
{
    quaternion_t q = quaternion_from_axis_angle(REAL_C(0.0), REAL_C(0.0),
                                                REAL_C(1.0), PI / REAL_C(2.0));
    real_t x = REAL_C(1.0);
    real_t y = REAL_C(0.0);
    real_t z = REAL_C(0.0);

    quaternion_rotate(q, x, y, z, &x, &y, &z);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(0.0), x);
    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0), y);
}

void test_the_matrix_and_the_quaternion_turn_a_vector_the_same_way(void)
{
    quaternion_t q = quaternion_from_axis_angle(REAL_C(0.4), REAL_C(-0.2),
                                                REAL_C(0.9), REAL_C(1.7));
    matrix_t rotation = matrix_alloc(3, 3);
    matrix_t vector = matrix_create_zero_matrix(3, 1);

    quaternion_to_matrix_into(q, &rotation);

    matrix_add_element(&vector, 0, 0, REAL_C(1.0));
    matrix_add_element(&vector, 1, 0, REAL_C(2.0));
    matrix_add_element(&vector, 2, 0, REAL_C(-3.0));

    matrix_t turned = matrix_multiply(&rotation, &vector);

    real_t x;
    real_t y;
    real_t z;
    quaternion_rotate(q, REAL_C(1.0), REAL_C(2.0), REAL_C(-3.0), &x, &y, &z);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), matrix_get_element(&turned, 0, 0), x);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), matrix_get_element(&turned, 1, 0), y);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), matrix_get_element(&turned, 2, 0), z);

    matrix_free(&rotation);
    matrix_free(&vector);
    matrix_free(&turned);
}

void test_the_way_out_and_the_way_back_agree_at_every_attitude(void)
{
    // The way back reads whichever of four forms is largest. Reading one form
    // always would lose its accuracy near a half turn, which is an ordinary
    // attitude. This walks all the way round three axes to hold that.
    matrix_t rotation = matrix_alloc(3, 3);

    for(uint32_t step = 0; step <= 16u; step++)
    {
        real_t angle = (REAL_C(2.0) * PI * (real_t)step) / REAL_C(16.0);

        real_t axes[3][3] = {{REAL_C(1.0), REAL_C(0.0), REAL_C(0.0)},
                             {REAL_C(0.0), REAL_C(1.0), REAL_C(0.0)},
                             {REAL_C(0.577), REAL_C(0.577), REAL_C(0.577)}};

        for(uint32_t which = 0; which < 3u; which++)
        {
            quaternion_t q = quaternion_from_axis_angle(axes[which][0],
                                                        axes[which][1],
                                                        axes[which][2], angle);

            quaternion_to_matrix_into(q, &rotation);
            quaternion_t back = quaternion_from_matrix(&rotation);

            TEST_ASSERT_EQUAL(true, quaternion_is_same_attitude(q, back,
                                                                REAL_C(0.001)));
        }
    }

    matrix_free(&rotation);
}

void test_the_matrix_of_an_attitude_is_a_rotation(void)
{
    // Its rows must be of unit length and at right angles, or it is a rotation
    // with a stretch in it.
    quaternion_t q = quaternion_from_axis_angle(REAL_C(0.3), REAL_C(0.6),
                                                REAL_C(-0.7), REAL_C(2.2));
    matrix_t rotation = matrix_alloc(3, 3);

    quaternion_to_matrix_into(q, &rotation);

    for(uint32_t i = 0; i < 3u; i++)
    {
        for(uint32_t j = 0; j < 3u; j++)
        {
            real_t together = REAL_C(0.0);
            for(uint32_t k = 0; k < 3u; k++)
            {
                together += matrix_get_element(&rotation, i, k)
                            * matrix_get_element(&rotation, j, k);
            }
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.001),
                                    (i == j) ? REAL_C(1.0) : REAL_C(0.0),
                                    together);
        }
    }

    matrix_free(&rotation);
}

void test_carrying_an_attitude_forward_by_a_turn_rate(void)
{
    // What a gyroscope gives: how fast the thing turns about each of its own
    // axes. A hundred steps of a hundredth of a second at one radian each
    // second is a turn of one radian.
    quaternion_t q = quaternion_identity();

    for(uint32_t step = 0; step < 100u; step++)
    {
        q = quaternion_integrate(q, REAL_C(0.0), REAL_C(0.0), REAL_C(1.0),
                                 REAL_C(0.01));
    }

    real_t angle;
    quaternion_to_axis_angle(q, NULL, NULL, NULL, &angle);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(1.0), angle);
    // And it stays of unit length, however many steps are taken.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(1.0),
                            quaternion_magnitude(q));
}

void test_carrying_forward_holds_its_length_over_a_long_run(void)
{
    // The one rule that four numbers must keep. A rotation matrix keeps six,
    // and arithmetic wears those away until it is no longer a rotation.
    quaternion_t q = quaternion_identity();

    for(uint32_t step = 0; step < 100000u; step++)
    {
        q = quaternion_integrate(q, REAL_C(0.3), REAL_C(-0.7), REAL_C(0.5),
                                 REAL_C(0.001));
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(1.0),
                            quaternion_magnitude(q));
}

void test_a_turn_rate_of_nothing_leaves_the_attitude_alone(void)
{
    quaternion_t q = quaternion_from_axis_angle(REAL_C(1.0), REAL_C(0.0),
                                                REAL_C(0.0), REAL_C(0.5));
    quaternion_t after = quaternion_integrate(q, REAL_C(0.0), REAL_C(0.0),
                                              REAL_C(0.0), REAL_C(0.01));

    TEST_ASSERT_EQUAL(true, quaternion_is_same_attitude(q, after, TOLERANCE));
}

void test_quaternion_slerp_at_its_two_ends(void)
{
    quaternion_t a = quaternion_identity();
    quaternion_t b = quaternion_from_axis_angle(REAL_C(0.0), REAL_C(0.0),
                                                REAL_C(1.0), PI / REAL_C(2.0));

    TEST_ASSERT_EQUAL(true, quaternion_is_same_attitude(
                          quaternion_slerp(a, b, REAL_C(0.0)), a, TOLERANCE));
    TEST_ASSERT_EQUAL(true, quaternion_is_same_attitude(
                          quaternion_slerp(a, b, REAL_C(1.0)), b, TOLERANCE));
}

void test_quaternion_slerp_turns_at_a_steady_rate(void)
{
    // The whole reason it exists. Adding the two and normalising is the
    // obvious shortcut and it hurries through the middle. This must cover
    // equal angles in equal steps.
    quaternion_t a = quaternion_identity();
    quaternion_t b = quaternion_from_axis_angle(REAL_C(0.0), REAL_C(0.0),
                                                REAL_C(1.0), REAL_C(2.0));

    real_t last = REAL_C(0.0);
    real_t first_step = REAL_C(0.0);

    for(uint32_t step = 1; step <= 8u; step++)
    {
        real_t part = (real_t)step / REAL_C(8.0);
        quaternion_t between = quaternion_slerp(a, b, part);

        real_t angle;
        quaternion_to_axis_angle(between, NULL, NULL, NULL, &angle);

        real_t moved = angle - last;
        if(step == 1u) { first_step = moved; }

        // Every step covers the same angle as the first.
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), first_step, moved);
        last = angle;
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(2.0), last);
}

void test_quaternion_slerp_takes_the_short_way_round(void)
{
    // Two attitudes that are a little apart, written so that their four
    // numbers point in opposite directions. Without allowing for the sign the
    // answer would take nearly a whole circle where a few degrees would do.
    quaternion_t a = quaternion_from_axis_angle(REAL_C(0.0), REAL_C(0.0),
                                                REAL_C(1.0), REAL_C(0.1));
    quaternion_t b = quaternion_scale(
        quaternion_from_axis_angle(REAL_C(0.0), REAL_C(0.0), REAL_C(1.0),
                                   REAL_C(0.3)), REAL_C(-1.0));

    quaternion_t middle = quaternion_slerp(a, b, REAL_C(0.5));

    real_t angle;
    quaternion_to_axis_angle(middle, NULL, NULL, NULL, &angle);

    // Half way between 0.1 and 0.3 radians.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.01), REAL_C(0.2), angle);
}

void test_quaternion_normalise_of_nothing_gives_a_usable_attitude(void)
{
    quaternion_t nothing = quaternion_make(REAL_C(0.0), REAL_C(0.0),
                                           REAL_C(0.0), REAL_C(0.0));

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0),
                            quaternion_magnitude(quaternion_normalise(nothing)));
}

void test_quaternion_normalise_puts_a_drifted_length_back(void)
{
    quaternion_t drifted = quaternion_make(REAL_C(2.0), REAL_C(0.0),
                                           REAL_C(0.0), REAL_C(0.0));
    quaternion_t put_back = quaternion_normalise(drifted);

    TEST_ASSERT_REAL_WITHIN(TOLERANCE, REAL_C(1.0),
                            quaternion_magnitude(put_back));
    TEST_ASSERT_EQUAL(true, quaternion_is_same_attitude(put_back,
                                                        quaternion_identity(),
                                                        TOLERANCE));
}
