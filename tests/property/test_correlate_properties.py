"""Rules that correlating two signals must keep.

Correlation is what delay, matched and every search for a period rest on, thus
what must hold here is held by more of the library than any of those modules
know.
"""

import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402

RUNS = settings(max_examples=40, deadline=None)

SCALINGS = st.sampled_from(ffitt.CORRELATE_SCALINGS)

SIZES = st.integers(min_value=8, max_value=96)


@st.composite
def signal(draw, least=8, most=96, size=None):
    if size is None:
        size = draw(st.integers(min_value=least, max_value=most))

    return draw(st.lists(sp.elements(4.0), min_size=size, max_size=size))


def correlated(lib, first, second, largest_lag, scaling):
    out = ffitt.real_buffer(largest_lag + 1)

    assert lib.correlate_cross(ffitt.float_array(first),
                               ffitt.float_array(second), len(first), out,
                               largest_lag, scaling)

    return [out[index] for index in range(largest_lag + 1)]


def by_hand(first, second, lag):
    """The sum the module says it forms, worked out here so that the two arrive
    by different roads."""
    return sum(first[index] * second[index + lag]
               for index in range(len(first) - lag))


@given(signal(), signal(), SCALINGS)
@RUNS
def test_the_raw_sum_is_the_sum_of_the_products(lib, first, second, scaling):
    """THE RULE THAT DEFINES THE OPERATION. Everything else here is that sum
    divided by something."""
    size = min(len(first), len(second))
    first = first[:size]
    second = second[:size]

    largest_lag = size // 2

    found = correlated(lib, first, second, largest_lag, ffitt.CORRELATE_RAW)

    for lag in range(largest_lag + 1):
        wanted = by_hand(first, second, lag)
        scale = 1.0 + sum(abs(value) for value in first) * max(
            abs(value) for value in second)

        assert abs(found[lag] - wanted) <= 1e-4 * scale


@given(signal())
@RUNS
def test_a_signal_correlated_with_itself_is_largest_at_no_lag(lib, values):
    """THE RULE EVERY SEARCH FOR A PERIOD RESTS ON. Nothing a signal can be
    compared against matches it better than itself, thus the lag of nothing
    must hold the largest value there is. A signal that matched a shift of
    itself better would make correlate_best_lag point at that shift instead."""
    assume(sum(value * value for value in values) > 0.25)

    largest_lag = len(values) // 2

    found = correlated(lib, values, values, largest_lag, ffitt.CORRELATE_RAW)

    for lag in range(1, largest_lag + 1):
        assert found[0] >= found[lag] - 1e-4 * (1.0 + abs(found[0]))


@given(signal())
@RUNS
def test_correlating_a_signal_with_itself_is_the_auto_correlation(lib, values):
    """correlate_auto is correlate_cross given the same signal twice, and the
    two must never part company."""
    largest_lag = len(values) // 2

    out = ffitt.real_buffer(largest_lag + 1)

    assert lib.correlate_auto(ffitt.float_array(values), len(values), out,
                              largest_lag, ffitt.CORRELATE_RAW)

    cross = correlated(lib, values, values, largest_lag, ffitt.CORRELATE_RAW)

    for lag in range(largest_lag + 1):
        assert out[lag] == cross[lag]


@given(signal(), signal(), st.floats(min_value=0.25, max_value=8.0, width=32))
@RUNS
def test_the_coefficient_does_not_move_when_a_signal_is_scaled(lib, first,
                                                               second,
                                                               louder):
    """THE WHOLE POINT OF THE COEFFICIENT. It says how alike two signals are,
    and how loud they are is not part of that. A coefficient that moved when a
    signal was turned up could not be judged against a fixed number, and every
    caller that reads a strength judges it against a fixed number."""
    size = min(len(first), len(second))
    first = first[:size]
    second = second[:size]

    assume(sum(value * value for value in first) > 0.25)
    assume(sum(value * value for value in second) > 0.25)

    # ENERGY IS NOT SHAPE, AND THE COEFFICIENT IS MADE OF SHAPE.
    #
    # A reading that sits at 3.0 and wanders by a hundred-thousandth has all
    # the energy the two lines above ask for and no shape at all: at 32 bits a
    # value near 3 is held to about three ten-millionths, thus the distances
    # from the mean carry barely two digits and the coefficient is worked out
    # from rounding.
    #
    # Such a signal is exactly where the coefficient stops being free of how
    # loud it is, which is what this test measures. The header of the module
    # now records how far it drifts and why nothing can mend it. Here the rule
    # is asked only of signals that really do have a shape: the wander must
    # stand well clear of the last digits of the samples themselves.
    for values in (first, second):
        largest = max(abs(value) for value in values) + 1.0
        middle = sum(values) / len(values)
        wander = max(abs(value - middle) for value in values)
        assume(wander > (1e-3 * largest))

    largest_lag = size // 2

    plain = correlated(lib, first, second, largest_lag,
                       ffitt.CORRELATE_COEFFICIENT)
    scaled = correlated(lib, first,
                        [sp.to_float32(value * louder) for value in second],
                        largest_lag, ffitt.CORRELATE_COEFFICIENT)

    for lag in range(largest_lag + 1):
        assert abs(plain[lag] - scaled[lag]) <= 1e-3


