"""Rules that the shapes a peak can have must keep.

The worth of this module is that the shapes are KNOWN, thus what must hold is
that each one really is the shape it claims to be and that the widths of two
of them mean the same thing. The last test here is the reason the module was
written: it measures how far a peak fitter leans, which cannot be measured at
all without a peak whose top is known exactly.
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

SHAPES = st.sampled_from(sptk.CURVE_SHAPES)
EVEN_SHAPES = st.sampled_from(sptk.CURVE_EVEN_SHAPES)

# Values a float of 32 bits holds exactly.
MIDDLES = st.sampled_from([-64.0, -1.5, 0.0, 0.25, 7.0, 1024.0])
WIDTHS = st.sampled_from([0.0625, 0.5, 1.0, 3.0, 32.0])
SKEWS = st.sampled_from([-8.0, -3.0, -0.5, 0.0, 0.5, 2.0, 4.0, 8.0])


def value(lib, shape, at, middle, width, skew=0.0):
    return lib.curve_value(shape, sp.to_float32(at), sp.to_float32(middle),
                           sp.to_float32(width), sp.to_float32(skew))


def room_at(middle, width, base=1e-5):
    """How far two answers may stand apart when the PLACES they were read at
    cannot themselves be written down exactly.

    THIS IS NOT SLACK. A place near 1024 written as a float of 32 bits sits on
    a grid whose steps are about 0.00012 apart, thus asking for a place a
    distance away on one side and the same distance on the other gives two
    places that are NOT the same distance out: each is rounded to its own
    nearest grid point. The shape is even in the arithmetic; the caller cannot
    say where to read it evenly enough to show that at every scale.

    What the answer can move by is how far the place moved multiplied by how
    steeply the shape falls, and these shapes fall by at most about 0.61 of
    their top across one width. Measured, the case that found this: a middle of
    1024 and a width of 1 gave two sides differing by 2.6e-5 where the room was
    1e-5."""
    spacing = abs(middle) * (2.0 ** -24 if not sptk.REAL_64 else 2.0 ** -53)

    return base + (2.0 * spacing / width)


@given(EVEN_SHAPES, MIDDLES, WIDTHS)
@RUNS
def test_every_shape_falls_to_the_same_share_at_one_width(lib, shape, middle,
                                                          width):
    """THE RULE THAT MAKES TWO WIDTHS COMPARABLE. Each shape is written so that
    at one width from the middle it has fallen to the share a normal spread has
    at one standard deviation. Without it a width of 2 would mean one thing for
    a gaussian and another for a lorentzian, and setting the two beside each
    other would say nothing at all."""
    for side in (1.0, -1.0):
        at = middle + (side * width)

        assert abs(value(lib, shape, at, middle, width)
                   - sptk.CURVE_AT_ONE_WIDTH) <= room_at(middle, width, 1e-4)


@given(EVEN_SHAPES, MIDDLES, WIDTHS, sp.elements(8.0))
@RUNS
def test_an_even_shape_is_the_same_on_both_sides(lib, shape, middle, width,
                                                 away):
    """A gaussian and a lorentzian are even about their middle. A shape that
    was not would have a top somewhere other than its middle, and the whole
    module rests on knowing where the top is."""
    above = value(lib, shape, middle + away, middle, width)
    below = value(lib, shape, middle - away, middle, width)

    assert abs(above - below) <= room_at(middle, width)


@given(SHAPES, MIDDLES, WIDTHS, SKEWS, sp.elements(8.0))
@RUNS
def test_no_shape_ever_stands_above_one_or_below_nothing(lib, shape, middle,
                                                         width, skew, away):
    """Every shape stands at one at its top and falls away from there, thus it
    can never leave the range from nothing to one. A caller scales from there,
    and a shape that reached past one would scale into something it did not
    mean."""
    at = middle + (away * width)
    found = value(lib, shape, at, middle, width, skew)

    assert math.isfinite(found)
    assert 0.0 <= found <= 1.0 + 1e-6


@given(SHAPES, MIDDLES, WIDTHS, SKEWS)
@RUNS
def test_every_shape_really_reaches_one_at_its_top(lib, shape, middle, width,
                                                   skew):
    """A shape that never reached one would not be a shape scaled to its top,
    and the shapes could not be set beside each other."""
    top = middle

    if shape == sptk.CURVE_SKEWED_GAUSSIAN:
        top = lib.curve_skewed_gaussian_top(sp.to_float32(middle),
                                            sp.to_float32(width),
                                            sp.to_float32(skew))

    assert abs(value(lib, shape, top, middle, width, skew)
               - 1.0) <= room_at(middle, width, 1e-4)


@given(MIDDLES, WIDTHS, st.sampled_from([2.0, 3.0, 5.0, 10.0]))
@RUNS
def test_the_lorentzian_holds_more_in_its_tails_than_the_gaussian(
        lib, middle, width, away):
    """THE DIFFERENCE THE MODULE EXISTS TO OFFER. Past one width the two part
    company and the lorentzian holds far more. A peak fitter that measures a
    baseline near the peak reads that tail AS baseline."""
    at = middle + (away * width)

    gaussian = value(lib, sptk.CURVE_GAUSSIAN, at, middle, width)
    lorentzian = value(lib, sptk.CURVE_LORENTZIAN, at, middle, width)

    assert lorentzian > gaussian


@given(MIDDLES, WIDTHS, sp.elements(6.0))
@RUNS
def test_a_skew_of_nothing_is_the_plain_gaussian(lib, middle, width, away):
    """It must be exactly the gaussian and not nearly it, or the skew is not a
    parameter that can be turned off."""
    at = middle + (away * width)

    plain = value(lib, sptk.CURVE_GAUSSIAN, at, middle, width)
    skewed = value(lib, sptk.CURVE_SKEWED_GAUSSIAN, at, middle, width, 0.0)

    assert abs(plain - skewed) <= room_at(middle, width, 1e-4)


@given(MIDDLES, WIDTHS, st.sampled_from([0.5, 2.0, 4.0, 8.0]))
@RUNS
def test_turning_the_skew_round_mirrors_the_shape(lib, middle, width, skew):
    """A tail on the high side turned round is a tail on the low side, and
    nothing else about the shape may change."""
    for away in (0.5, 1.5, 3.0):
        one = value(lib, sptk.CURVE_SKEWED_GAUSSIAN, middle + (away * width),
                    middle, width, skew)
        other = value(lib, sptk.CURVE_SKEWED_GAUSSIAN, middle - (away * width),
                      middle, width, -skew)

        assert abs(one - other) <= room_at(middle, width, 1e-4)

    above = lib.curve_skewed_gaussian_top(sp.to_float32(middle),
                                          sp.to_float32(width),
                                          sp.to_float32(skew))
    below = lib.curve_skewed_gaussian_top(sp.to_float32(middle),
                                          sp.to_float32(width),
                                          sp.to_float32(-skew))

    assert abs((above - middle) + (below - middle)) <= 1e-3 * width


@given(MIDDLES, WIDTHS, SKEWS)
@RUNS
def test_the_top_that_is_given_is_really_the_top(lib, middle, width, skew):
    """THE NUMBER A PEAK FITTER IS TRYING TO FIND, thus the number everything
    else here is measured against. Nothing either side of it may stand
    higher."""
    top = lib.curve_skewed_gaussian_top(sp.to_float32(middle),
                                        sp.to_float32(width),
                                        sp.to_float32(skew))
    tallest = value(lib, sptk.CURVE_SKEWED_GAUSSIAN, top, middle, width, skew)

    for step in range(-40, 41):
        at = top + ((step / 10.0) * width)

        assert value(lib, sptk.CURVE_SKEWED_GAUSSIAN, at, middle, width,
                     skew) <= tallest + room_at(middle, width)


@given(MIDDLES, WIDTHS, st.sampled_from([1.0, 2.0, 4.0, 8.0]))
@RUNS
def test_a_skewed_peak_carries_more_on_its_long_side(lib, middle, width, skew):
    """Which is what a tail IS. A shape whose two sides carried the same would
    be even, whatever its skew said."""
    above = 0.0
    below = 0.0

    for step in range(1, 200):
        away = (step / 25.0) * width

        above += value(lib, sptk.CURVE_SKEWED_GAUSSIAN, middle + away, middle,
                       width, skew)
        below += value(lib, sptk.CURVE_SKEWED_GAUSSIAN, middle - away, middle,
                       width, skew)

    assert above > below


@given(SHAPES, MIDDLES, WIDTHS, SKEWS, st.integers(min_value=1,
                                                   max_value=64))
@RUNS
def test_a_block_is_the_places_read_one_at_a_time(lib, shape, middle, width,
                                                  skew, count):
    """The block is there for speed and must be there for nothing else. It also
    looks for the top of a skewed shape once rather than at every place, thus
    it could easily part company with reading them one at a time."""
    written = sptk.real_buffer(count)
    low = middle - (4.0 * width)
    high = middle + (4.0 * width)

    assert lib.curve_block(shape, sp.to_float32(low), sp.to_float32(high),
                           sp.to_float32(middle), sp.to_float32(width),
                           sp.to_float32(skew), written, count)

    between = ((high - low) / (count - 1)) if count > 1 else 0.0

    for index in range(count):
        at = low + (between * index)

        assert abs(written[index]
                   - value(lib, shape, at, middle, width, skew)) <= room_at(
                       middle, width, 1e-4)


@given(st.floats(min_value=-4.0, max_value=4.0, width=32))
def test_only_a_width_above_nothing_is_taken(lib, width):
    """A peak of no width is not a peak, and every shape divides by it."""
    assert lib.curve_is_valid_width(sp.to_float32(width)) == (width > 0.0)


@given(st.integers(min_value=-4, max_value=8))
def test_only_the_three_shapes_are_taken(lib, shape):
    assert lib.curve_is_valid_shape(shape) == (0 <= shape <= 2)


@given(SHAPES, MIDDLES, SKEWS, sp.elements(4.0))
@RUNS
def test_a_width_that_is_refused_gives_nothing(lib, shape, middle, skew, at):
    """Nothing rather than a shape of some width nobody chose."""
    assert value(lib, shape, at, middle, 0.0, skew) == 0.0
    assert value(lib, shape, at, middle, -1.0, skew) == 0.0


@given(MIDDLES, WIDTHS, SKEWS)
@RUNS
def test_a_curve_fitted_through_three_points_leans_by_the_shape(lib, middle,
                                                                width, skew):
    """THE REASON THIS MODULE WAS WRITTEN.

    delay_refine_peak fits a curve of the second order through a peak and its
    two neighbours. That curve is exact for a peak that IS of the second order
    and wrong for every other one, and until now there was no way to say how
    wrong, because there was no peak in this library whose top was known
    exactly.

    Now there is. The refinement is given three samples of a known shape and
    what it says is set against where the top really stands. What must hold is
    not that it is right -- it cannot be -- but that it is CLOSER THAN THE
    NEAREST WHOLE SAMPLE, which is the whole reason to refine at all."""
    assume(width >= 1.0)

    top = lib.curve_skewed_gaussian_top(sp.to_float32(middle),
                                        sp.to_float32(width),
                                        sp.to_float32(skew))

    # Three samples a whole step apart, with the middle one the largest. The
    # step is a fifth of the width, thus the peak is sampled about as coarsely
    # as a real measurement samples one.
    step = width / 5.0
    nearest = round((top - middle) / step)

    places = [middle + ((nearest + offset) * step) for offset in (-1, 0, 1)]
    values = [value(lib, sptk.CURVE_SKEWED_GAUSSIAN, at, middle, width, skew)
              for at in places]

    # The refinement wants the middle of the three to be the largest, which is
    # what taking the nearest sample to the top gives.
    assume(values[1] > values[0] and values[1] > values[2])

    within = lib.delay_refine_peak(sptk.float_array(values), 3, 1)

    refined = places[1] + (within * step)

    assert abs(refined - top) < abs(places[1] - top) + (0.5 * step)
    assert abs(refined - top) <= step
