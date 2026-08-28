#include "unity.h"
#include "real_assert.h"
#include "curve.h"
#include <math.h>

// What every shape here has fallen to at one width from its middle.
#define AT_ONE_WIDTH    REAL_C(0.60653066)

void setUp(void)
{

}

void tearDown(void)
{

}

void test_curve_is_valid_width_and_shape(void)
{
    TEST_ASSERT_EQUAL(true, curve_is_valid_width(REAL_C(1.0)));
    TEST_ASSERT_EQUAL(true, curve_is_valid_width(REAL_C(0.001)));
    // A peak of no width at all is not a peak, and every shape divides by it.
    TEST_ASSERT_EQUAL(false, curve_is_valid_width(REAL_C(0.0)));
    TEST_ASSERT_EQUAL(false, curve_is_valid_width(-REAL_C(1.0)));

    TEST_ASSERT_EQUAL(true, curve_is_valid_shape(CURVE_GAUSSIAN));
    TEST_ASSERT_EQUAL(true, curve_is_valid_shape(CURVE_SKEWED_GAUSSIAN));
    TEST_ASSERT_EQUAL(false, curve_is_valid_shape((curve_shape_t)7));
    TEST_ASSERT_EQUAL(false, curve_is_valid_shape((curve_shape_t)-1));
}

// THE RULE THAT MAKES TWO WIDTHS COMPARABLE. Every shape here is written so
// that at one width from the middle it has fallen to the same share of its top,
// which is the share a normal spread has at one standard deviation. Without
// that a width of 2 would mean one thing for a gaussian and another for a
// lorentzian, and setting the two beside each other would say nothing.
void test_curve_every_shape_falls_to_the_same_share_at_one_width(void)
{
    real_t middles[3] = {REAL_C(0.0), REAL_C(5.0), -REAL_C(100.0)};
    real_t widths[3] = {REAL_C(0.25), REAL_C(1.0), REAL_C(40.0)};

    for(uint32_t m = 0; m < 3u; m++)
    {
        for(uint32_t w = 0; w < 3u; w++)
        {
            real_t at = middles[m] + widths[w];

            TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), AT_ONE_WIDTH,
                                    curve_gaussian(at, middles[m], widths[w]));
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), AT_ONE_WIDTH,
                                    curve_lorentzian(at, middles[m],
                                                     widths[w]));
            // And the same the other side.
            at = middles[m] - widths[w];

            TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), AT_ONE_WIDTH,
                                    curve_gaussian(at, middles[m], widths[w]));
            TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), AT_ONE_WIDTH,
                                    curve_lorentzian(at, middles[m],
                                                     widths[w]));
        }
    }
}

void test_curve_every_shape_stands_at_one_at_its_top(void)
{
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(1.0),
                            curve_gaussian(REAL_C(3.0), REAL_C(3.0),
                                           REAL_C(2.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(1.0),
                            curve_lorentzian(REAL_C(3.0), REAL_C(3.0),
                                             REAL_C(2.0)));

    // The skewed shape stands at one at ITS top, which is not its middle.
    real_t skews[4] = {REAL_C(0.5), REAL_C(2.0), REAL_C(8.0), -REAL_C(3.0)};

    for(uint32_t which = 0; which < 4u; which++)
    {
        real_t top = curve_skewed_gaussian_top(REAL_C(3.0), REAL_C(2.0),
                                               skews[which]);

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), REAL_C(1.0),
                                curve_skewed_gaussian(top, REAL_C(3.0),
                                                      REAL_C(2.0),
                                                      skews[which]));
    }
}

