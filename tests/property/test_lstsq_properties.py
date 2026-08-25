"""Rules that a least squares fit must keep, for every set of readings.

The defining rule is the one this file leans on: the error that is left after
the fit holds NOTHING IN COMMON with any column of the model. If it did, some
of it could be taken out by moving a coefficient, and the fit would not be the
one that leaves the least error.
"""

import os
import sys

from hypothesis import assume, given
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

# The order is kept low and the places are kept near zero on purpose. The
# header of the module records what happens otherwise, and a test of the rules
# of a fit should not be a test of the conditioning as well.
ORDERS = st.integers(min_value=1, max_value=3)


@st.composite
def readings(draw, min_size=6, max_size=24):
    """Give readings whose places rise and lie in a range a fit can hold."""
    size = draw(st.integers(min_value=min_size, max_value=max_size))
    steps = draw(st.lists(st.floats(min_value=0.0625, max_value=0.5, width=32),
                          min_size=size - 1, max_size=size - 1))
    start = draw(st.floats(min_value=-2.0, max_value=2.0, width=32))

    x = [start]
    for step in steps:
        x.append(sp.to_float32(x[-1] + step))

    y = draw(st.lists(sp.elements(20.0), min_size=size, max_size=size))
    return x, y


def fit(lib, x, y, order):
    """Fit and give the coefficients, or None where the module refuses."""
    coefficients = sptk.real_buffer(order + 1)
    if not lib.lstsq_polyfit(sptk.float_array(x), sptk.float_array(y), len(x),
                             order, coefficients):
        return None
    return coefficients


@given(readings(), ORDERS)
def test_the_error_that_is_left_holds_nothing_of_the_model(lib, points, order):
    """THE RULE THAT DEFINES A LEAST SQUARES FIT.

    For every power of x in the model, the errors weighed by that power must
    add to nothing. Any other answer could be improved, thus it would not be
    the least.
    """
    x, y = points
    assume(len(x) > order + 1)
    coefficients = fit(lib, x, y, order)
    assume(coefficients is not None)

    errors = [y[index] - lib.lstsq_evaluate(coefficients, order, x[index])
              for index in range(len(x))]

    scale = 1.0 + sum(abs(value) for value in y)

    for power in range(order + 1):
        total = sum(error * (place ** power)
                    for error, place in zip(errors, x))
        room = 1e-3 * scale * max(1.0, max(abs(place) for place in x) ** power)
        assert abs(total) <= room


@given(readings(), ORDERS)
def test_a_fit_is_never_worse_than_the_flat_line_through_the_readings(lib,
                                                                     points,
                                                                     order):
    """A fit must leave no more error than the mean of the readings does.

    The flat line is what a fit of order 0 would give, and it is always
    available to the fit as a special case. An answer worse than it is a
    failure of the arithmetic and not a feature of the data.
    """
    x, y = points
    assume(len(x) > order + 1)
    coefficients = fit(lib, x, y, order)
    assume(coefficients is not None)

    mean = sum(y) / len(y)
    flat = sum((value - mean) ** 2 for value in y)
    fitted = sum((y[index] - lib.lstsq_evaluate(coefficients, order,
                                                x[index])) ** 2
                 for index in range(len(x)))

    assert fitted <= flat + (1e-3 * (1.0 + flat))


@given(readings(), ORDERS)
def test_the_quality_of_a_fit_lies_between_nothing_and_everything(lib, points,
                                                                  order):
    """It reports a part of the movement, thus it cannot leave 0 to 1."""
    x, y = points
    assume(len(x) > order + 1)
    coefficients = fit(lib, x, y, order)
    assume(coefficients is not None)

    quality = lib.lstsq_fit_quality(sptk.float_array(x), sptk.float_array(y),
                                    len(x), coefficients, order)

    assert -1e-4 <= quality <= 1.0 + 1e-4


def quality_scaled(lib, x, y, order):
    """Fit with the scaling and give the quality, or None where it refuses."""
    coefficients = sptk.real_buffer(order + 1)
    centre = sptk.real_buffer(1)
    width = sptk.real_buffer(1)
    if not lib.lstsq_polyfit_scaled(sptk.float_array(x), sptk.float_array(y),
                                    len(x), order, coefficients, centre,
                                    width):
        return None
    return lib.lstsq_fit_quality_scaled(sptk.float_array(x),
                                        sptk.float_array(y), len(x),
                                        coefficients, order, centre[0],
                                        width[0])