@given(signal())
@RUNS
def test_the_coefficient_never_leaves_the_range_of_one(lib, values):
    """It is a coefficient, thus it stands between -1 and 1 whatever it is
    given. A caller judging a strength against a fixed number needs that to be
    true of every signal and not of most of them."""
    assume(sum(value * value for value in values) > 0.25)

    largest_lag = len(values) // 2

    found = correlated(lib, values, values, largest_lag,
                       ffitt.CORRELATE_COEFFICIENT)

    for value in found:
        assert math.isfinite(value)
        assert -1.0 - 1e-4 <= value <= 1.0 + 1e-4

    # And a signal against itself at no lag is exactly one.
    assert abs(found[0] - 1.0) <= 1e-3


@given(signal(least=16, most=96))
@RUNS
def test_the_biased_and_unbiased_scalings_differ_by_the_overlap(lib, values):
    """Two ways of dividing the same sum. The biased one divides by the size
    however far the two are slid apart; the unbiased one divides by how many
    samples really overlapped. The second is larger at every lag but nothing,
    and by exactly the share of the signal that fell off the end."""
    size = len(values)
    largest_lag = size // 2

    biased = correlated(lib, values, values, largest_lag,
                        ffitt.CORRELATE_BIASED)
    unbiased = correlated(lib, values, values, largest_lag,
                          ffitt.CORRELATE_UNBIASED)

    for lag in range(largest_lag + 1):
        # size samples against size - lag of them.
        expected = biased[lag] * (size / (size - lag))

        assert abs(unbiased[lag] - expected) <= 1e-3 * (1.0 + abs(expected))


@given(signal(least=24, most=96), st.integers(min_value=2, max_value=8))
@RUNS
def test_a_signal_that_repeats_is_found_to_repeat(lib, values, period):
    """THE REASON correlate_best_lag EXISTS, given a signal that really does
    repeat.

    WHAT IS CHECKED IS THAT THE LAG FOUND IS REALLY A PERIOD, and not that it
    is the period the piece was cut to. A piece of two samples that holds the
    same value twice makes a signal whose period is ONE, and a piece of four
    holding two values twice makes one whose period is two. Asking for the
    nominal period back is asking the module to report something that is not
    true of the signal: measured, a piece of 0.7 and 0.7 gave a best lag of 1,
    and 1 is right."""
    assume(period * 3 <= len(values))
    assume(sum(value * value for value in values[:period]) > 0.5)

    piece = values[:period]
    made = [piece[index % period] for index in range(len(values))]

    # A signal that never changes has every lag for a period, thus the question
    # means nothing for it.
    assume(max(made) - min(made) > 0.25)

    repeating = ffitt.float_array(made)

    out = ffitt.real_buffer(len(values))
    strength = ffitt.real_buffer(1)

    found = lib.correlate_best_lag(repeating, len(values), out, 1,
                                   len(values) // 2, strength)

    assert found >= 1

    # The lag found really is a period: sliding the signal along by it leaves
    # every sample where it was.
    for index in range(len(made) - found):
        assert abs(made[index] - made[index + found]) <= 1e-5

    # And a signal that truly repeats agrees with itself almost perfectly.
    assert strength[0] > 0.9


@given(signal(least=24, most=96))
@RUNS
def test_a_signal_that_does_not_repeat_says_so(lib, values):
    """THE NUMBER THAT MUST BE READ. Every signal has a lag where it agrees
    with itself best, thus an answer is always given. The strength is the only
    thing that says whether the answer means anything."""
    assume(sum(value * value for value in values) > 0.25)

    out = ffitt.real_buffer(len(values))
    strength = ffitt.real_buffer(1)

    lib.correlate_best_lag(ffitt.float_array(values), len(values), out, 1,
                           len(values) // 2, strength)

    # Whatever it found, the strength is a coefficient and can be judged.
    assert -1.0 - 1e-4 <= strength[0] <= 1.0 + 1e-4


@given(st.integers(min_value=-4, max_value=8))
def test_only_the_four_scalings_are_taken(lib, scaling):
    assert lib.correlate_is_valid_scaling(scaling) == (0 <= scaling <= 3)


@given(signal(), SCALINGS)
@RUNS
def test_a_lag_as_long_as_the_signal_is_refused(lib, values, scaling):
    """At a lag of the whole size the two signals do not overlap at all, thus
    there is nothing to add up and no answer to give."""
    size = len(values)
    out = ffitt.real_buffer(size + 1)
    given_values = ffitt.float_array(values)

    assert not lib.correlate_cross(given_values, given_values, size, out,
                                   size, scaling)
    assert not lib.correlate_cross(given_values, given_values, 0, out, 0,
                                   scaling)


@given(signal(least=16, most=64))
@RUNS
def test_the_fast_method_agrees_with_the_plain_one(lib, values):
    """The transform is there for speed and must be there for nothing else. A
    caller choosing it for a long signal must get the answer the plain one
    would have given."""
    size = len(values)
    across = lib.correlate_transform_size(size)

    assume(across > 0)

    largest_lag = size // 2

    plain = ffitt.real_buffer(largest_lag + 1)
    fast = ffitt.real_buffer(largest_lag + 1)
    given_values = ffitt.float_array(values)

    assert lib.correlate_auto(given_values, size, plain, largest_lag,
                              ffitt.CORRELATE_RAW)

    fft = lib.fft_alloc(across)
    work = (ffitt.Cnum * across)()
    window = ffitt.real_buffer(across)

    try:
        assert lib.correlate_auto_by_transform(given_values, size, fast,
                                               largest_lag,
                                               ffitt.CORRELATE_RAW, fft, work,
                                               window)
    finally:
        lib.fft_free(fft)

    scale = 1.0 + abs(plain[0])

    for lag in range(largest_lag + 1):
        assert abs(plain[lag] - fast[lag]) <= 1e-3 * scale
