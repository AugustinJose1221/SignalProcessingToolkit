"""Rules that a Savitzky-Golay filter must keep.

What parts this from every other smoother is one property, and it is not a
matter of degree: A POLYNOMIAL OF THE ORDER IT WAS DESIGNED FOR PASSES THROUGH
EXACTLY UNCHANGED.

A moving average rounds a peak because a peak is not flat. This lays a
polynomial through the window instead, thus a signal that already IS such a
polynomial has nothing taken off it. That is the whole reason to reach for it
where the shape of a peak matters, and it is what this file is built around.
"""

import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

RUNS = settings(max_examples=40, deadline=None)

# The window must be odd and hold more samples than the order needs.
WINDOWS = st.sampled_from([5, 7, 9, 11, 15, 21])
ORDERS = st.integers(min_value=0, max_value=4)


def designed(lib, window, order, derivative=0):
    savgol = lib.savgol_alloc(window)

    assert lib.savgol_design(savgol, order, derivative)

    return savgol


def through(lib, savgol, values):
    out = sptk.real_buffer(len(values))

    lib.savgol_process_block(savgol, sptk.float_array(values), out,
                             len(values))

    return [out[index] for index in range(len(values))]


def polynomial(size, coefficients, middle=None):
    """A polynomial read at every whole place, centred so that the numbers stay
    small enough for a float of 32 bits to hold them exactly."""
    if middle is None:
        middle = size / 2.0

    values = []

    for index in range(size):
        at = index - middle
        total = 0.0

        for power, weight in enumerate(coefficients):
            total += weight * (at ** power)

        values.append(sp.to_float32(total))

    return values


@given(WINDOWS, ORDERS,
       st.lists(st.floats(min_value=-2.0, max_value=2.0, width=32),
                min_size=5, max_size=5))
@RUNS
def test_a_polynomial_of_that_order_passes_through_unchanged(lib, window,
                                                             order, weights):
    """THE PROPERTY THAT DEFINES THE FILTER.

    The filter lays a polynomial of its order through the window and reads the
    middle of it. A signal that already is such a polynomial is therefore its
    own answer: there is nothing for the fit to change. A moving average would
    round it, and that rounding is exactly what this exists not to do.

    Held at every order the filter takes, on polynomials of that order and
    below."""
    assume(window > order + 1)

    weights = weights[:order + 1]

    savgol = designed(lib, window, order)

    try:
        size = window * 4
        values = polynomial(size, weights)
        got = through(lib, savgol, values)

        # The filter needs a whole window, thus the ends are read from fewer
        # samples than the middle and the module says so.
        half = window // 2
        scale = 1.0 + max(abs(value) for value in values)

        for index in range(half, size - half):
            assert abs(got[index] - values[index]) <= 1e-3 * scale
    finally:
        lib.savgol_free(savgol)


@given(WINDOWS, st.integers(min_value=1, max_value=4),
       st.floats(min_value=-2.0, max_value=2.0, width=32),
       st.floats(min_value=-2.0, max_value=2.0, width=32))
@RUNS
def test_the_first_derivative_of_a_straight_line_is_its_slope(lib, window,
                                                              order, level,
                                                              slope):
    """THE OTHER THING IT DOES, AND THE REASON IT IS WORTH HAVING FOR IT.

    Taking a difference between neighbouring samples gives a derivative and
    multiplies the noise by two. This fits a polynomial and reads the slope of
    the fit, thus it gives a derivative that has been smoothed rather than one
    that has been made worse.

    A straight line has a slope that is the same everywhere, thus the answer
    must be that slope at every sample."""
    assume(window > order + 1)
    assume(abs(slope) > 0.05)

    savgol = designed(lib, window, order, 1)

    try:
        size = window * 4
        values = polynomial(size, [level, slope])
        got = through(lib, savgol, values)

        half = window // 2

        for index in range(half, size - half):
            assert abs(got[index] - slope) <= 1e-2 * (1.0 + abs(slope))
    finally:
        lib.savgol_free(savgol)


@given(WINDOWS, st.integers(min_value=2, max_value=4),
       st.floats(min_value=-1.0, max_value=1.0, width=32))
