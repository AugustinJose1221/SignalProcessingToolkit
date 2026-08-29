#include "unity.h"
#include "real_assert.h"
#include "peakdetect.h"
#include "curve.h"

void setUp(void)
{

}

void tearDown(void)
{

}

void test_peakdetect_get_peaks(void)
{
    real_t input[5] = {1, 2, 3, 2, 1};
    real_t index_buffer[5];
    real_t peak_buffer[5];
    uint32_t peakcount = peakdetect_get_peaks(input, index_buffer, peak_buffer, 5);
    TEST_ASSERT_EQUAL(1, peakcount);
    TEST_ASSERT_EQUAL(3, peak_buffer[0]);
    TEST_ASSERT_EQUAL(2, index_buffer[0]);

    real_t input2[2] = {1, 2};
    real_t index_buffer2[2];
    real_t peak_buffer2[2];
    uint32_t peakcount2 = peakdetect_get_peaks(input2, index_buffer2, peak_buffer2, 2);
    TEST_ASSERT_EQUAL(0, peakcount2);
}

void test_peakdetect_no_rules_lets_everything_through(void)
{
    // Three peaks of very different sizes, one of them negative.
    real_t signal[11] = {REAL_C(-5.0), REAL_C(-3.0), REAL_C(-5.0),
                         REAL_C(0.0), REAL_C(0.1), REAL_C(0.0),
                         REAL_C(0.0), REAL_C(9.0), REAL_C(0.0),
                         REAL_C(0.0), REAL_C(0.0)};
    uint32_t found[11];

    peakdetect_options_t options = peakdetect_no_rules();

    TEST_ASSERT_EQUAL(3, peakdetect_find(signal, 11u, &options, found, 11u));
    TEST_ASSERT_EQUAL(1, found[0]);
    TEST_ASSERT_EQUAL(4, found[1]);
    TEST_ASSERT_EQUAL(7, found[2]);
}

void test_a_flat_top_counts_as_one_peak(void)
{
    // THIS IS WHAT A READING FROM A CONVERTER LOOKS LIKE. The top of a peak is
    // a whole number of counts, thus two or three samples hold exactly the
    // same value. A test of "larger than both neighbours" finds no peak here
    // at all and loses it completely.
    real_t signal[9] = {REAL_C(0.0), REAL_C(1.0), REAL_C(5.0), REAL_C(5.0),
                        REAL_C(5.0), REAL_C(1.0), REAL_C(0.0), REAL_C(0.0),
                        REAL_C(0.0)};
    uint32_t found[9];

    peakdetect_options_t options = peakdetect_no_rules();

    TEST_ASSERT_EQUAL(1, peakdetect_find(signal, 9u, &options, found, 9u));
    // The middle of the flat part.
    TEST_ASSERT_EQUAL(3, found[0]);

    // The older function, which asks only whether a sample is larger than both
    // its neighbours, finds nothing here at all.
    real_t index_buffer[9];
    real_t peak_buffer[9];
    TEST_ASSERT_EQUAL(0, peakdetect_get_peaks(signal, index_buffer,
                                              peak_buffer, 9u));
}

void test_peakdetect_prominence_of_a_lone_peak(void)
{
    // A peak standing alone in a flat signal. Its prominence is its whole
    // height above the flat.
    real_t signal[7] = {REAL_C(1.0), REAL_C(1.0), REAL_C(1.0), REAL_C(6.0),
                        REAL_C(1.0), REAL_C(1.0), REAL_C(1.0)};

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(5.0),
                            peakdetect_prominence(signal, 7u, 3u));
}