@given(readings(), ORDERS)
def test_a_higher_order_never_follows_the_readings_less_well(lib, points,
                                                             order):
    """Every curve of one order is also a curve of the next.

    Thus the fit of the higher order can always do whatever the lower one did,
    and its quality can never be the worse of the two.

    THIS IS ASKED OF THE SCALED FIT AND NOT THE PLAIN ONE, and the header of
    the module says why at length: the plain fit loses its digits when the
    normal equations are formed, and the guard on the factor cannot see that
    loss. The next test pins how far the plain fit really goes.
    """
    x, y = points
    assume(len(x) > order + 2)

    lower = quality_scaled(lib, x, y, order)
    higher = quality_scaled(lib, x, y, order + 1)
    assume(lower is not None and higher is not None)

    assert higher >= lower - 1e-3


def test_the_plain_fit_refuses_where_it_would_have_answered_wrongly(lib):
    """THE FAULT THAT THE PROPERTY TESTS FOUND, AND ITS FIX, BOTH PINNED.

    A curve of order 4 can always do whatever a curve of order 3 did, thus the
    quality must never fall as the order rises. On these readings at 32 bits it
    fell, from 0.769 to 0.527, and lstsq_polyfit gave the answer back without
    complaint.

    The module now does the same fit with the places brought near zero and
    compares. Where the plain fit leaves more error, it is refused. At 64 bits
    there are digits to spare and the plain fit is right, thus nothing is
    refused there.
    """
    x = [2.0, 2.5, 3.0, 3.440809488296509, 3.867011547088623,
         4.205442905426025, 4.460936546325684, 4.656173229217529,
         4.8380656242370605, 5.082167625427246, 5.221558094024658,
         5.471558094024658, 5.549683094024658, 6.049683094024658]
    y = [0.0] * 13 + [1.0]

    # The scaled fit is right at either width, and that is the point.
    assert quality_scaled(lib, x, y, 4) > 0.9

    cubic = fit(lib, x, y, 3)
    assert cubic is not None

    quartic = fit(lib, x, y, 4)

    if sptk.REAL_64:
        assert quartic is not None
        plain_cubic = lib.lstsq_fit_quality(sptk.float_array(x),
                                            sptk.float_array(y), len(x),
                                            cubic, 3)
        plain_quartic = lib.lstsq_fit_quality(sptk.float_array(x),
                                              sptk.float_array(y), len(x),
                                              quartic, 4)
        assert plain_quartic >= plain_cubic
    else:
        assert quartic is None


@given(readings(), ORDERS)
def test_a_plain_fit_that_answers_is_never_worse_than_the_scaled_one(lib,
                                                                     points,
                                                                     order):
    """THE RULE THE MODULE NOW KEEPS.

    Wherever lstsq_polyfit gives an answer at all, that answer must leave no
    more error than the same fit done with the places brought near zero. That
    is what the module promises, and it is the whole of what the check inside
    it does.
    """
    x, y = points
    assume(len(x) > order + 1)

    coefficients = fit(lib, x, y, order)
    assume(coefficients is not None)

    plain = lib.lstsq_fit_quality(sptk.float_array(x), sptk.float_array(y),
                                  len(x), coefficients, order)
    scaled = quality_scaled(lib, x, y, order)
    assume(scaled is not None)

    assert plain >= scaled - 0.02


@given(readings(), ORDERS)
def test_readings_that_lie_on_a_curve_give_that_curve_back(lib, points,
                                                            order):
    """Where the readings really do lie on a curve, the fit must find it.

    THIS IS ASKED OF THE SCALED FIT, and the reason is the whole subject of the
    header of the module. A plain fit through places that sit away from zero
    loses digits in forming the normal equations, thus it can follow the
    readings to a quality above 0.999 while the curve between them is still out
    by parts in a hundred. Measured: 6 readings whose x runs from 1.5 to 2.8,
    at order 3, reach 0.999 with the curve out by 0.02.

    The scaled fit does not lose those digits, and recovers the curve.
    """
    x, _ = points
    assume(len(x) > order + 1)

    true = [1.5, -0.75, 0.5, -0.25][:order + 1]
    y = [sp.to_float32(sum(true[power] * (place ** power)
                           for power in range(order + 1))) for place in x]

    coefficients = sptk.real_buffer(order + 1)
    centre = sptk.real_buffer(1)
    width = sptk.real_buffer(1)

    assume(lib.lstsq_polyfit_scaled(sptk.float_array(x), sptk.float_array(y),
                                    len(x), order, coefficients, centre,
                                    width))

    spread = 1.0 + max(abs(value) for value in y)

    for place in x:
        wanted = sum(true[power] * (place ** power)
                     for power in range(order + 1))
        got = lib.lstsq_evaluate_scaled(coefficients, order, centre[0],
                                        width[0], place)
        assert abs(got - wanted) <= 1e-2 * spread