@RUNS
def test_the_second_derivative_of_a_square_law_is_twice_its_curve(lib, window,
                                                                  order,
                                                                  curve):
    """The second derivative of a curve of the second order is twice its
    coefficient, everywhere. A filter that gave anything else would not be
    giving a derivative at all."""
    assume(window > order + 1)
    assume(abs(curve) > 0.05)

    savgol = designed(lib, window, order, 2)

    try:
        size = window * 4
        values = polynomial(size, [0.0, 0.0, curve])
        got = through(lib, savgol, values)

        half = window // 2
        wanted = 2.0 * curve

        for index in range(half, size - half):
            assert abs(got[index] - wanted) <= 1e-2 * (1.0 + abs(wanted))
    finally:
        lib.savgol_free(savgol)


@given(WINDOWS, ORDERS, sp.elements(8.0))
@RUNS
def test_the_coefficients_of_a_smoother_add_up_to_one(lib, window, order,
                                                      level):
    """A CONSEQUENCE OF THE FIRST RULE, AND THE EASIEST WAY TO SEE IT BROKEN. A
    level is a polynomial of no order at all, thus it must pass through
    unchanged whatever the order is, thus the coefficients must add up to
    exactly one. Anything else and every smoothed signal would come out at the
    wrong level."""
    assume(window > order + 1)

    savgol = designed(lib, window, order)

    try:
        total = sum(lib.savgol_get_coefficient(savgol, index)
                    for index in range(window))

        assert abs(total - 1.0) <= 1e-4

        # And the level really does come through.
        window_values = sptk.float_array([level] * window)

        assert abs(lib.savgol_apply(savgol, window_values)
                   - level) <= 1e-3 * (1.0 + abs(level))
    finally:
        lib.savgol_free(savgol)


@given(WINDOWS, st.integers(min_value=1, max_value=4))
@RUNS
def test_the_coefficients_of_a_derivative_add_up_to_nothing(lib, window,
                                                            order):
    """The derivative of a level is nothing, thus the coefficients of a
    derivative filter must add up to nothing. A set that did not would report a
    slope where a reading is standing still."""
    assume(window > order + 1)

    for derivative in (1, 2):
        if order < derivative:
            continue

        savgol = designed(lib, window, order, derivative)

        try:
            total = sum(lib.savgol_get_coefficient(savgol, index)
                        for index in range(window))

            assert abs(total) <= 1e-4
        finally:
            lib.savgol_free(savgol)


@given(WINDOWS, ORDERS, st.floats(min_value=0.25, max_value=8.0, width=32))
@RUNS
def test_it_is_linear_in_the_signal(lib, window, order, louder):
    """It is a set of fixed weights, thus twice the signal gives twice the
    answer and two signals added give the two answers added."""
    assume(window > order + 1)

    savgol = designed(lib, window, order)

    try:
        size = window * 3
        state = 3
        values = []

        for _ in range(size):
            state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
            values.append(sp.to_float32((((state >> 16) % 20000) / 10000.0)
                                        - 1.0))

        louder_values = [sp.to_float32(value * louder) for value in values]

        plain = through(lib, savgol, values)
        scaled = through(lib, savgol, louder_values)

        for one, other in zip(plain, scaled):
            assert abs(other - (one * louder)) <= 1e-3 * (1.0 + abs(other))
    finally:
        lib.savgol_free(savgol)


@given(WINDOWS, ORDERS)
@RUNS
def test_a_smoother_of_order_nothing_is_a_moving_average(lib, window, order):
    """A polynomial of no order at all is a level, thus fitting one is taking
    the mean. The two must therefore agree exactly, and a caller who wanted a
    mean has not been given something else by accident."""
    assume(window > 1)

    savgol = designed(lib, window, 0)

    try:
        for index in range(window):
            assert abs(lib.savgol_get_coefficient(savgol, index)
                       - (1.0 / window)) <= 1e-5
    finally:
        lib.savgol_free(savgol)


@given(st.integers(min_value=0, max_value=32),
       st.integers(min_value=0, max_value=8),
       st.integers(min_value=0, max_value=4))
def test_only_a_window_that_can_hold_the_fit_is_taken(lib, window, order,
                                                      derivative):
    """A polynomial of order n needs n+1 points to be fixed at all, and the
    window must be odd so that it has a middle to read. A derivative above the
    order is nothing everywhere and is not worth a filter."""
    # THE RULE IS THE MODULE'S AND NOT A GUESS AT IT. A window of one with an
    # order of nothing is taken: it fits a level through a single point and
    # gives that point back, which is a filter that does nothing and is not
    # wrong. Asking for three here was asking for a bound the module does not
    # have.
    fits = ((window % 2) == 1
            and window > order
            and derivative <= order)

    assert lib.savgol_is_valid(window, order, derivative) == fits