// THE TAILS ARE THE WHOLE REASON BOTH EVEN SHAPES ARE HERE. These are the
// measured shares of the top at each distance from the middle, in widths.
void test_curve_the_lorentzian_holds_far_more_in_its_tails(void)
{
    real_t widths[4] = {REAL_C(2.0), REAL_C(3.0), REAL_C(5.0), REAL_C(10.0)};
    real_t gaussian[4] = {REAL_C(0.135335), REAL_C(0.011109),
                          REAL_C(0.000004), REAL_C(0.0)};
    real_t lorentzian[4] = {REAL_C(0.278173), REAL_C(0.146231),
                            REAL_C(0.058079), REAL_C(0.015181)};

    for(uint32_t which = 0; which < 4u; which++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), gaussian[which],
                                curve_gaussian(widths[which], REAL_C(0.0),
                                               REAL_C(1.0)));
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001), lorentzian[which],
                                curve_lorentzian(widths[which], REAL_C(0.0),
                                                 REAL_C(1.0)));

        // And the lorentzian holds more at every distance past one width.
        TEST_ASSERT_TRUE(curve_lorentzian(widths[which], REAL_C(0.0),
                                          REAL_C(1.0))
                         > curve_gaussian(widths[which], REAL_C(0.0),
                                          REAL_C(1.0)));
    }
}

// A skew of nothing is the plain gaussian and must give exactly it, or the two
// cannot be set beside each other.
void test_curve_a_skew_of_nothing_is_the_plain_gaussian(void)
{
    for(int32_t step = -40; step <= 40; step++)
    {
        real_t at = REAL_C(2.0) + ((real_t)step / REAL_C(10.0));

        TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001),
                                curve_gaussian(at, REAL_C(2.0), REAL_C(1.0)),
                                curve_skewed_gaussian(at, REAL_C(2.0),
                                                      REAL_C(1.0),
                                                      REAL_C(0.0)));
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(2.0),
                            curve_skewed_gaussian_top(REAL_C(2.0),
                                                      REAL_C(1.0),
                                                      REAL_C(0.0)));
}

// THE NUMBER A PEAK FITTER IS TRYING TO FIND. The top of a skewed peak does not
// stand at its middle, and how far it stands from it is exactly what a fitter
// that assumes an even peak gets wrong.
void test_curve_the_top_of_a_skewed_peak_stands_away_from_its_middle(void)
{
    real_t skews[5] = {REAL_C(0.5), REAL_C(1.0), REAL_C(2.0), REAL_C(4.0),
                       REAL_C(8.0)};
    real_t tops[5] = {REAL_C(0.3454), REAL_C(0.5060), REAL_C(0.5306),
                      REAL_C(0.4169), REAL_C(0.2770)};

    for(uint32_t which = 0; which < 5u; which++)
    {
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), tops[which],
                                curve_skewed_gaussian_top(REAL_C(0.0),
                                                          REAL_C(1.0),
                                                          skews[which]));

        // And turning the skew round mirrors the shape exactly.
        TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), -tops[which],
                                curve_skewed_gaussian_top(REAL_C(0.0),
                                                          REAL_C(1.0),
                                                          -skews[which]));
    }

    // It really is the top: nothing either side of it stands higher.
    for(uint32_t which = 0; which < 5u; which++)
    {
        real_t top = curve_skewed_gaussian_top(REAL_C(0.0), REAL_C(1.0),
                                               skews[which]);
        real_t tallest = curve_skewed_gaussian(top, REAL_C(0.0), REAL_C(1.0),
                                               skews[which]);

        for(int32_t step = -50; step <= 50; step++)
        {
            real_t at = top + ((real_t)step / REAL_C(10.0));

            TEST_ASSERT_TRUE(curve_skewed_gaussian(at, REAL_C(0.0),
                                                   REAL_C(1.0), skews[which])
                             <= (tallest + REAL_C(0.000001)));
        }
    }
}

// A skewed peak leans: it carries more on the side its tail is on.
void test_curve_a_skewed_peak_carries_more_on_its_long_side(void)
{
    real_t middle = REAL_C(0.0);

    real_t above = REAL_C(0.0);
    real_t below = REAL_C(0.0);

    for(int32_t step = 1; step <= 400; step++)
    {
        real_t away = (real_t)step / REAL_C(50.0);

        above += curve_skewed_gaussian(middle + away, middle, REAL_C(1.0),
                                       REAL_C(3.0));
        below += curve_skewed_gaussian(middle - away, middle, REAL_C(1.0),
                                       REAL_C(3.0));
    }

    TEST_ASSERT_TRUE(above > (REAL_C(2.0) * below));
}