void test_a_wobble_on_a_large_peak_has_almost_no_prominence(void)
{
    // THE WHOLE REASON PROMINENCE IS THE RULE TO REACH FOR.
    //
    // The small peak here stands HIGHER than the lone peak of the test above,
    // and it is not a peak of the signal at all: it is a wobble on the side of
    // a much larger one. Height cannot tell the two apart. Prominence can.
    real_t signal[11] = {REAL_C(0.0), REAL_C(2.0), REAL_C(4.0), REAL_C(7.9),
                         REAL_C(7.7), REAL_C(9.0), REAL_C(6.0), REAL_C(3.0),
                         REAL_C(0.0), REAL_C(0.0), REAL_C(0.0)};

    real_t wobble = peakdetect_prominence(signal, 11u, 3u);
    real_t real_peak = peakdetect_prominence(signal, 11u, 5u);

    // The wobble stands at 7.9, which is higher than the lone peak of 6.0 in
    // the test above, and it stands out by only 0.2.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.2), wobble);
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(9.0), real_peak);

    // One rule keeps the real peak and throws the wobble away, although the
    // wobble is the taller of the two against a height rule.
    uint32_t found[11];
    peakdetect_options_t options = peakdetect_no_rules();
    options.minimum_prominence = REAL_C(1.0);

    TEST_ASSERT_EQUAL(1, peakdetect_find(signal, 11u, &options, found, 11u));
    TEST_ASSERT_EQUAL(5, found[0]);
}

void test_prominence_does_not_care_where_the_signal_sits(void)
{
    // A drifting level defeats a height rule and does not defeat prominence.
    real_t low[9] = {REAL_C(0.0), REAL_C(0.0), REAL_C(3.0), REAL_C(0.0),
                     REAL_C(0.0), REAL_C(0.0), REAL_C(0.0), REAL_C(0.0),
                     REAL_C(0.0)};
    real_t high[9] = {REAL_C(100.0), REAL_C(100.0), REAL_C(103.0),
                      REAL_C(100.0), REAL_C(100.0), REAL_C(100.0),
                      REAL_C(100.0), REAL_C(100.0), REAL_C(100.0)};

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001),
                            peakdetect_prominence(low, 9u, 2u),
                            peakdetect_prominence(high, 9u, 2u));
}

void test_peakdetect_width(void)
{
    // A triangle 4 samples wide at its base, rising from 0 to 4. At half its
    // prominence, which is 2, it is half as wide.
    real_t signal[9] = {REAL_C(0.0), REAL_C(0.0), REAL_C(2.0), REAL_C(4.0),
                        REAL_C(2.0), REAL_C(0.0), REAL_C(0.0), REAL_C(0.0),
                        REAL_C(0.0)};

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.1), REAL_C(2.0),
                            peakdetect_width(signal, 9u, 3u, REAL_C(0.5)));
}

void test_a_width_rule_throws_away_a_spike(void)
{
    // A spike one sample wide is noise. A peak thirty samples wide is a
    // heartbeat. A width rule keeps the second and not the first.
    real_t signal[41];
    uint32_t found[41];

    for(uint32_t index = 0; index < 41u; index++) { signal[index] = REAL_C(0.0); }

    // A wide, smooth peak.
    for(uint32_t index = 5; index < 25u; index++)
    {
        real_t part = (real_t)(index - 5u) / REAL_C(20.0);
        signal[index] = REAL_C(5.0) * REAL_SIN(REAL_C(3.14159265) * part);
    }
    // A spike of one sample, taller than the wide peak.
    signal[35] = REAL_C(7.0);

    peakdetect_options_t options = peakdetect_no_rules();
    options.minimum_width = REAL_C(3.0);

    TEST_ASSERT_EQUAL(1, peakdetect_find(signal, 41u, &options, found, 41u));
    TEST_ASSERT_TRUE(found[0] > 10u);
    TEST_ASSERT_TRUE(found[0] < 20u);
}

