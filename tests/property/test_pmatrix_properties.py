"""Properties of the matrix whose elements are functions of a parameter.

The module holds a pointer to a function at each place rather than a value.
Give it a value for the parameter and it gives back a matrix of numbers that
every other module of the library can take.

Thus the thing to test is not that a pointer can be stored and read back. It is
that the matrix which comes out is the RIGHT matrix, and that it keeps working
as a matrix when it is used as one. The tests below build a matrix of rotation
from sines and cosines and hold it to the laws a rotation obeys:

    turning by one angle and then by another is the same as turning by the two
    added together, a rotation undoes itself when it is transposed, and it
    changes no length.

None of that can be true by accident. A matrix built the wrong way round, or
one whose elements were read from the wrong place, fails all three.
"""

import ctypes
import math
import os
import sys

from hypothesis import assume, given
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402

REFERENCE = ctypes.byref

angles = st.floats(min_value=-6.0, max_value=6.0, allow_nan=False,
                   allow_infinity=False, width=32)


# The elements. These are kept at the top level and not built inside a test,
# because a pointer to a function that Python has let go of is a pointer into
# nothing.
@ffitt.PMATRIX_FUNCTION
def sine(x):
    return math.sin(x)


@ffitt.PMATRIX_FUNCTION
def cosine(x):
    return math.cos(x)


@ffitt.PMATRIX_FUNCTION
def minus_sine(x):
    return -math.sin(x)


@ffitt.PMATRIX_FUNCTION
def twice(x):
    return 2.0 * x


def rotation(lib):
    """Give the matrix of a turn about the origin, as a parameter matrix."""
    matrix = lib.pmatrix_alloc(2, 2)
    lib.pmatrix_add_element(REFERENCE(matrix), 0, 0, cosine)
    lib.pmatrix_add_element(REFERENCE(matrix), 0, 1, minus_sine)
    lib.pmatrix_add_element(REFERENCE(matrix), 1, 0, sine)
    lib.pmatrix_add_element(REFERENCE(matrix), 1, 1, cosine)
    return matrix


def values(lib, matrix):
    return ffitt.matrix_rows(lib, matrix)


def near_rows(first, second, room=1e-4):
    return all(abs(a - b) <= room
               for row_a, row_b in zip(first, second)
               for a, b in zip(row_a, row_b))


@given(first=angles, second=angles)
def test_turning_twice_is_the_same_as_turning_by_the_two_added_together(
        lib, first, second):
    """The law that says the matrix really is a rotation.

    R(a) times R(b) must be R(a+b). This is true of the mathematics and it is
    true of nothing else: a matrix with its sines in the wrong corners, or one
    whose rows and columns were exchanged, gives an answer for every angle and
    the right answer for none.
    """
    turn = rotation(lib)
    try:
        one = lib.pmatrix_evaluate(REFERENCE(turn), sp.to_float32(first))
        other = lib.pmatrix_evaluate(REFERENCE(turn), sp.to_float32(second))
        together = lib.pmatrix_evaluate(REFERENCE(turn),
                                        sp.to_float32(first + second))

        product = lib.matrix_multiply(REFERENCE(one), REFERENCE(other))

        assert near_rows(values(lib, product), values(lib, together), 1e-3)

        for item in (one, other, together, product):
            lib.matrix_free(REFERENCE(item))
    finally:
        lib.pmatrix_free(REFERENCE(turn))


@given(angle=angles)
def test_a_rotation_is_undone_by_its_own_transpose(lib, angle):
    turn = rotation(lib)
    try:
        made = lib.pmatrix_evaluate(REFERENCE(turn), sp.to_float32(angle))
        back = lib.matrix_transpose(REFERENCE(made))
        product = lib.matrix_multiply(REFERENCE(made), REFERENCE(back))

        assert near_rows(values(lib, product), [[1.0, 0.0], [0.0, 1.0]], 1e-3)

        for item in (made, back, product):
            lib.matrix_free(REFERENCE(item))
    finally:
        lib.pmatrix_free(REFERENCE(turn))


@given(angle=angles)
def test_a_rotation_changes_no_area(lib, angle):
    """The determinant of a turn is one, at every angle."""
    turn = rotation(lib)
    try:
        made = lib.pmatrix_evaluate(REFERENCE(turn), sp.to_float32(angle))
        assert abs(lib.matrix_determinant(REFERENCE(made)) - 1.0) <= 1e-4
        lib.matrix_free(REFERENCE(made))
    finally:
        lib.pmatrix_free(REFERENCE(turn))


