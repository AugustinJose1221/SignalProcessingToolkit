"""Rules that an attitude must keep, for every attitude.

quaternion_from_matrix reads whichever of four forms is largest, thus which
line of arithmetic runs depends on the attitude given to it. A handful of
examples exercises one or two of those four. These tests reach all of them.
"""

import ctypes
import math
import os
import sys

from hypothesis import assume, given
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

PARTS = st.floats(min_value=-4.0, max_value=4.0, width=32)
ANGLES = st.floats(min_value=-6.25, max_value=6.25, width=32)


@st.composite
def attitudes(draw):
    """Give an attitude, made from an axis and an angle so that it is real.

    Drawing four numbers and normalising them would work too, but an axis and
    an angle is how an attitude arrives in a device, and it never gives the
    zero quaternion that has no direction at all.
    """
    x = draw(PARTS)
    y = draw(PARTS)
    z = draw(PARTS)
    assume((x * x) + (y * y) + (z * z) > 0.01)
    angle = draw(ANGLES)
    return (x, y, z, angle)


def made(lib, axis_and_angle):
    x, y, z, angle = axis_and_angle
    return lib.quaternion_from_axis_angle(x, y, z, angle)


def parts_of(q):
    return (q.w, q.x, q.y, q.z)


@given(attitudes())
def test_an_attitude_from_an_axis_and_an_angle_has_a_magnitude_of_one(lib,
                                                                      axis):
    """An attitude carries a direction and not a size."""
    assert sp.close(lib.quaternion_magnitude(made(lib, axis)), 1.0,
                    relative=1e-4, absolute=1e-4)


@given(attitudes(), attitudes())
def test_two_attitudes_multiplied_keep_a_magnitude_of_one(lib, first, second):
    """Turning and then turning again is still only a turning.

    A product whose magnitude drifts from 1 is a product that stretches the
    thing it turns, and after a few thousand steps of a filter that stretch is
    the whole answer.
    """
    product = lib.quaternion_multiply(made(lib, first), made(lib, second))

    assert sp.close(lib.quaternion_magnitude(product), 1.0, relative=1e-3,
                    absolute=1e-3)


@given(attitudes())
def test_an_attitude_undone_by_its_conjugate_is_no_turn_at_all(lib, axis):
    """The conjugate is the turn the other way, thus the two cancel."""
    q = made(lib, axis)
    both = lib.quaternion_multiply(q, lib.quaternion_conjugate(q))

    assert lib.quaternion_is_same_attitude(both, lib.quaternion_identity(),
                                           1e-3)


@given(attitudes(), PARTS, PARTS, PARTS)
def test_turning_a_vector_does_not_change_its_length(lib, axis, x, y, z):
    """A turn moves a thing and does not stretch it."""
    q = made(lib, axis)
    out_x = sptk.real_buffer(1)
    out_y = sptk.real_buffer(1)
    out_z = sptk.real_buffer(1)

    lib.quaternion_rotate(q, x, y, z, out_x, out_y, out_z)

    before = math.sqrt((x * x) + (y * y) + (z * z))
    after = math.sqrt((out_x[0] ** 2) + (out_y[0] ** 2) + (out_z[0] ** 2))

    assert sp.close(before, after, relative=1e-3, absolute=1e-3)


@given(attitudes(), PARTS, PARTS, PARTS)
def test_turning_a_vector_and_turning_it_back_gives_the_vector(lib, axis, x, y,
                                                               z):
    """The conjugate must undo the turn on a vector as well as on an attitude."""
    q = made(lib, axis)
    mid = [sptk.real_buffer(1) for _ in range(3)]
    back = [sptk.real_buffer(1) for _ in range(3)]

    lib.quaternion_rotate(q, x, y, z, mid[0], mid[1], mid[2])
    lib.quaternion_rotate(lib.quaternion_conjugate(q), mid[0][0], mid[1][0],
                          mid[2][0], back[0], back[1], back[2])

    scale = 1.0 + math.sqrt((x * x) + (y * y) + (z * z))

    for got, wanted in zip(back, (x, y, z)):
        assert abs(got[0] - wanted) <= 1e-3 * scale


@given(attitudes())
def test_an_attitude_and_its_negative_are_the_same_attitude(lib, axis):
    """Four numbers hold every attitude twice, and a great deal of trouble
    comes from comparing them value by value."""
    q = made(lib, axis)
    negative = lib.quaternion_make(-q.w, -q.x, -q.y, -q.z)

    assert lib.quaternion_is_same_attitude(q, negative, 1e-4)