void test_a_distance_rule_keeps_the_taller_of_two_that_stand_too_close(void)
{
    // A heart cannot beat twice in 200 ms, thus a second peak inside that is
    // the same beat counted twice.
    real_t signal[15] = {REAL_C(0.0), REAL_C(0.0), REAL_C(9.0), REAL_C(0.0),
                         REAL_C(6.0), REAL_C(0.0), REAL_C(0.0), REAL_C(0.0),
                         REAL_C(0.0), REAL_C(0.0), REAL_C(0.0), REAL_C(8.0),
                         REAL_C(0.0), REAL_C(0.0), REAL_C(0.0)};
    uint32_t found[15];

    peakdetect_options_t loose = peakdetect_no_rules();
    TEST_ASSERT_EQUAL(3, peakdetect_find(signal, 15u, &loose, found, 15u));

    peakdetect_options_t options = peakdetect_no_rules();
    options.minimum_distance = 5u;

    TEST_ASSERT_EQUAL(2, peakdetect_find(signal, 15u, &options, found, 15u));
    // The taller of the close pair stays, and the far one is untouched.
    TEST_ASSERT_EQUAL(2, found[0]);
    TEST_ASSERT_EQUAL(11, found[1]);
}

void test_a_height_rule_throws_away_what_is_too_low(void)
{
    real_t signal[11] = {REAL_C(0.0), REAL_C(1.0), REAL_C(0.0), REAL_C(0.0),
                         REAL_C(5.0), REAL_C(0.0), REAL_C(0.0), REAL_C(0.0),
                         REAL_C(9.0), REAL_C(0.0), REAL_C(0.0)};
    uint32_t found[11];

    peakdetect_options_t options = peakdetect_no_rules();
    options.minimum_height = REAL_C(4.0);

    TEST_ASSERT_EQUAL(2, peakdetect_find(signal, 11u, &options, found, 11u));
    TEST_ASSERT_EQUAL(4, found[0]);
    TEST_ASSERT_EQUAL(8, found[1]);
}

void test_the_answer_always_stands_in_the_order_of_the_signal(void)
{
    real_t signal[21];
    uint32_t found[21];

    for(uint32_t index = 0; index < 21u; index++) { signal[index] = REAL_C(0.0); }
    signal[3] = REAL_C(2.0);
    signal[8] = REAL_C(9.0);
    signal[13] = REAL_C(1.0);
    signal[18] = REAL_C(5.0);

    peakdetect_options_t options = peakdetect_no_rules();
    uint32_t count = peakdetect_find(signal, 21u, &options, found, 21u);

    for(uint32_t index = 1; index < count; index++)
    {
        TEST_ASSERT_TRUE(found[index] > found[index - 1u]);
    }
}

void test_when_there_is_no_room_the_ones_that_stand_out_most_are_kept(void)
{
    real_t signal[21];
    uint32_t found[2];

    for(uint32_t index = 0; index < 21u; index++) { signal[index] = REAL_C(0.0); }
    signal[3] = REAL_C(2.0);
    signal[8] = REAL_C(9.0);
    signal[13] = REAL_C(1.0);
    signal[18] = REAL_C(5.0);

    peakdetect_options_t options = peakdetect_no_rules();

    TEST_ASSERT_EQUAL(2, peakdetect_find(signal, 21u, &options, found, 2u));
    // The two largest, still in the order they stand in the signal.
    TEST_ASSERT_EQUAL(8, found[0]);
    TEST_ASSERT_EQUAL(18, found[1]);
}

void test_peakdetect_find_on_a_signal_too_short_to_hold_a_peak(void)
{
    real_t signal[2] = {REAL_C(1.0), REAL_C(2.0)};
    uint32_t found[2];

    peakdetect_options_t options = peakdetect_no_rules();

    TEST_ASSERT_EQUAL(0, peakdetect_find(signal, 2u, &options, found, 2u));
    TEST_ASSERT_EQUAL(0, peakdetect_find(signal, 2u, &options, found, 0u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0),
                            peakdetect_prominence(signal, 2u, 1u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.001), REAL_C(0.0),
                            peakdetect_width(signal, 2u, 1u, REAL_C(0.5)));
}

