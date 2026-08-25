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


def test_the_plain_fit_can_answer_worse_at_a_higher_order(lib):
    """THE FAULT THE HEADER RECORDS, PINNED SO THAT IT CANNOT BE FORGOTTEN.

    A curve of order 4 can always do whatever a curve of order 3 did, thus the
    quality must never fall as the order rises. On these readings, at 32 bits,
    IT FALLS, and lstsq_polyfit gives the answer back without complaint. The
    guard on the diagonal of the factor sees a ratio of 0.64, which is a
    thousand times above where it sits, because the digits were spent before
    the factor was ever taken.

    The scaled fit gets it right on the same data at the same width. This test
    holds both halves of that, so that a change to either is a decision.
    """
    x = [2.0, 2.5, 3.0, 3.440809488296509, 3.867011547088623,
         4.205442905426025, 4.460936546325684, 4.656173229217529,
         4.8380656242370605, 5.082167625427246, 5.221558094024658,
         5.471558094024658, 5.549683094024658, 6.049683094024658]
    y = [0.0] * 13 + [1.0]

    cubic = fit(lib, x, y, 3)
    quartic = fit(lib, x, y, 4)
    assert cubic is not None and quartic is not None

    plain_cubic = lib.lstsq_fit_quality(sptk.float_array(x),
                                        sptk.float_array(y), len(x), cubic, 3)
    plain_quartic = lib.lstsq_fit_quality(sptk.float_array(x),
                                          sptk.float_array(y), len(x),
                                          quartic, 4)

    scaled_quartic = quality_scaled(lib, x, y, 4)

    # The scaled fit is right at either width.
    assert scaled_quartic > 0.9

    if sptk.REAL_64:
        # With digits to spare the plain fit is right as well.
        assert plain_quartic >= plain_cubic
    else:
        # At 32 bits the plain fit falls away, and this is the fault recorded
        # in the header of the module.
        assert plain_quartic < plain_cubic
        assert plain_quartic < scaled_quartic - 0.3


@given(readings(), ORDERS)
def test_readings_that_lie_on_a_curve_give_that_curve_back(lib, points, order):
    """Where the readings really do lie on a curve, the fit must find it."""
    x, _ = points
    assume(len(x) > order + 1)

    true = [1.5, -0.75, 0.5, -0.25][:order + 1]
    y = [sp.to_float32(sum(true[power] * (place ** power)
                           for power in range(order + 1))) for place in x]

    coefficients = fit(lib, x, y, order)
    assume(coefficients is not None)

    quality = lib.lstsq_fit_quality(sptk.float_array(x), sptk.float_array(y),
                                    len(x), coefficients, order)
    assume(quality > 0.99)

    for place in x:
        wanted = sum(true[power] * (place ** power)
                     for power in range(order + 1))
        assert sp.close(lib.lstsq_evaluate(coefficients, order, place), wanted,
                        relative=1e-2, absolute=1e-2)


@given(readings(), ORDERS,
       st.floats(min_value=-30.0, max_value=30.0, width=32))
def test_moving_every_reading_moves_only_the_constant(lib, points, order,
                                                      shift):
    """Adding the same amount to every reading lifts the curve by that amount.

    Nothing about the shape changes, thus every coefficient but the first must
    stay where it was.
    """
    x, y = points
    assume(len(x) > order + 1)

    first = fit(lib, x, y, order)
    lifted = fit(lib, x, [sp.to_float32(value + shift) for value in y], order)
    assume(first is not None and lifted is not None)

    scale = 1.0 + abs(shift) + max(abs(value) for value in y)

    assert abs((first[0] + shift) - lifted[0]) <= 1e-2 * scale

    for power in range(1, order + 1):
        assert abs(first[power] - lifted[power]) <= 1e-2 * scale


@given(readings(), ORDERS,
       st.floats(min_value=0.125, max_value=8.0, width=32))
def test_scaling_every_reading_scales_the_whole_curve(lib, points, order,
                                                      factor):
    """Twice the readings must give twice the curve, coefficient by coefficient."""
    x, y = points
    assume(len(x) > order + 1)

    first = fit(lib, x, y, order)
    scaled = fit(lib, x, [sp.to_float32(value * factor) for value in y], order)
    assume(first is not None and scaled is not None)

    scale = 1.0 + (factor * max(abs(value) for value in y))

    for power in range(order + 1):
        assert abs((first[power] * factor) - scaled[power]) <= 1e-2 * scale


@given(readings(), ORDERS)
def test_the_scaled_fit_reads_the_same_curve_as_the_plain_one(lib, points,
                                                              order):
    """Scaling changes where x sits and nothing about the answer.

    Read back with the centre and the width it gave, the scaled fit must follow
    the readings exactly as the plain fit does.
    """
    x, y = points
    assume(len(x) > order + 1)

    plain = fit(lib, x, y, order)
    assume(plain is not None)

    coefficients = sptk.real_buffer(order + 1)
    centre = sptk.real_buffer(1)
    width = sptk.real_buffer(1)

    assert lib.lstsq_polyfit_scaled(sptk.float_array(x), sptk.float_array(y),
                                    len(x), order, coefficients, centre, width)

    scale = 1.0 + max(abs(value) for value in y)

    for place in x:
        by_plain = lib.lstsq_evaluate(plain, order, place)
        by_scaled = lib.lstsq_evaluate_scaled(coefficients, order, centre[0],
                                              width[0], place)
        assert abs(by_plain - by_scaled) <= 1e-2 * scale


@given(st.integers(min_value=1, max_value=40),
       st.integers(min_value=0, max_value=6))
def test_a_fit_needs_at_least_as_many_readings_as_numbers_to_find(lib, size,
                                                                  order):
    """Fewer readings than coefficients leaves the answer undecided."""
    if size < order + 1:
        assert not lib.lstsq_is_valid_fit(size, order)
