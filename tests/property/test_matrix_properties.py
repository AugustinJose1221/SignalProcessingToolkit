"""Properties of the matrix module.

The unit tests examine known examples. These tests examine rules that must
hold for every matrix. Hypothesis makes the matrices and looks for a matrix
that breaks a rule.
"""

import ctypes
import os
import sys

from hypothesis import given, assume
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402

REFERENCE = ctypes.byref


@given(rows=sp.matrix_rows())
def test_transpose_two_times_gives_the_first_matrix(lib, rows):
    matrix = ffitt.make_matrix(lib, rows)
    once = lib.matrix_transpose(REFERENCE(matrix))
    twice = lib.matrix_transpose(REFERENCE(once))

    assert ffitt.matrix_rows(lib, twice) == rows

    for item in (matrix, once, twice):
        lib.matrix_free(REFERENCE(item))


@given(rows=sp.matrix_rows())
def test_transpose_exchanges_the_order(lib, rows):
    matrix = ffitt.make_matrix(lib, rows)
    transpose = lib.matrix_transpose(REFERENCE(matrix))

    assert transpose.m == matrix.n
    assert transpose.n == matrix.m

    lib.matrix_free(REFERENCE(matrix))
    lib.matrix_free(REFERENCE(transpose))


@given(pair=sp.matrix_pair_of_the_same_order())
def test_addition_does_not_depend_on_the_order_of_the_matrices(lib, pair):
    first_rows, second_rows = pair
    first = ffitt.make_matrix(lib, first_rows)
    second = ffitt.make_matrix(lib, second_rows)

    forward = lib.matrix_add(REFERENCE(first), REFERENCE(second))
    backward = lib.matrix_add(REFERENCE(second), REFERENCE(first))

    assert ffitt.matrix_rows(lib, forward) == ffitt.matrix_rows(lib, backward)

    for item in (first, second, forward, backward):
        lib.matrix_free(REFERENCE(item))


@given(rows=sp.matrix_rows())
def test_a_matrix_minus_itself_is_a_zero_matrix(lib, rows):
    first = ffitt.make_matrix(lib, rows)
    second = ffitt.make_matrix(lib, rows)

    difference = lib.matrix_subtract(REFERENCE(first), REFERENCE(second))

    assert lib.matrix_is_zero(REFERENCE(difference))

    for item in (first, second, difference):
        lib.matrix_free(REFERENCE(item))


@given(pair=sp.matrix_pair_of_the_same_order())
def test_subtraction_in_the_other_order_changes_only_the_sign(lib, pair):
    first_rows, second_rows = pair
    first = ffitt.make_matrix(lib, first_rows)
    second = ffitt.make_matrix(lib, second_rows)

    forward = lib.matrix_subtract(REFERENCE(first), REFERENCE(second))
    backward = lib.matrix_subtract(REFERENCE(second), REFERENCE(first))
    negative = lib.matrix_multiply_scalar(REFERENCE(backward), -1.0)

    assert sp.rows_close(ffitt.matrix_rows(lib, forward),
                         ffitt.matrix_rows(lib, negative))

    for item in (first, second, forward, backward, negative):
        lib.matrix_free(REFERENCE(item))


@given(pair=sp.matrix_pair_of_the_same_order())
def test_addition_and_subtraction_are_opposite_operations(lib, pair):
    first_rows, second_rows = pair
    first = ffitt.make_matrix(lib, first_rows)
    second = ffitt.make_matrix(lib, second_rows)

    total = lib.matrix_add(REFERENCE(first), REFERENCE(second))
    back = lib.matrix_subtract(REFERENCE(total), REFERENCE(second))

    assert sp.rows_close(ffitt.matrix_rows(lib, back), first_rows,
                         relative=1e-3, absolute=1e-3)

    for item in (first, second, total, back):
        lib.matrix_free(REFERENCE(item))


@given(rows=sp.square_matrix_rows())
def test_a_matrix_does_not_change_when_the_unit_matrix_multiplies_it(lib, rows):
    matrix = ffitt.make_matrix(lib, rows)
    unit = lib.matrix_create_unit_matrix(len(rows))

    right = lib.matrix_multiply(REFERENCE(matrix), REFERENCE(unit))
    left = lib.matrix_multiply(REFERENCE(unit), REFERENCE(matrix))

    assert sp.rows_close(ffitt.matrix_rows(lib, right), rows)
    assert sp.rows_close(ffitt.matrix_rows(lib, left), rows)

    for item in (matrix, unit, right, left):
        lib.matrix_free(REFERENCE(item))


@given(pair=sp.multipliable_matrix_pair())
def test_the_transpose_of_a_product_is_the_product_of_the_transposes(lib, pair):
    first_rows, second_rows = pair
    first = ffitt.make_matrix(lib, first_rows)
    second = ffitt.make_matrix(lib, second_rows)

    product = lib.matrix_multiply(REFERENCE(first), REFERENCE(second))
    product_transpose = lib.matrix_transpose(REFERENCE(product))

    first_transpose = lib.matrix_transpose(REFERENCE(first))
    second_transpose = lib.matrix_transpose(REFERENCE(second))
    other_product = lib.matrix_multiply(REFERENCE(second_transpose),
                                        REFERENCE(first_transpose))

    assert sp.rows_close(ffitt.matrix_rows(lib, product_transpose),
                         ffitt.matrix_rows(lib, other_product),
                         relative=1e-3, absolute=1e-3)

    for item in (first, second, product, product_transpose,
                 first_transpose, second_transpose, other_product):
        lib.matrix_free(REFERENCE(item))