void test_peakdetect_finds_the_beats_of_a_heart_where_a_local_maximum_cannot(void)
{
    // The case this was built for. A signal of beats with noise on it. Every
    // local maximum finds a hundred; the rules find the beats.
    const uint32_t size = 1000u;
    static real_t signal[1000];
    static uint32_t found[1000];
    uint32_t seed = 7u;

    for(uint32_t index = 0; index < size; index++)
    {
        seed = (seed * 1103515245u) + 12345u;
        real_t noise = ((real_t)((seed >> 16) % 2000u) / REAL_C(1000.0))
                       - REAL_C(1.0);
        signal[index] = REAL_C(0.15) * noise;
    }

    // Ten beats, 100 samples apart, each 21 samples wide.
    for(uint32_t beat = 0; beat < 10u; beat++)
    {
        uint32_t at = 50u + (beat * 100u);
        for(uint32_t k = 0; k < 21u; k++)
        {
            real_t part = (real_t)k / REAL_C(20.0);
            signal[(at + k) - 10u] += REAL_C(3.0)
                                      * REAL_SIN(REAL_C(3.14159265) * part);
        }
    }

    // Every local maximum: far too many.
    peakdetect_options_t none = peakdetect_no_rules();
    uint32_t all_of_them = peakdetect_find(signal, size, &none, found, size);
    TEST_ASSERT_TRUE(all_of_them > 100u);

    // The rules: exactly the beats.
    peakdetect_options_t options = peakdetect_no_rules();
    options.minimum_prominence = REAL_C(1.0);
    options.minimum_distance = 50u;

    uint32_t beats = peakdetect_find(signal, size, &options, found, size);

    TEST_ASSERT_EQUAL(10, beats);
    for(uint32_t beat = 0; beat < 10u; beat++)
    {
        // Each one within a sample or two of where it was put.
        uint32_t expected = 50u + (beat * 100u);
        uint32_t apart = (found[beat] > expected) ? (found[beat] - expected)
                                                  : (expected - found[beat]);
        TEST_ASSERT_TRUE(apart <= 3u);
    }
}

// A peak almost never stands on a sample, and rounding to the nearest one is
// out by up to half a sample in every measurement equally.
void test_peakdetect_refine_finds_the_top_between_two_samples(void)
{
    // A curve of the second order whose top stands a quarter of a sample to
    // the right of the middle point. Three points fix it exactly, thus the
    // answer must be exact.
    real_t values[3];

    for(uint32_t index = 0; index < 3u; index++)
    {
        real_t at = (real_t)index - REAL_C(1.0) - REAL_C(0.25);

        values[index] = REAL_C(4.0) - (at * at);
    }

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.25),
                            peakdetect_refine(values, 3u, 1u));

    // And it says how high the top reaches, which the largest sample
    // under-reports: the sample stands a quarter away, thus it is lower by the
    // square of a quarter.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(4.0),
                            peakdetect_refine_height(values, 3u, 1u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(4.0) - REAL_C(0.0625),
                            values[1]);

    // Three even points have their top on the middle one.
    real_t even[3] = {REAL_C(1.0), REAL_C(2.0), REAL_C(1.0)};

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            peakdetect_refine(even, 3u, 1u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(2.0),
                            peakdetect_refine_height(even, 3u, 1u));
}

