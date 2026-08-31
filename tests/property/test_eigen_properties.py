"""Rules that the directions a matrix stretches must keep.

The unit tests build matrices by turning a known set of values with a fixed
pattern of rotations. These build them with whatever pattern Hypothesis finds,
which reaches the cases a fixed pattern never visits: values that stand very
close together, values far apart, and matrices that squash a direction flat.
"""

import ctypes
import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402

ORDERS = st.integers(min_value=1, max_value=6)
SOLVE = settings(max_examples=40)


def make_matrix(lib, rows):
    """Give a matrix holding these rows."""
    order = len(rows)
    matrix = lib.matrix_create_zero_matrix(order, len(rows[0]))

    for row in range(order):
        for column in range(len(rows[0])):
            lib.matrix_add_element(ctypes.byref(matrix), row, column,
                                   sp.to_float32(rows[row][column]))

    return matrix


@st.composite
def symmetric_rows(draw, order=None):
    """Give the rows of a symmetric matrix.

    Built by writing each element and its partner across the diagonal from the
    same draw, thus it is symmetric exactly and not nearly.
    """
    if order is None:
        order = draw(ORDERS)

    rows = [[0.0] * order for _ in range(order)]

    for row in range(order):
        for column in range(row, order):
            value = draw(sp.elements(10.0))
            rows[row][column] = value
            rows[column][row] = value

    return rows


def solve(lib, rows, want_vectors=True):
    """Solve and give the values and the vectors, or None where it refuses."""
    order = len(rows)
    matrix = make_matrix(lib, rows)
    vectors = lib.matrix_create_zero_matrix(order, order) if want_vectors \
        else None
    values = ffitt.real_buffer(order)

    handle = ctypes.byref(vectors) if want_vectors else None
    answered = lib.eigen_solve(ctypes.byref(matrix), values, handle)

    lib.matrix_free(ctypes.byref(matrix))

    if not answered:
        if want_vectors:
            lib.matrix_free(ctypes.byref(vectors))
        return None

    return values, vectors


@given(symmetric_rows())
@SOLVE
def test_a_symmetric_matrix_is_always_solved(lib, rows):
    """Every symmetric matrix has real values and directions at right angles.

    That is what makes this case worth having a module for, thus the module
    must never refuse one.
    """
    matrix = make_matrix(lib, rows)

    assert lib.eigen_is_valid_matrix(ctypes.byref(matrix))

    lib.matrix_free(ctypes.byref(matrix))

    assert solve(lib, rows) is not None


@given(symmetric_rows())
@SOLVE
def test_the_matrix_really_stretches_each_direction_by_its_value(lib, rows):
    """THE RULE THAT SAYS THE ANSWER IS AN ANSWER.

    A value and a direction mean nothing apart. Together they claim that the
    matrix multiplied by the direction is the direction multiplied by the
    value, and this holds them to it.
    """
    order = len(rows)
    answered = solve(lib, rows)
    assume(answered is not None)

    values, vectors = answered

    largest = max(abs(values[index]) for index in range(order))
    room = 1e-4 * (largest + 1.0) * order

    for which in range(order):
        for row in range(order):
            stretched = sum(
                rows[row][column]
                * lib.matrix_get_element(ctypes.byref(vectors), column, which)
                for column in range(order))

            scaled = values[which] * lib.matrix_get_element(
                ctypes.byref(vectors), row, which)

            assert abs(stretched - scaled) <= room

    lib.matrix_free(ctypes.byref(vectors))


@given(symmetric_rows())
@SOLVE
def test_every_direction_is_of_unit_length_and_at_right_angles(lib, rows):
    """A direction with itself is 1, and with any other is nothing."""
    order = len(rows)
    answered = solve(lib, rows)
    assume(answered is not None)

    values, vectors = answered

    for first in range(order):
        for second in range(order):
            together = sum(
                lib.matrix_get_element(ctypes.byref(vectors), row, first)
                * lib.matrix_get_element(ctypes.byref(vectors), row, second)
                for row in range(order))

            wanted = 1.0 if first == second else 0.0

            assert abs(together - wanted) <= 1e-3

    lib.matrix_free(ctypes.byref(vectors))


@given(symmetric_rows())
@SOLVE
def test_the_values_add_up_to_the_diagonal(lib, rows):
    """What a matrix stretches by, added up, is what stands on its diagonal.

    That holds however the matrix is turned, thus it is a check that costs
    nothing and catches a great deal.
    """
    order = len(rows)
    answered = solve(lib, rows, False)
    assume(answered is not None)

    values, _ = answered

    trace = sum(rows[index][index] for index in range(order))
    total = sum(values[index] for index in range(order))

    scale = 1.0 + sum(abs(rows[i][j]) for i in range(order)
                      for j in range(order))

    assert abs(total - trace) <= 1e-4 * scale


@given(symmetric_rows())
@SOLVE
def test_the_values_come_back_largest_first(lib, rows):
    """A caller reading the first of them expects the one that matters most,
    and eigen_part_held counts from the front."""
    order = len(rows)
    answered = solve(lib, rows, False)
    assume(answered is not None)

    values, _ = answered

    for index in range(order - 1):
        assert values[index] >= values[index + 1]