@given(rows=sp.square_matrix_rows(max_side=4))
def test_the_determinant_of_the_transpose_is_the_same(lib, rows):
    matrix = ffitt.make_matrix(lib, rows)
    transpose = lib.matrix_transpose(REFERENCE(matrix))

    first = lib.matrix_determinant(REFERENCE(matrix))
    second = lib.matrix_determinant(REFERENCE(transpose))

    tolerance = 1e-3 + (1e-5 * sp.determinant_bound(rows))
    assert abs(first - second) <= tolerance

    lib.matrix_free(REFERENCE(matrix))
    lib.matrix_free(REFERENCE(transpose))


@given(rows=sp.square_matrix_rows(min_side=2, max_side=4),
       first_row=st.integers(min_value=0, max_value=3),
       second_row=st.integers(min_value=0, max_value=3))
def test_the_determinant_is_zero_when_two_rows_are_the_same(lib, rows,
                                                            first_row, second_row):
    side = len(rows)
    assume(first_row < side and second_row < side)
    assume(first_row != second_row)

    rows = [list(row) for row in rows]
    rows[second_row] = list(rows[first_row])

    matrix = ffitt.make_matrix(lib, rows)
    determinant = lib.matrix_determinant(REFERENCE(matrix))

    assert abs(determinant) <= 1e-3 + (1e-5 * sp.determinant_bound(rows))

    lib.matrix_free(REFERENCE(matrix))


@given(pair=sp.square_matrix_pair())
def test_the_trace_of_a_sum_is_the_sum_of_the_traces(lib, pair):
    first_rows, second_rows = pair

    first = ffitt.make_matrix(lib, first_rows)
    second = ffitt.make_matrix(lib, second_rows)
    total = lib.matrix_add(REFERENCE(first), REFERENCE(second))

    assert sp.close(lib.matrix_trace(REFERENCE(total)),
                    lib.matrix_trace(REFERENCE(first))
                    + lib.matrix_trace(REFERENCE(second)),
                    relative=1e-3, absolute=1e-3)

    for item in (first, second, total):
        lib.matrix_free(REFERENCE(item))


@given(rows=sp.dominant_square_matrix_rows())
def test_a_matrix_multiplied_by_its_inverse_gives_the_unit_matrix(lib, rows):
    matrix = ffitt.make_matrix(lib, rows)
    inverse = lib.matrix_inverse(REFERENCE(matrix))
    product = lib.matrix_multiply(REFERENCE(matrix), REFERENCE(inverse))

    side = len(rows)
    for i in range(side):
        for j in range(side):
            expected = 1.0 if i == j else 0.0
            assert sp.close(lib.matrix_get_element(REFERENCE(product), i, j),
                            expected, relative=1e-3, absolute=1e-3)

    for item in (matrix, inverse, product):
        lib.matrix_free(REFERENCE(item))


@given(rows=sp.dominant_square_matrix_rows())
def test_the_inverse_of_the_inverse_gives_the_first_matrix(lib, rows):
    matrix = ffitt.make_matrix(lib, rows)
    inverse = lib.matrix_inverse(REFERENCE(matrix))
    again = lib.matrix_inverse(REFERENCE(inverse))

    assert sp.rows_close(ffitt.matrix_rows(lib, again), rows,
                         relative=1e-2, absolute=1e-2)

    for item in (matrix, inverse, again):
        lib.matrix_free(REFERENCE(item))


@given(rows=sp.matrix_rows())
def test_the_rows_and_the_columns_agree_with_the_elements(lib, rows):
    matrix = ffitt.make_matrix(lib, rows)

    for index in range(matrix.m):
        row = lib.matrix_get_nth_row(REFERENCE(matrix), index)
        assert row.m == 1
        assert row.n == matrix.n
        assert ffitt.matrix_rows(lib, row)[0] == rows[index]
        lib.matrix_free(REFERENCE(row))

    for index in range(matrix.n):
        column = lib.matrix_get_nth_col(REFERENCE(matrix), index)
        assert column.m == matrix.m
        assert column.n == 1
        values = [row[0] for row in ffitt.matrix_rows(lib, column)]
        assert values == [row[index] for row in rows]
        lib.matrix_free(REFERENCE(column))

    lib.matrix_free(REFERENCE(matrix))


@given(rows=sp.matrix_rows(), scalar=sp.elements(10.0))
def test_a_scalar_multiplies_every_element(lib, rows, scalar):
    matrix = ffitt.make_matrix(lib, rows)
    product = lib.matrix_multiply_scalar(REFERENCE(matrix), scalar)

    expected = [[value * scalar for value in row] for row in rows]
    assert sp.rows_close(ffitt.matrix_rows(lib, product), expected,
                         relative=1e-3, absolute=1e-3)

    lib.matrix_free(REFERENCE(matrix))
    lib.matrix_free(REFERENCE(product))


@given(rows=sp.matrix_rows())
def test_a_matrix_is_equal_to_itself(lib, rows):
    first = ffitt.make_matrix(lib, rows)
    second = ffitt.make_matrix(lib, rows)

    assert lib.matrix_is_equal(REFERENCE(first), REFERENCE(second))

    lib.matrix_free(REFERENCE(first))
    lib.matrix_free(REFERENCE(second))
