"""Rules that a polynomial and its roots must keep.

The module caps the order it will find roots for, and the header explains that
the cap is not the method but the width: by order 5 at 32 bits the coefficients
themselves no longer describe the polynomial that was meant. These tests keep
inside that cap and hold what must be true there.
"""

import ctypes
import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

ROOTS = settings(max_examples=50)

# The cap follows the width, thus so does what these tests may ask for.
LARGEST_ORDER = 12 if sptk.REAL_64 else 4


def coefficients(values):
    return sptk.float_array(values)


@st.composite
def polynomial(draw, order=None, magnitude=4.0):
    """Give the coefficients of a polynomial whose highest one is not nothing.

    A highest coefficient of nothing means the polynomial is really of a lower
    order, and the module refuses it for that reason.
    """
    if order is None:
        order = draw(st.integers(min_value=1, max_value=LARGEST_ORDER))

    lower = draw(st.lists(sp.elements(magnitude), min_size=order,
                          max_size=order))
    highest = draw(st.floats(min_value=0.25, max_value=magnitude, width=32))

    if draw(st.booleans()):
        highest = -highest

    return lower + [highest]


@given(polynomial(), sp.elements(3.0))
def test_reading_a_polynomial_agrees_with_adding_its_powers(lib, p, at):
    """The work is done from the highest power inwards, which never forms a
    power on its own. It must still give what the powers added up would."""
    order = len(p) - 1

    plainly = sum(p[power] * (at ** power) for power in range(order + 1))

    scale = 1.0 + sum(abs(value) * (abs(at) ** power)
                      for power, value in enumerate(p))

    assert abs(lib.poly_evaluate(coefficients(p), order, at)
               - plainly) <= 1e-4 * scale


@given(polynomial(), sp.elements(2.0), sp.elements(2.0))
def test_reading_at_a_complex_place_agrees_at_a_real_one(lib, p, at, ignored):
    """A place with no imaginary part must give the same answer as the plain
    form, or the two are not reading the same polynomial."""
    order = len(p) - 1

    real_way = lib.poly_evaluate(coefficients(p), order, at)
    complex_way = lib.poly_evaluate_complex(coefficients(p), order,
                                            lib.cnum_make(at,
                                                          sp.to_float32(0.0)))

    scale = 1.0 + sum(abs(value) * (abs(at) ** power)
                      for power, value in enumerate(p))

    assert abs(complex_way.re - real_way) <= 1e-4 * scale
    assert abs(complex_way.im) <= 1e-4 * scale


@given(polynomial(magnitude=2.0), polynomial(magnitude=2.0), sp.elements(1.5))
def test_a_product_read_at_a_place_is_the_two_read_and_multiplied(lib, first,
                                                                  second, at):
    """THE RULE THAT DEFINES MULTIPLYING. Anything else is a different
    operation wearing the name."""
    first_order = len(first) - 1
    second_order = len(second) - 1
    order = first_order + second_order
    assume(order <= 20)

    product = sptk.real_buffer(order + 1)

    assert lib.poly_multiply(coefficients(first), first_order,
                             coefficients(second), second_order, product,
                             order + 1)

    apart = (lib.poly_evaluate(coefficients(first), first_order, at)
             * lib.poly_evaluate(coefficients(second), second_order, at))

    together = lib.poly_evaluate(product, order, at)

    scale = 1.0 + abs(apart) + sum(abs(product[i]) * (abs(at) ** i)
                                   for i in range(order + 1))

    assert abs(together - apart) <= 1e-3 * scale


@given(polynomial(magnitude=2.0), polynomial(magnitude=2.0))
def test_multiplying_does_not_depend_on_the_order_of_the_two(lib, first,
                                                             second):
    """One polynomial multiplied by another is the same either way round."""
    first_order = len(first) - 1
    second_order = len(second) - 1
    order = first_order + second_order

    one = sptk.real_buffer(order + 1)
    other = sptk.real_buffer(order + 1)

    lib.poly_multiply(coefficients(first), first_order, coefficients(second),
                      second_order, one, order + 1)
    lib.poly_multiply(coefficients(second), second_order, coefficients(first),
                      first_order, other, order + 1)

    scale = 1.0 + max(abs(one[i]) for i in range(order + 1))

    for index in range(order + 1):
        assert abs(one[index] - other[index]) <= 1e-4 * scale