@given(angle=angles)
def test_every_element_of_the_answer_is_its_own_function_at_that_value(lib,
                                                                       angle):
    """The whole matrix must agree with the elements read one at a time."""
    angle = sp.to_float32(angle)
    turn = rotation(lib)
    try:
        made = lib.pmatrix_evaluate(REFERENCE(turn), angle)
        answer = values(lib, made)

        for i in range(2):
            for j in range(2):
                assert answer[i][j] == lib.pmatrix_evaluate_element(
                    REFERENCE(turn), i, j, angle)

        lib.matrix_free(REFERENCE(made))
    finally:
        lib.pmatrix_free(REFERENCE(turn))


@given(angle=angles)
def test_writing_into_room_that_is_held_gives_what_getting_room_gives(lib,
                                                                      angle):
    """The library must run with no heap, thus the twin that writes into room
    the caller already holds must give the same matrix.
    """
    angle = sp.to_float32(angle)
    turn = rotation(lib)
    try:
        got = lib.pmatrix_evaluate(REFERENCE(turn), angle)
        room = lib.matrix_alloc(2, 2)
        lib.pmatrix_evaluate_into(REFERENCE(turn), angle, REFERENCE(room))

        assert values(lib, room) == values(lib, got)

        for item in (got, room):
            lib.matrix_free(REFERENCE(item))
    finally:
        lib.pmatrix_free(REFERENCE(turn))


@given(rows=st.integers(min_value=1, max_value=4),
       columns=st.integers(min_value=1, max_value=4),
       value=angles)
def test_a_new_matrix_gives_nothing_at_every_place(lib, rows, columns, value):
    """An element that holds no function gives zero, thus a caller who wants a
    zero at one place needs no function for it.
    """
    value = sp.to_float32(value)
    matrix = lib.pmatrix_alloc(rows, columns)
    try:
        for i in range(rows):
            for j in range(columns):
                assert not lib.pmatrix_get_element(REFERENCE(matrix), i, j)
                assert lib.pmatrix_evaluate_element(REFERENCE(matrix), i, j,
                                                    value) == 0.0

        made = lib.pmatrix_evaluate(REFERENCE(matrix), value)
        assert lib.matrix_is_zero(REFERENCE(made))
        lib.matrix_free(REFERENCE(made))
    finally:
        lib.pmatrix_free(REFERENCE(matrix))


@given(rows=st.integers(min_value=1, max_value=4),
       columns=st.integers(min_value=1, max_value=4),
       value=angles)
def test_clearing_a_matrix_takes_every_function_out_of_it(lib, rows, columns,
                                                          value):
    value = sp.to_float32(value)
    matrix = lib.pmatrix_alloc(rows, columns)
    try:
        for i in range(rows):
            for j in range(columns):
                lib.pmatrix_add_element(REFERENCE(matrix), i, j, twice)

        lib.pmatrix_set_zero(REFERENCE(matrix))

        made = lib.pmatrix_evaluate(REFERENCE(matrix), value)
        assert lib.matrix_is_zero(REFERENCE(made))
        lib.matrix_free(REFERENCE(made))
    finally:
        lib.pmatrix_free(REFERENCE(matrix))


@given(value=angles)
def test_the_two_elements_the_module_gives_hold_whatever_the_parameter_is(
        lib, value):
    """A constant element must not move when the parameter does."""
    value = sp.to_float32(value)
    assert lib.pmatrix_zero(value) == 0.0
    assert lib.pmatrix_one(value) == 1.0


@given(rows=st.integers(min_value=1, max_value=3),
       columns=st.integers(min_value=1, max_value=3),
       value=angles)
def test_memory_of_the_caller_holds_what_memory_of_the_heap_holds(lib, rows,
                                                                  columns,
                                                                  value):
    value = sp.to_float32(value)
    room = (ffitt.PMATRIX_FUNCTION * (rows * columns))()

    heap = lib.pmatrix_alloc(rows, columns)
    given_room = lib.pmatrix_static_alloc(rows, columns, room)
    try:
        assert given_room.dynamic_alloc is False

        for i in range(rows):
            for j in range(columns):
                lib.pmatrix_add_element(REFERENCE(heap), i, j, twice)
                lib.pmatrix_add_element(REFERENCE(given_room), i, j, twice)

        first = lib.pmatrix_evaluate(REFERENCE(heap), value)
        second = lib.pmatrix_evaluate(REFERENCE(given_room), value)

        assert values(lib, first) == values(lib, second)
        assert values(lib, first) == [[sp.to_float32(2.0 * value)] * columns
                                      for _ in range(rows)]

        for item in (first, second):
            lib.matrix_free(REFERENCE(item))
    finally:
        lib.pmatrix_free(REFERENCE(heap))
        lib.pmatrix_free(REFERENCE(given_room))