// There are not three points at either end, and three points that do not bend
// downwards hold no top between them.
void test_peakdetect_refine_gives_nothing_where_there_is_no_peak(void)
{
    real_t rising[4] = {REAL_C(1.0), REAL_C(2.0), REAL_C(3.0), REAL_C(4.0)};

    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            peakdetect_refine(rising, 4u, 0u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            peakdetect_refine(rising, 4u, 3u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            peakdetect_refine(rising, 4u, 2u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(0.0),
                            peakdetect_refine(rising, 2u, 1u));

    // With nothing to fit, the height that can be said is the sample itself.
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(3.0),
                            peakdetect_refine_height(rising, 4u, 2u));
    TEST_ASSERT_REAL_WITHIN(REAL_C(0.000001), REAL_C(1.0),
                            peakdetect_refine_height(rising, 4u, 0u));
}

// THE NUMBERS IN THE HEADER, HELD TO. A peak whose top is known exactly is
// sampled five to a width with the top moved between two samples, and what the
// refinement says is set against where the top really stands.
void test_peakdetect_refine_beats_rounding_on_every_shape(void)
{
    curve_shape_t shapes[3] = {CURVE_GAUSSIAN, CURVE_LORENTZIAN,
                               CURVE_SKEWED_GAUSSIAN};
    real_t skews[3] = {REAL_C(0.0), REAL_C(0.0), REAL_C(2.0)};
    // The worst the refinement was measured to be, in samples.
    real_t allowed[3] = {REAL_C(0.005), REAL_C(0.010), REAL_C(0.050)};

    for(uint32_t which = 0; which < 3u; which++)
    {
        real_t width = REAL_C(1.0);
        real_t step = width / REAL_C(5.0);

        // THE WORST ACROSS EVERY PLACE THE TOP CAN FALL, and not the error at
        // each place on its own. Where the top happens to land almost exactly
        // ON a sample, rounding to that sample is already perfect and refining
        // can only add a little to it. What the refinement is for is the other
        // places, and the worst is what says whether it is worth doing.
        real_t worst_refined = REAL_C(0.0);
        real_t worst_rounded = REAL_C(0.0);
        real_t worst_height = REAL_C(0.0);
        real_t worst_largest = REAL_C(0.0);

        for(uint32_t moved = 0; moved < 20u; moved++)
        {
            real_t middle = ((real_t)moved / REAL_C(20.0)) * step;
            real_t top = (shapes[which] == CURVE_SKEWED_GAUSSIAN)
                         ? curve_skewed_gaussian_top(middle, width,
                                                     skews[which])
                         : middle;

            real_t values[41];
            uint32_t best = 0u;

            for(uint32_t index = 0; index < 41u; index++)
            {
                real_t at = ((real_t)index - REAL_C(20.0)) * step;

                values[index] = curve_value(shapes[which], at, middle, width,
                                            skews[which]);

                if(values[index] > values[best]) { best = index; }
            }

            real_t at_best = ((real_t)best - REAL_C(20.0)) * step;
            real_t refined = at_best
                             + (peakdetect_refine(values, 41u, best) * step);
            real_t height = peakdetect_refine_height(values, 41u, best);

            real_t off_refined = REAL_ABS(refined - top) / step;
            real_t off_rounded = REAL_ABS(at_best - top) / step;

            if(off_refined > worst_refined) { worst_refined = off_refined; }
            if(off_rounded > worst_rounded) { worst_rounded = off_rounded; }

            real_t off_height = REAL_ABS(height - REAL_C(1.0));
            real_t off_largest = REAL_ABS(values[best] - REAL_C(1.0));

            if(off_height > worst_height) { worst_height = off_height; }
            if(off_largest > worst_largest) { worst_largest = off_largest; }
        }

        // Inside what the header promises, and far better than rounding.
        TEST_ASSERT_TRUE(worst_refined <= allowed[which]);
        TEST_ASSERT_TRUE(worst_refined < (REAL_C(0.25) * worst_rounded));

        // Rounding really is out by about half a sample at its worst, which is
        // what makes the comparison worth making.
        TEST_ASSERT_TRUE(worst_rounded > REAL_C(0.4));

        // And the height it gives beats the largest sample by a wide margin.
        TEST_ASSERT_TRUE(worst_height < (REAL_C(0.5) * worst_largest));
    }
}

// THE PLACES WHERE A PEAK IS NOT A PEAK, AND THE LIST THAT IS TOO SMALL.
//
// A detector is given whatever the signal happens to be, and the awkward cases
// arrive on their own: a top that sits at the very first or last sample, a
// flat run that reaches the end, a signal that never comes back down. Each has
// one right answer and it is fixed here.

void test_the_first_and_the_last_sample_are_never_peaks(void)
{
    // A peak must stand above a neighbour on BOTH sides. The samples at the
    // two ends have only one neighbour, thus nothing can be said about them.
    real_t rising[5] = {REAL_C(9.0), REAL_C(1.0), REAL_C(2.0), REAL_C(3.0),
                        REAL_C(9.0)};

    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0),
                           peakdetect_prominence(rising, 5u, 0u));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0),
                           peakdetect_prominence(rising, 5u, 4u));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0),
                           peakdetect_prominence(rising, 5u, 9u));
}

