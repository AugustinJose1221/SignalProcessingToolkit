"""Strategies and comparison helpers for the property based tests."""

import ctypes
import os
import sys

from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402


def to_float32(value):
    """Give the value that a variable of the library really holds.

    Python calculates with 64 bits. The library holds every value in real_t,
    which is 32 bits by default and 64 bits when the library is built that way.
    A test that compares a Python result with a library result must first bring
    the Python value to the width that the library holds, or the comparison
    measures the difference between the two languages and not the library.
    """
    return sptk.REAL_T(value).value

# The library holds every value in a float. A float keeps about 7 digits. Very
# large values and very small values lose all the digits after the point, and
# then a comparison has no meaning. Thus the strategies keep the values in a
# range where the arithmetic still gives a result that a user can trust.
MAGNITUDE = 100.0


def elements(magnitude=MAGNITUDE):
    """Give float values that a float variable can hold exactly enough."""
    return st.floats(min_value=-magnitude, max_value=magnitude,
                     allow_nan=False, allow_infinity=False, width=32)


@st.composite
def matrix_rows(draw, min_side=1, max_side=4, rows=None, columns=None,
                magnitude=MAGNITUDE):
    """Give the elements of a matrix as a list of lists."""
    if rows is None:
        rows = draw(st.integers(min_value=min_side, max_value=max_side))
    if columns is None:
        columns = draw(st.integers(min_value=min_side, max_value=max_side))
    return draw(st.lists(
        st.lists(elements(magnitude), min_size=columns, max_size=columns),
        min_size=rows, max_size=rows))


@st.composite
def square_matrix_rows(draw, min_side=1, max_side=4, magnitude=MAGNITUDE):
    side = draw(st.integers(min_value=min_side, max_value=max_side))
    return draw(matrix_rows(rows=side, columns=side, magnitude=magnitude))


@st.composite
def matrix_pair_of_the_same_order(draw, min_side=1, max_side=4):
    """Give two matrices that have the same number of rows and columns."""
    rows = draw(st.integers(min_value=min_side, max_value=max_side))
    columns = draw(st.integers(min_value=min_side, max_value=max_side))
    first = draw(matrix_rows(rows=rows, columns=columns))
    second = draw(matrix_rows(rows=rows, columns=columns))
    return first, second


@st.composite
def square_matrix_pair(draw, min_side=1, max_side=4):
    """Give two square matrices of the same order."""
    side = draw(st.integers(min_value=min_side, max_value=max_side))
    first = draw(matrix_rows(rows=side, columns=side))
    second = draw(matrix_rows(rows=side, columns=side))
    return first, second


@st.composite
def multipliable_matrix_pair(draw, min_side=1, max_side=4):
    """Give two matrices where the first one can multiply the second one."""
    rows = draw(st.integers(min_value=min_side, max_value=max_side))
    shared = draw(st.integers(min_value=min_side, max_value=max_side))
    columns = draw(st.integers(min_value=min_side, max_value=max_side))
    first = draw(matrix_rows(rows=rows, columns=shared))
    second = draw(matrix_rows(rows=shared, columns=columns))
    return first, second


@st.composite
def dominant_square_matrix_rows(draw, min_side=1, max_side=4):
    """Give a square matrix that has an inverse.

    Each element on the diagonal is larger than the sum of the other elements
    of its row. Such a matrix is never singular, and the elimination gives a
    result that the float type can hold with confidence.
    """
    side = draw(st.integers(min_value=min_side, max_value=max_side))
    rows = draw(st.lists(
        st.lists(elements(1.0), min_size=side, max_size=side),
        min_size=side, max_size=side))
    for index in range(side):
        others = sum(abs(value) for value in rows[index]) - abs(rows[index][index])
        extra = draw(st.floats(min_value=1.0, max_value=5.0, width=32))
        sign = 1.0 if rows[index][index] >= 0 else -1.0
        rows[index][index] = sign * (others + extra)
    return rows


@st.composite
def rising_points(draw, min_size=3, max_size=8):
    """Give points where the x values rise. A spline needs such points."""
    size = draw(st.integers(min_value=min_size, max_value=max_size))
    steps = draw(st.lists(st.floats(min_value=0.25, max_value=4.0, width=32),
                          min_size=size - 1, max_size=size - 1))
    start = draw(st.floats(min_value=-10.0, max_value=10.0, width=32))

    x = [start]
    for step in steps:
        x.append(to_float32(x[-1] + step))

    y = draw(st.lists(elements(10.0), min_size=size, max_size=size))
    return x, y


def determinant_bound(rows):
    """Give the largest value that the determinant of these rows can reach.

    This is the bound of Hadamard, with the sum of each row in the place of the
    length of each row. The calculation of a determinant adds and subtracts
    products of this size. Thus the error of a float calculation grows with
    this bound, and not with the size of the result. A matrix where two rows
    are almost the same has a small determinant and a large bound.
    """
    product = 1.0
    for row in rows:
        product *= sum(abs(value) for value in row)
    return product


def close(first, second, relative=1e-4, absolute=1e-4):
    """Give True if two float values agree inside the tolerance."""
    return abs(first - second) <= absolute + (relative * max(abs(first), abs(second)))


def rows_close(first, second, relative=1e-4, absolute=1e-4):
    if len(first) != len(second):
        return False
    for row_a, row_b in zip(first, second):
        if len(row_a) != len(row_b):
            return False
        for value_a, value_b in zip(row_a, row_b):
            if not close(value_a, value_b, relative, absolute):
                return False
    return True