@given(symmetric_rows(), st.floats(min_value=0.125, max_value=8.0, width=32))
@SOLVE
def test_scaling_the_matrix_scales_every_value(lib, rows, factor):
    """A matrix multiplied by a number stretches by that much more in every
    direction, and turns in exactly the same ones."""
    order = len(rows)
    scaled_rows = [[sp.to_float32(value * factor) for value in row]
                   for row in rows]

    plain = solve(lib, rows, False)
    scaled = solve(lib, scaled_rows, False)
    assume(plain is not None and scaled is not None)

    largest = max(abs(plain[0][index]) for index in range(order))
    room = 1e-3 * (largest * factor + 1.0)

    for index in range(order):
        assert abs((plain[0][index] * factor) - scaled[0][index]) <= room


@given(symmetric_rows(order=3))
@SOLVE
def test_a_matrix_that_squashes_a_direction_flat_cannot_be_undone(lib, rows):
    """A direction stretched by nothing is a direction that cannot be brought
    back, and eigen_condition says so rather than giving a number."""
    # Two rows the same makes a matrix that squashes the direction between
    # them to nothing.
    rows = [list(row) for row in rows]
    rows[2] = list(rows[1])
    for column in range(3):
        rows[column][2] = rows[column][1]

    # AND THE MATRIX IS SCALED SO THAT ITS LARGEST ELEMENT IS ONE. That is not
    # slack: the rank of a matrix does not change when the matrix is scaled,
    # thus scaling takes nothing away from what is examined here, and it takes
    # away a fault that belongs to the width rather than to the module.
    #
    # eigen_solve decides how far to turn at each step from the SQUARES of the
    # elements. A matrix whose elements are smaller than the square root of the
    # smallest ordinary number the width holds has squares that are no longer
    # ordinary numbers, and it is then turned by the wrong amount. Measured at
    # 32 bits on this very matrix, the rank came back as 2 at every scale from
    # 1e-1 down to 1e-17 and as 3 from 1e-19 down. 1e-19 is the square root of
    # the smallest ordinary number a float of 32 bits holds. The header of
    # eigen_solve says so, and says to scale.
    largest = max(abs(value) for row in rows for value in row)
    assume(largest > 0.0)

    rows = [[sp.to_float32(value / largest) for value in row] for row in rows]

    answered = solve(lib, rows, False)
    assume(answered is not None)

    values, _ = answered

    # One of the three stands at nothing, thus the matrix stretches only two
    # directions and cannot be undone.
    assert lib.eigen_rank(values, 3, sp.to_float32(1e-4)) <= 2


@given(st.lists(st.floats(min_value=0.5, max_value=100.0, width=32),
                min_size=1, max_size=6))
def test_eigen_condition_is_the_largest_over_the_smallest(lib, sizes):
    """Taken by size, because how far from nothing a value stands is what
    matters here and not which way it leans."""
    values = ffitt.float_array(sizes)

    wanted = max(sizes) / min(sizes)

    assert sp.close(lib.eigen_condition(values, len(sizes)), wanted,
                    relative=1e-4, absolute=1e-4)

    # A value that leans the other way counts by how far from nothing it is.
    leaning = ffitt.float_array([-value for value in sizes])

    assert sp.close(lib.eigen_condition(leaning, len(sizes)), wanted,
                    relative=1e-4, absolute=1e-4)


@given(st.lists(st.floats(min_value=0.5, max_value=100.0, width=32),
                min_size=1, max_size=6),
       st.integers(min_value=0, max_value=8))
def test_eigen_part_held_climbs_to_one_and_never_past_it(lib, sizes, first):
    """It reports a part of the whole spread, thus it cannot leave 0 to 1, and
    taking more directions can never hold less."""
    values = ffitt.float_array(sizes)
    count = len(sizes)

    held = lib.eigen_part_held(values, count, first)

    assert -1e-5 <= held <= 1.0 + 1e-5

    if first > 0:
        assert held >= lib.eigen_part_held(values, count, first - 1) - 1e-5

    # All of them is all of it.
    assert sp.close(lib.eigen_part_held(values, count, count), 1.0,
                    relative=1e-4, absolute=1e-4)


@given(symmetric_rows())
@SOLVE
def test_the_matrix_may_be_solved_without_asking_for_the_directions(lib, rows):
    """The values must not depend on whether the directions were wanted."""
    with_them = solve(lib, rows)
    without = solve(lib, rows, False)
    assume(with_them is not None and without is not None)

    order = len(rows)

    for index in range(order):
        assert sp.close(with_them[0][index], without[0][index],
                        relative=1e-4, absolute=1e-4)

    lib.matrix_free(ctypes.byref(with_them[1]))


@given(symmetric_rows(), st.integers(min_value=1, max_value=4))
@SOLVE
def test_eigen_refuses_a_matrix_that_is_not_symmetric(lib, rows, nudge):
    """A matrix that is not symmetric can have values that are complex, thus
    the module says no rather than answering a question that was not asked."""
    order = len(rows)
    assume(order >= 2)

    rows = [list(row) for row in rows]
    rows[0][1] += float(nudge) * 10.0

    matrix = make_matrix(lib, rows)

    assert not lib.eigen_is_valid_matrix(ctypes.byref(matrix))

    values = ffitt.real_buffer(order)

    assert not lib.eigen_solve(ctypes.byref(matrix), values, None)

    lib.matrix_free(ctypes.byref(matrix))
