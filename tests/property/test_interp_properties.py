"""Rules that the reading of a table must keep, for every table.

The pchip curve is in the library because it NEVER LEAVES THE RANGE OF THE
TABLE and never turns back on data that only rises. Those two claims were
measured on one table when the module was written. These tests make the claim
for every table that Hypothesis can find.
"""

import os
import sys

from hypothesis import assume, given
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402


def slopes_for(lib, x, y):
    """Give the slopes that pchip needs for this table."""
    size = len(x)
    slopes = ffitt.real_buffer(size)
    assert lib.interp_pchip_slopes(ffitt.float_array(x), ffitt.float_array(y),
                                   size, slopes)
    return slopes


def places_across(x, count=40):
    """Give places spread across the whole of a table."""
    first, last = x[0], x[-1]
    if last <= first:
        return [first]
    step = (last - first) / float(count - 1)
    return [sp.to_float32(first + (step * index)) for index in range(count)]


@given(sp.rising_points())
def test_pchip_never_leaves_the_range_of_the_table(lib, points):
    """THE CLAIM THE MODULE IS BUILT ON.

    A smooth curve through a table can swing outside it between the points.
    The cspline module does exactly that, by 22 parts in 100 on the table the
    guide measures. This one must not, on any table whatever.
    """
    x, y = points
    slopes = slopes_for(lib, x, y)
    lowest, highest = min(y), max(y)

    for place in places_across(x):
        value = lib.interp_pchip(ffitt.float_array(x), ffitt.float_array(y),
                                 slopes, len(x), place)
        # A little room for the rounding of a float, and no more.
        room = 1e-4 + (1e-4 * max(abs(lowest), abs(highest)))
        assert lowest - room <= value <= highest + room


@given(sp.rising_points())
def test_pchip_never_turns_back_on_a_table_that_only_rises(lib, points):
    """A table that only rises must give a curve that only rises.

    This is the property that a calibration depends on. A curve that dips
    between two points of a rising table reports a temperature falling while
    the reading climbs.
    """
    x, y = points
    # Make the values rise, keeping whatever x the strategy gave.
    y = sorted(y)
    assume(y[0] < y[-1])

    slopes = slopes_for(lib, x, y)
    places = places_across(x, 60)
    values = [lib.interp_pchip(ffitt.float_array(x), ffitt.float_array(y),
                               slopes, len(x), place) for place in places]

    for first, second in zip(values, values[1:]):
        assert second >= first - 1e-4


@given(sp.rising_points())
def test_every_reading_passes_through_the_points_of_the_table(lib, points):
    """Both kinds must give back the table itself at the places of the table.

    A curve that does not pass through its own points is not reading the table;
    it is inventing one.
    """
    x, y = points
    slopes = slopes_for(lib, x, y)

    for index, place in enumerate(x):
        straight = lib.interp_linear(ffitt.float_array(x), ffitt.float_array(y),
                                     len(x), place)
        smooth = lib.interp_pchip(ffitt.float_array(x), ffitt.float_array(y),
                                  slopes, len(x), place)
        assert sp.close(straight, y[index])
        assert sp.close(smooth, y[index])


@given(sp.rising_points(),
       st.floats(min_value=1.0, max_value=50.0, width=32))
def test_outside_the_table_the_answer_is_held_flat(lib, points, distance):
    """Past either end the answer must stop, and not carry on.

    A straight line carried on past the end of a calibration says what the
    device would read at a temperature it was never calibrated at. The module
    says nothing instead, and this holds it to that.
    """
    x, y = points
    slopes = slopes_for(lib, x, y)

    below = sp.to_float32(x[0] - distance)
    above = sp.to_float32(x[-1] + distance)

    for kind, call in (("linear", lambda place: lib.interp_linear(
                            ffitt.float_array(x), ffitt.float_array(y), len(x),
                            place)),
                       ("pchip", lambda place: lib.interp_pchip(
                            ffitt.float_array(x), ffitt.float_array(y), slopes,
                            len(x), place))):
        assert sp.close(call(below), y[0]), kind
        assert sp.close(call(above), y[-1]), kind


@given(sp.rising_points())
def test_a_straight_line_read_by_straight_lines_is_that_line(lib, points):
    """Where the table lies on a line, reading it must give that line back."""
    x, _ = points
    slope = 2.5
    offset = -1.25
    y = [sp.to_float32(offset + (slope * value)) for value in x]

    for place in places_across(x):
        value = lib.interp_linear(ffitt.float_array(x), ffitt.float_array(y),
                                  len(x), place)
        assert sp.close(value, sp.to_float32(offset + (slope * place)),
                        relative=1e-3, absolute=1e-3)


@given(sp.rising_points())
def test_a_table_that_never_moves_is_read_as_never_moving(lib, points):
    """Every value the same must give that value everywhere, by either kind.

    A curve fitted through equal points can still wander if its slopes are
    worked out carelessly.
    """
    x, _ = points
    level = 3.75
    y = [level] * len(x)
    slopes = slopes_for(lib, x, y)

    for place in places_across(x):
        assert sp.close(lib.interp_linear(ffitt.float_array(x),
                                          ffitt.float_array(y), len(x), place),
                        level)
        assert sp.close(lib.interp_pchip(ffitt.float_array(x),
                                         ffitt.float_array(y), slopes, len(x),
                                         place), level)


@given(sp.rising_points())
def test_reading_a_block_agrees_with_reading_one_place_at_a_time(lib, points):
    """The block form is a convenience and must not be a second answer."""
    x, y = points
    slopes = slopes_for(lib, x, y)
    places = places_across(x)
    count = len(places)
    answers = ffitt.real_buffer(count)

    for kind, slope_argument in ((ffitt.INTERP_LINEAR, None),
                                 (ffitt.INTERP_PCHIP, slopes)):
        assert lib.interp_block(ffitt.float_array(x), ffitt.float_array(y),
                                slope_argument, len(x), kind,
                                ffitt.float_array(places), answers, count)

        for index, place in enumerate(places):
            if kind == ffitt.INTERP_LINEAR:
                one = lib.interp_linear(ffitt.float_array(x),
                                        ffitt.float_array(y), len(x), place)
            else:
                one = lib.interp_pchip(ffitt.float_array(x),
                                       ffitt.float_array(y), slopes, len(x),
                                       place)
            assert sp.close(answers[index], one)


@given(sp.rising_points())
def test_a_table_whose_places_do_not_rise_is_refused(lib, points):
    """A table must be in order, and the module must say so rather than read it."""
    x, y = points
    assume(len(x) >= 3)

    # Put two places out of order.
    broken = list(x)
    broken[1], broken[0] = broken[0], broken[1]
    assume(broken[0] != broken[1])

    assert not lib.interp_is_valid_table(ffitt.float_array(broken), len(broken))
    assert lib.interp_is_valid_table(ffitt.float_array(x), len(x))

    slopes = ffitt.real_buffer(len(x))
    assert not lib.interp_pchip_slopes(ffitt.float_array(broken),
                                       ffitt.float_array(y), len(broken),
                                       slopes)