def curve_at(lib, coefficients, order, x):
    """Give the value of a fit at each place.

    THE CURVE IS COMPARED AND NOT THE COEFFICIENTS, and the difference matters.
    Where the fit is poorly conditioned, two rather different sets of
    coefficients describe the same curve to the last digit that can be seen. A
    test that compares the coefficients is then measuring the conditioning
    rather than the answer, and it fails now and then for no fault of the
    library.
    """
    return [lib.lstsq_evaluate(coefficients, order, place) for place in x]


@given(readings(), ORDERS,
       st.floats(min_value=-30.0, max_value=30.0, width=32))
def test_moving_every_reading_moves_the_whole_curve_by_that_much(lib, points,
                                                                 order, shift):
    """Adding the same amount to every reading lifts the curve by that amount.

    Nothing about the shape changes.
    """
    x, y = points
    assume(len(x) > order + 1)

    first = fit(lib, x, y, order)
    lifted = fit(lib, x, [sp.to_float32(value + shift) for value in y], order)
    assume(first is not None and lifted is not None)

    scale = 1.0 + abs(shift) + max(abs(value) for value in y)

    for before, after in zip(curve_at(lib, first, order, x),
                             curve_at(lib, lifted, order, x)):
        assert abs((before + shift) - after) <= 1e-2 * scale


@given(readings(), ORDERS,
       st.floats(min_value=0.125, max_value=8.0, width=32))
def test_scaling_every_reading_scales_the_whole_curve(lib, points, order,
                                                      factor):
    """Twice the readings must give twice the curve, at every place."""
    x, y = points
    assume(len(x) > order + 1)

    first = fit(lib, x, y, order)
    scaled = fit(lib, x, [sp.to_float32(value * factor) for value in y], order)
    assume(first is not None and scaled is not None)

    scale = 1.0 + (factor * max(abs(value) for value in y))

    for before, after in zip(curve_at(lib, first, order, x),
                             curve_at(lib, scaled, order, x)):
        assert abs((before * factor) - after) <= 1e-2 * scale


@given(readings(), ORDERS)
def test_a_plain_fit_that_answers_leaves_no_more_error_than_the_scaled_one(
        lib, points, order):
    """EXACTLY WHAT THE MODULE PROMISES, AND NOT MORE.

    It does not promise that the two fits give the same curve at every place;
    poor conditioning can move a curve about between the readings without
    moving the error it leaves. What it promises is that where lstsq_polyfit
    answers at all, the answer leaves no more error than the same fit done with
    the places brought near zero, within LSTSQ_LARGEST_EXCESS.
    """
    x, y = points
    assume(len(x) > order + 1)

    plain = fit(lib, x, y, order)
    assume(plain is not None)

    coefficients = sptk.real_buffer(order + 1)
    centre = sptk.real_buffer(1)
    width = sptk.real_buffer(1)
    assume(lib.lstsq_polyfit_scaled(sptk.float_array(x), sptk.float_array(y),
                                    len(x), order, coefficients, centre,
                                    width))

    plain_error = sum(
        (y[index] - lib.lstsq_evaluate(plain, order, x[index])) ** 2
        for index in range(len(x)))
    scaled_error = sum(
        (y[index] - lib.lstsq_evaluate_scaled(coefficients, order, centre[0],
                                              width[0], x[index])) ** 2
        for index in range(len(x)))

    scale = 1.0 + sum(value * value for value in y)

    assert plain_error <= (scaled_error * 1.05) + (1e-3 * scale)


@given(st.integers(min_value=1, max_value=40),
       st.integers(min_value=0, max_value=6))
def test_a_fit_needs_at_least_as_many_readings_as_numbers_to_find(lib, size,
                                                                  order):
    """Fewer readings than coefficients leaves the answer undecided."""
    if size < order + 1:
        assert not lib.lstsq_is_valid_fit(size, order)
