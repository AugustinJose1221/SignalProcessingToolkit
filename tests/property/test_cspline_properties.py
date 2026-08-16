"""Properties of the cubic spline module."""

import ctypes
import math
import os
import sys

from hypothesis import given
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

REFERENCE = ctypes.byref


def make_spline(lib, x, y):
    """Give a spline and its memory pool for the given points."""
    size = len(x)
    spline = lib.cspline_alloc(size)
    mempool = lib.cspline_alloc_mempool(size)
    lib.cspline_init(REFERENCE(spline), mempool,
                     sptk.float_array(x), sptk.float_array(y))
    return spline, mempool


def free_spline(lib, spline, mempool):
    lib.cspline_free(spline)
    lib.cspline_free_mempool(mempool)


@given(points=sp.rising_points())
def test_the_spline_gives_the_knot_value_at_a_knot(lib, points):
    x, y = points
    spline, mempool = make_spline(lib, x, y)

    for knot_x, knot_y in zip(x, y):
        value = lib.cspline_get_interpolated_point(REFERENCE(spline), knot_x)
        assert sp.close(value, knot_y, relative=1e-3, absolute=1e-3)

    free_spline(lib, spline, mempool)


@given(points=sp.rising_points())
def test_the_spline_has_no_step_at_a_knot(lib, points):
    # A spline is continuous. The value on the left of a knot and the value on
    # the right of a knot must be almost the same.
    x, y = points
    spline, mempool = make_spline(lib, x, y)

    # The largest slope between two knots. A spline can rise faster than this
    # between the knots, but not by a large factor. Thus the difference across
    # a small step must stay near the slope times the step. A spline that takes
    # the wrong interval gives a difference of the size of the y values, and
    # that difference does not fall when the step falls.
    slope = max(abs(y[index + 1] - y[index]) / (x[index + 1] - x[index])
                for index in range(len(x) - 1))

    for index in range(1, len(x) - 1):
        interval = min(x[index] - x[index - 1], x[index + 1] - x[index])
        step = interval / 1000.0
        left = lib.cspline_get_interpolated_point(REFERENCE(spline), x[index] - step)
        right = lib.cspline_get_interpolated_point(REFERENCE(spline), x[index] + step)
        assert abs(left - right) <= 0.01 + (50.0 * slope * step)

    free_spline(lib, spline, mempool)


@given(points=sp.rising_points())
def test_the_spline_gives_a_value_that_a_float_can_hold(lib, points):
    x, y = points
    spline, mempool = make_spline(lib, x, y)

    steps = 20
    for step in range(steps + 1):
        position = x[0] + ((x[-1] - x[0]) * step / steps)
        value = lib.cspline_get_interpolated_point(REFERENCE(spline), position)
        assert not math.isnan(value)
        assert not math.isinf(value)

    free_spline(lib, spline, mempool)


@given(size=st.integers(min_value=3, max_value=8),
       start=st.floats(min_value=-10.0, max_value=10.0, width=32),
       step=st.floats(min_value=0.5, max_value=3.0, width=32),
       slope=st.floats(min_value=-5.0, max_value=5.0, width=32),
       offset=st.floats(min_value=-5.0, max_value=5.0, width=32))
def test_the_spline_of_a_straight_line_is_that_line(lib, size, start, step,
                                                    slope, offset):
    # The knots lie on a straight line. A spline through them must give that
    # same line at every point between the knots.
    x = [start + (index * step) for index in range(size)]
    y = [(slope * value) + offset for value in x]

    spline, mempool = make_spline(lib, x, y)

    steps = 20
    for index in range(steps + 1):
        position = x[0] + ((x[-1] - x[0]) * index / steps)
        expected = (slope * position) + offset
        value = lib.cspline_get_interpolated_point(REFERENCE(spline), position)
        assert sp.close(value, expected, relative=1e-2, absolute=1e-2)

    free_spline(lib, spline, mempool)


@given(size=st.integers(min_value=3, max_value=8),
       start=st.floats(min_value=-10.0, max_value=10.0, width=32),
       step=st.floats(min_value=0.5, max_value=3.0, width=32),
       level=st.floats(min_value=-50.0, max_value=50.0, width=32))
def test_the_spline_of_equal_values_stays_at_that_value(lib, size, start, step,
                                                        level):
    x = [start + (index * step) for index in range(size)]
    y = [level] * size

    spline, mempool = make_spline(lib, x, y)

    steps = 20
    for index in range(steps + 1):
        position = x[0] + ((x[-1] - x[0]) * index / steps)
        value = lib.cspline_get_interpolated_point(REFERENCE(spline), position)
        assert sp.close(value, level, relative=1e-3, absolute=1e-3)

    free_spline(lib, spline, mempool)