void test_curve_value_agrees_with_each_shape_by_name(void)
{
    real_t at = REAL_C(1.3);
    real_t middle = REAL_C(0.4);
    real_t width = REAL_C(0.9);
    real_t skew = REAL_C(2.5);

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001),
                            curve_gaussian(at, middle, width),
                            curve_value(CURVE_GAUSSIAN, at, middle, width,
                                        skew));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001),
                            curve_lorentzian(at, middle, width),
                            curve_value(CURVE_LORENTZIAN, at, middle, width,
                                        skew));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001),
                            curve_skewed_gaussian(at, middle, width, skew),
                            curve_value(CURVE_SKEWED_GAUSSIAN, at, middle,
                                        width, skew));

    // A shape that is not a shape gives nothing rather than one of the others.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            curve_value((curve_shape_t)9, at, middle, width,
                                        skew));
}

// The block is there for speed and must be there for nothing else.
void test_curve_a_block_is_the_places_read_one_at_a_time(void)
{
    real_t written[101];
    curve_shape_t shapes[3] = {CURVE_GAUSSIAN, CURVE_LORENTZIAN,
                               CURVE_SKEWED_GAUSSIAN};

    for(uint32_t which = 0; which < 3u; which++)
    {
        TEST_ASSERT_EQUAL(true, curve_block(shapes[which], -REAL_C(5.0),
                                            REAL_C(5.0), REAL_C(0.5),
                                            REAL_C(1.5), REAL_C(2.0),
                                            written, 101u));

        for(uint32_t index = 0; index < 101u; index++)
        {
            real_t at = -REAL_C(5.0)
                        + ((REAL_C(10.0) / REAL_C(100.0)) * (real_t)index);

            TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001),
                                    curve_value(shapes[which], at,
                                                REAL_C(0.5), REAL_C(1.5),
                                                REAL_C(2.0)),
                                    written[index]);
        }
    }

    // The two ends of the range are both reached.
    TEST_ASSERT_EQUAL(true, curve_block(CURVE_GAUSSIAN, REAL_C(0.0),
                                        REAL_C(4.0), REAL_C(0.0),
                                        REAL_C(1.0), REAL_C(0.0), written,
                                        5u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(1.0), written[0]);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.0001),
                            curve_gaussian(REAL_C(4.0), REAL_C(0.0),
                                           REAL_C(1.0)),
                            written[4]);

    // One place gives the place it started at.
    TEST_ASSERT_EQUAL(true, curve_block(CURVE_GAUSSIAN, REAL_C(2.0),
                                        REAL_C(9.0), REAL_C(2.0),
                                        REAL_C(1.0), REAL_C(0.0), written,
                                        1u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(1.0), written[0]);
}

void test_curve_refuses_what_it_cannot_read(void)
{
    real_t written[8];

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            curve_gaussian(REAL_C(1.0), REAL_C(0.0),
                                           REAL_C(0.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            curve_lorentzian(REAL_C(1.0), REAL_C(0.0),
                                             -REAL_C(2.0)));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            curve_skewed_gaussian(REAL_C(1.0), REAL_C(0.0),
                                                  REAL_C(0.0), REAL_C(1.0)));

    // A width it cannot use leaves the top where the middle is, because there
    // is no shape to find a top of.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(7.0),
                            curve_skewed_gaussian_top(REAL_C(7.0),
                                                      REAL_C(0.0),
                                                      REAL_C(3.0)));

    TEST_ASSERT_EQUAL(false, curve_block(CURVE_GAUSSIAN, REAL_C(0.0),
                                         REAL_C(1.0), REAL_C(0.0),
                                         REAL_C(0.0), REAL_C(0.0), written,
                                         8u));
    TEST_ASSERT_EQUAL(false, curve_block(CURVE_GAUSSIAN, REAL_C(0.0),
                                         REAL_C(1.0), REAL_C(0.0),
                                         REAL_C(1.0), REAL_C(0.0), written,
                                         0u));
    TEST_ASSERT_EQUAL(false, curve_block((curve_shape_t)9, REAL_C(0.0),
                                         REAL_C(1.0), REAL_C(0.0),
                                         REAL_C(1.0), REAL_C(0.0), written,
                                         8u));
}