@given(attitudes(), PARTS, PARTS, PARTS)
def test_an_attitude_and_its_negative_turn_a_vector_the_same_way(lib, axis, x,
                                                                 y, z):
    """The two are the same attitude, thus they must act the same."""
    q = made(lib, axis)
    negative = lib.quaternion_make(-q.w, -q.x, -q.y, -q.z)

    first = [sptk.real_buffer(1) for _ in range(3)]
    second = [sptk.real_buffer(1) for _ in range(3)]

    lib.quaternion_rotate(q, x, y, z, first[0], first[1], first[2])
    lib.quaternion_rotate(negative, x, y, z, second[0], second[1], second[2])

    scale = 1.0 + math.sqrt((x * x) + (y * y) + (z * z))

    for got, wanted in zip(second, first):
        assert abs(got[0] - wanted[0]) <= 1e-3 * scale


@given(attitudes())
def test_an_attitude_written_as_a_matrix_and_read_back_is_unchanged(lib, axis):
    """THE ONE THAT REACHES ALL FOUR LINES OF ARITHMETIC.

    quaternion_from_matrix reads whichever of four forms is largest, and which
    one that is depends on where the attitude points. A test with a handful of
    attitudes runs one or two of the four; this one runs all of them.
    """
    q = made(lib, axis)
    matrix = lib.matrix_create_zero_matrix(3, 3)

    lib.quaternion_to_matrix_into(q, ctypes.byref(matrix))
    back = lib.quaternion_from_matrix(ctypes.byref(matrix))

    assert lib.quaternion_is_same_attitude(q, back, 1e-2)

    lib.matrix_free(ctypes.byref(matrix))


@given(attitudes(), attitudes(), attitudes())
def test_turning_three_times_does_not_depend_on_the_grouping(lib, first,
                                                             second, third):
    """Multiplication of attitudes is associative, though it is not commutative.

    The order of the turns matters; where the brackets fall does not.
    """
    a, b, c = made(lib, first), made(lib, second), made(lib, third)

    left = lib.quaternion_multiply(lib.quaternion_multiply(a, b), c)
    right = lib.quaternion_multiply(a, lib.quaternion_multiply(b, c))

    assert lib.quaternion_is_same_attitude(left, right, 1e-2)


@given(attitudes(), attitudes())
def test_a_turn_between_two_attitudes_ends_where_it_was_told_to(lib, first,
                                                                second):
    """At no part of the way the answer is the first, and at all of it the
    second."""
    a, b = made(lib, first), made(lib, second)

    assert lib.quaternion_is_same_attitude(lib.quaternion_slerp(a, b, 0.0), a,
                                           1e-3)
    assert lib.quaternion_is_same_attitude(lib.quaternion_slerp(a, b, 1.0), b,
                                           1e-3)


@given(attitudes(), attitudes(),
       st.floats(min_value=0.0, max_value=1.0, width=32))
def test_a_turn_between_two_attitudes_stays_an_attitude_all_the_way(lib, first,
                                                                    second,
                                                                    part):
    """Adding two attitudes and dividing gives something that is not a turn.

    slerp exists so that every step of the way is still an attitude, and this
    holds that at every part of the way.
    """
    between = lib.quaternion_slerp(made(lib, first), made(lib, second), part)

    assert sp.close(lib.quaternion_magnitude(between), 1.0, relative=1e-3,
                    absolute=1e-3)


@given(PARTS, PARTS, PARTS)
def test_an_angle_of_nothing_is_no_turn_at_all(lib, x, y, z):
    """Whatever axis is named, turning by nothing about it changes nothing."""
    assume((x * x) + (y * y) + (z * z) > 0.01)

    q = lib.quaternion_from_axis_angle(x, y, z, 0.0)

    assert lib.quaternion_is_same_attitude(q, lib.quaternion_identity(), 1e-4)


@given(attitudes())
def test_normalising_an_attitude_that_is_already_one_changes_nothing(lib,
                                                                     axis):
    """A magnitude of 1 is already what normalising asks for."""
    q = made(lib, axis)
    again = lib.quaternion_normalise(q)

    for got, wanted in zip(parts_of(again), parts_of(q)):
        assert abs(got - wanted) <= 1e-3