void test_a_flat_top_that_runs_to_the_end_is_not_a_peak(void)
{
    // The signal rises to a level and stays there. Nothing says it ever comes
    // down again, thus it is a step and not a peak.
    real_t step[6] = {REAL_C(0.0), REAL_C(1.0), REAL_C(5.0), REAL_C(5.0),
                      REAL_C(5.0), REAL_C(5.0)};

    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), peakdetect_prominence(step, 6u, 2u));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), peakdetect_width(step, 6u, 2u,
                                                          REAL_C(0.5)));
}

void test_the_width_of_something_that_is_not_a_peak_is_nothing(void)
{
    real_t flat[5] = {REAL_C(2.0), REAL_C(2.0), REAL_C(2.0), REAL_C(2.0),
                      REAL_C(2.0)};

    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), peakdetect_width(flat, 5u, 2u,
                                                          REAL_C(0.5)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), peakdetect_width(flat, 5u, 2u,
                                                          REAL_C(-0.1)));
    TEST_ASSERT_EQUAL_REAL(REAL_C(0.0), peakdetect_width(flat, 5u, 2u,
                                                          REAL_C(1.5)));
}

void test_a_peak_that_never_falls_far_enough_is_measured_to_the_end(void)
{
    // The peak stands in the middle and the signal on either side never falls
    // to half of its prominence before the signal runs out. The width is then
    // measured to the ends rather than left as nothing, because the peak is
    // real and its width is at least that much.
    real_t hill[5] = {REAL_C(4.0), REAL_C(4.5), REAL_C(5.0), REAL_C(4.5),
                      REAL_C(4.0)};

    real_t width = peakdetect_width(hill, 5u, 2u, REAL_C(0.5));

    TEST_ASSERT_TRUE(width > REAL_C(0.0));
    TEST_ASSERT_TRUE(width <= REAL_C(4.0));
}

void test_where_there_is_room_for_fewer_peaks_the_ones_that_stand_out_are_kept(void)
{
    // Six peaks and room for three. The three kept must be the three that
    // stand out most, and they must still be listed in the order they stand in
    // the signal and not in the order they were judged.
    real_t signal[25];
    for(uint32_t index = 0; index < 25u; index++)
    {
        signal[index] = REAL_C(0.0);
    }

    // Rising importance, placed out of that order along the signal.
    signal[2] = REAL_C(1.0);
    signal[6] = REAL_C(6.0);
    signal[10] = REAL_C(2.0);
    signal[14] = REAL_C(5.0);
    signal[18] = REAL_C(3.0);
    signal[22] = REAL_C(4.0);

    peakdetect_options_t options = peakdetect_no_rules();
    uint32_t found_at[3];

    uint32_t found = peakdetect_find(signal, 25u, &options, found_at, 3u);

    TEST_ASSERT_EQUAL(3, found);

    // The three tallest stand at 6, 14 and 22.
    TEST_ASSERT_EQUAL(6, found_at[0]);
    TEST_ASSERT_EQUAL(14, found_at[1]);
    TEST_ASSERT_EQUAL(22, found_at[2]);

    // And they are in the order they stand in the signal.
    TEST_ASSERT_TRUE(found_at[0] < found_at[1]);
    TEST_ASSERT_TRUE(found_at[1] < found_at[2]);
}