@given(polynomial(), sp.elements(2.0))
def test_the_derivative_is_how_fast_the_polynomial_is_changing(lib, p, at):
    """Measured against the polynomial itself: moving a little way along must
    change the value by about the derivative multiplied by that way."""
    order = len(p) - 1
    slope = sptk.real_buffer(max(order, 1))

    assert lib.poly_derivative(coefficients(p), order, slope)

    step = 1e-3
    before = lib.poly_evaluate(coefficients(p), order, sp.to_float32(at - step))
    after = lib.poly_evaluate(coefficients(p), order, sp.to_float32(at + step))

    measured = (after - before) / (2.0 * step)
    given_slope = lib.poly_evaluate(slope, order - 1, at) if order >= 1 else 0.0

    scale = 1.0 + abs(given_slope) + sum(abs(v) * (abs(at) ** i)
                                         for i, v in enumerate(p))

    assert abs(measured - given_slope) <= 1e-2 * scale


@given(polynomial())
@ROOTS
def test_every_root_that_comes_back_is_really_a_root(lib, p):
    """THE RULE THAT SAYS THE ANSWER IS AN ANSWER. A root is a place where the
    polynomial is nothing, and nothing else about it matters."""
    order = len(p) - 1
    roots = (sptk.Cnum * (order + 1))()

    assume(lib.poly_roots(coefficients(p), order, roots))

    # How large the polynomial gets near its roots, which is what the value at
    # a root must be measured against.
    scale = 1.0 + sum(abs(value) for value in p)

    for index in range(order):
        value = lib.poly_evaluate_complex(coefficients(p), order, roots[index])

        assert math.hypot(value.re, value.im) <= 1e-3 * scale


@given(polynomial())
@ROOTS
def test_a_root_off_the_real_line_brings_its_mirror_with_it(lib, p):
    """A polynomial whose coefficients are all real cannot have a root off the
    real line on its own: the two come as a pair, one the mirror of the other.
    A module that gave one without the other would be describing a polynomial
    with coefficients that are not real."""
    order = len(p) - 1
    roots = (sptk.Cnum * (order + 1))()

    assume(lib.poly_roots(coefficients(p), order, roots))

    for index in range(order):
        if abs(roots[index].im) <= 1e-6:
            continue

        mirrored = False

        for other in range(order):
            if other == index:
                continue
            if (abs(roots[other].re - roots[index].re) <= 1e-3
                    and abs(roots[other].im + roots[index].im) <= 1e-3):
                mirrored = True

        assert mirrored


@given(st.lists(st.floats(min_value=-0.875, max_value=0.875, width=32),
                min_size=1, max_size=LARGEST_ORDER))
@ROOTS
def test_a_filter_built_from_poles_inside_the_circle_is_stable(lib, places):
    """THE REASON THE MODULE EXISTS, built the other way round: a polynomial
    made by multiplying out roots that all lie inside the circle must be
    judged stable."""
    order = len(places)

    built = [1.0]
    degree = 0

    for place in places:
        factor = sptk.float_array([sp.to_float32(-place), sp.to_float32(1.0)])
        out = sptk.real_buffer(degree + 2)

        lib.poly_multiply(sptk.float_array(built), degree, factor, 1, out,
                          degree + 2)

        degree += 1
        built = [out[index] for index in range(degree + 1)]

    assert lib.poly_is_inside_circle(sptk.float_array(built), order)


@given(st.lists(st.floats(min_value=-0.875, max_value=0.875, width=32),
                min_size=1, max_size=LARGEST_ORDER - 1),
       st.floats(min_value=1.0625, max_value=3.0, width=32))
@ROOTS
def test_one_pole_outside_the_circle_is_enough_to_run_away(lib, inside,
                                                           outside):
    """A filter is stable only if EVERY pole is inside. One outside is a filter
    whose answer doubles every few samples."""
    order = len(inside) + 1

    built = [1.0]
    degree = 0

    for place in list(inside) + [outside]:
        factor = sptk.float_array([sp.to_float32(-place), sp.to_float32(1.0)])
        out = sptk.real_buffer(degree + 2)

        lib.poly_multiply(sptk.float_array(built), degree, factor, 1, out,
                          degree + 2)

        degree += 1
        built = [out[index] for index in range(degree + 1)]

    assert not lib.poly_is_inside_circle(sptk.float_array(built), order)


@given(st.integers(min_value=0, max_value=20))
def test_only_the_orders_the_module_can_hold_are_taken(lib, order):
    """The cap follows the width, because by then the coefficients no longer
    describe the polynomial that was meant."""
    expected = 1 <= order <= (12 if sptk.REAL_64 else 4)

    assert lib.poly_is_valid_order(order) == expected


@given(polynomial())
def test_a_highest_coefficient_of_nothing_is_refused(lib, p):
    """It means the polynomial is really of a lower order, and answering as
    though it were not would give a root at infinity."""
    order = len(p)
    lowered = list(p) + [0.0]
    roots = (sptk.Cnum * (order + 2))()

    assume(lib.poly_is_valid_order(order))

    assert not lib.poly_roots(coefficients(lowered), order, roots)
    assert not lib.poly_is_inside_circle(coefficients(lowered), order)
