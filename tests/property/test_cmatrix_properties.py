"""Properties of the matrix of complex numbers.

The identities of linear algebra hold whatever the numbers are, thus most of
what is tested here is what is tested for the matrix of real numbers. What is
NOT the same is the reason this module exists, and those tests carry the
weight:

    THE CONJUGATE TRANSPOSE, and not the transpose, is the operation that
    matters once the numbers are complex. It is the one that turns the product
    round, it is the one that makes a Hermitian matrix, and a module that used
    a plain transpose in its place would pass every test built on real numbers
    and be wrong for every matrix with an imaginary part.

Each answer is also set against the same arithmetic worked out in Python with
its own complex numbers, which is a different road to the same place.
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

REFERENCE = ctypes.byref

# Small numbers and small matrices. A determinant grows as the product of its
# elements, and a 3 by 3 of numbers near a hundred is already past what 32 bits
# can hold without losing the digits the comparison needs.
parts = st.floats(min_value=-8.0, max_value=8.0, allow_nan=False,
                  allow_infinity=False, width=32)


@st.composite
def complex_rows(draw, rows=None, columns=None, min_side=1, max_side=3):
    if rows is None:
        rows = draw(st.integers(min_value=min_side, max_value=max_side))
    if columns is None:
        columns = draw(st.integers(min_value=min_side, max_value=max_side))
    return [[complex(sp.to_float32(draw(parts)), sp.to_float32(draw(parts)))
             for _ in range(columns)]
            for _ in range(rows)]


@st.composite
def square_complex_rows(draw, min_side=1, max_side=3):
    side = draw(st.integers(min_value=min_side, max_value=max_side))
    return draw(complex_rows(rows=side, columns=side))


@st.composite
def complex_pair_of_the_same_order(draw, min_side=1, max_side=3):
    rows = draw(st.integers(min_value=min_side, max_value=max_side))
    columns = draw(st.integers(min_value=min_side, max_value=max_side))
    return (draw(complex_rows(rows=rows, columns=columns)),
            draw(complex_rows(rows=rows, columns=columns)))


@st.composite
def multipliable_complex_pair(draw, min_side=1, max_side=3):
    rows = draw(st.integers(min_value=min_side, max_value=max_side))
    shared = draw(st.integers(min_value=min_side, max_value=max_side))
    columns = draw(st.integers(min_value=min_side, max_value=max_side))
    return (draw(complex_rows(rows=rows, columns=shared)),
            draw(complex_rows(rows=shared, columns=columns)))


@st.composite
def square_complex_pair(draw, min_side=1, max_side=3):
    side = draw(st.integers(min_value=min_side, max_value=max_side))
    return (draw(complex_rows(rows=side, columns=side)),
            draw(complex_rows(rows=side, columns=side)))


def make(lib, rows):
    """Give a matrix of the library that holds the given complex numbers."""
    m = len(rows)
    n = len(rows[0])
    matrix = lib.cmatrix_alloc(m, n)
    for i in range(m):
        for j in range(n):
            lib.cmatrix_add_element(REFERENCE(matrix), i, j,
                                    lib.cnum_make(sp.to_float32(rows[i][j].real),
                                                  sp.to_float32(rows[i][j].imag)))
    return matrix


def held(lib, matrix):
    """Give the elements of a matrix of the library as Python numbers."""
    out = []
    for i in range(matrix.m):
        row = []
        for j in range(matrix.n):
            value = lib.cmatrix_get_element(REFERENCE(matrix), i, j)
            row.append(complex(value.re, value.im))
        out.append(row)
    return out


def to_complex(value):
    return complex(value.re, value.im)


def near(first, second, room=1e-3):
    return abs(first - second) <= (room * (1.0 + abs(first) + abs(second)))


def hadamard(rows):
    """A bound on how large a determinant of these rows can be.

    The determinant is a sum of products, one element taken from each row, and
    it can be no larger than the lengths of the rows multiplied together. That
    bound is the natural SIZE OF THE ARITHMETIC that works it out, and it is
    what the rounding of that arithmetic must be weighed against.

    Weighing the rounding against the ANSWER instead is wrong in exactly the
    case that matters. A determinant near nothing is one where large products
    cancelled, and the digits thrown away in that cancelling do not become
    small merely because their sum did.
    """
    total = 1.0
    for row in rows:
        total *= math.sqrt(sum(abs(value) ** 2 for value in row)) or 1.0
    return total


def rows_near(first, second, room=1e-3):
    if len(first) != len(second):
        return False
    return all(near(a, b, room)
               for row_a, row_b in zip(first, second)
               for a, b in zip(row_a, row_b))


def by_hand_multiply(first, second):
    shared = len(second)
    return [[sum(first[i][k] * second[k][j] for k in range(shared))
             for j in range(len(second[0]))]
            for i in range(len(first))]


def by_hand_determinant(rows):
    size = len(rows)
    if size == 1:
        return rows[0][0]
    total = 0.0 + 0.0j
    for column in range(size):
        smaller = [[row[j] for j in range(size) if j != column]
                   for row in rows[1:]]
        sign = 1 if (column % 2 == 0) else -1
        total += sign * rows[0][column] * by_hand_determinant(smaller)
    return total


@given(rows=complex_rows())
def test_the_conjugate_transpose_two_times_gives_the_first_matrix(lib, rows):
    matrix = make(lib, rows)
    once = lib.cmatrix_conjugate_transpose(REFERENCE(matrix))
    twice = lib.cmatrix_conjugate_transpose(REFERENCE(once))

    assert held(lib, twice) == rows

    for item in (matrix, once, twice):
        lib.cmatrix_free(REFERENCE(item))


@given(rows=complex_rows())
def test_the_conjugate_transpose_turns_the_sign_of_every_imaginary_part(
        lib, rows):
    """The transpose alone is not enough, and the difference is the module.

    A matrix whose elements have imaginary parts must give a DIFFERENT answer
    for the two operations. A module that quietly used a plain transpose for
    both would pass every test written for real numbers.
    """
    matrix = make(lib, rows)
    plain = lib.cmatrix_transpose(REFERENCE(matrix))
    conjugate = lib.cmatrix_conjugate_transpose(REFERENCE(matrix))

    wanted = [[rows[j][i].conjugate() for j in range(len(rows))]
              for i in range(len(rows[0]))]
    assert held(lib, conjugate) == wanted

    has_an_imaginary_part = any(value.imag != 0.0
                                for row in rows for value in row)
    assert (held(lib, plain) != held(lib, conjugate)) == has_an_imaginary_part

    for item in (matrix, plain, conjugate):
        lib.cmatrix_free(REFERENCE(item))


@given(pair=multipliable_complex_pair())
def test_the_conjugate_transpose_of_a_product_turns_the_product_round(lib,
                                                                      pair):
    """(AB) conjugate transposed is B conjugate transposed times A.

    The order is exchanged. This is the identity that every use of a complex
    matrix rests on, and it is the one that fails silently if the conjugating
    is left out of any single step.
    """
    first_rows, second_rows = pair
    first = make(lib, first_rows)
    second = make(lib, second_rows)

    product = lib.cmatrix_multiply(REFERENCE(first), REFERENCE(second))
    left = lib.cmatrix_conjugate_transpose(REFERENCE(product))

    first_star = lib.cmatrix_conjugate_transpose(REFERENCE(first))
    second_star = lib.cmatrix_conjugate_transpose(REFERENCE(second))
    right = lib.cmatrix_multiply(REFERENCE(second_star), REFERENCE(first_star))

    assert rows_near(held(lib, left), held(lib, right))

    for item in (first, second, product, left, first_star, second_star, right):
        lib.cmatrix_free(REFERENCE(item))


@given(rows=complex_rows())
def test_a_matrix_times_its_own_conjugate_transpose_is_hermitian(lib, rows):
    """The way a Hermitian matrix is made in practice, and the test of the
    reading that says so.

    A Hermitian matrix is equal to its own conjugate transpose. That forces
    every value on the diagonal to be real, because only a real number is its
    own conjugate.
    """
    matrix = make(lib, rows)
    star = lib.cmatrix_conjugate_transpose(REFERENCE(matrix))
    product = lib.cmatrix_multiply(REFERENCE(matrix), REFERENCE(star))

    values = held(lib, product)
    size = len(values)
    for index in range(size):
        assert abs(values[index][index].imag) <= 1e-3
    for i in range(size):
        for j in range(size):
            assert near(values[i][j], values[j][i].conjugate())

    for item in (matrix, star, product):
        lib.cmatrix_free(REFERENCE(item))


@given(rows=square_complex_rows())
def test_the_determinant_of_the_conjugate_transpose_is_the_conjugate(lib,
                                                                     rows):
    """det of the transpose is the same. det of the CONJUGATE transpose is the
    conjugate of it. The two differ, and only the second is right here.
    """
    matrix = make(lib, rows)
    plain = lib.cmatrix_transpose(REFERENCE(matrix))
    star = lib.cmatrix_conjugate_transpose(REFERENCE(matrix))

    first = to_complex(lib.cmatrix_determinant(REFERENCE(matrix)))
    turned = to_complex(lib.cmatrix_determinant(REFERENCE(plain)))
    starred = to_complex(lib.cmatrix_determinant(REFERENCE(star)))

    assert near(turned, first, 1e-2)
    assert near(starred, first.conjugate(), 1e-2)

    for item in (matrix, plain, star):
        lib.cmatrix_free(REFERENCE(item))


@given(rows=square_complex_rows())
def test_the_determinant_agrees_with_one_worked_out_by_hand(lib, rows):
    """A second road to the same number, taken along the rows in Python."""
    matrix = make(lib, rows)
    answer = to_complex(lib.cmatrix_determinant(REFERENCE(matrix)))
    lib.cmatrix_free(REFERENCE(matrix))

    assert near(answer, by_hand_determinant(rows), 1e-2)


@given(pair=square_complex_pair())
def test_the_determinant_of_a_product_is_the_product_of_the_determinants(
        lib, pair):
    first_rows, second_rows = pair
    first = make(lib, first_rows)
    second = make(lib, second_rows)
    product = lib.cmatrix_multiply(REFERENCE(first), REFERENCE(second))

    together = to_complex(lib.cmatrix_determinant(REFERENCE(product)))
    apart = (to_complex(lib.cmatrix_determinant(REFERENCE(first)))
             * to_complex(lib.cmatrix_determinant(REFERENCE(second))))

    for item in (first, second, product):
        lib.cmatrix_free(REFERENCE(item))

    # The two roads to the same number pass through different products, thus
    # they throw away different digits. How many they can throw away is set by
    # the size of the products and not by the size of what is left at the end.
    room = 1e-3 * (hadamard(first_rows) * hadamard(second_rows) + 1.0)

    assert abs(together - apart) <= room


@given(pair=multipliable_complex_pair())
def test_the_product_agrees_with_one_worked_out_by_hand(lib, pair):
    first_rows, second_rows = pair
    first = make(lib, first_rows)
    second = make(lib, second_rows)
    product = lib.cmatrix_multiply(REFERENCE(first), REFERENCE(second))

    answer = held(lib, product)

    for item in (first, second, product):
        lib.cmatrix_free(REFERENCE(item))

    assert rows_near(answer, by_hand_multiply(first_rows, second_rows))


@given(rows=square_complex_rows())
def test_a_matrix_multiplied_by_its_inverse_gives_the_unit_matrix(lib, rows):
    matrix = make(lib, rows)
    determinant = to_complex(lib.cmatrix_determinant(REFERENCE(matrix)))
    # A matrix that is nearly singular cannot be inverted at the width the
    # library holds, and the module is not claiming otherwise.
    assume(abs(determinant) > 0.5)

    inverse = lib.cmatrix_inverse(REFERENCE(matrix))
    product = lib.cmatrix_multiply(REFERENCE(matrix), REFERENCE(inverse))
    values = held(lib, product)

    for item in (matrix, inverse, product):
        lib.cmatrix_free(REFERENCE(item))

    size = len(rows)
    for i in range(size):
        for j in range(size):
            wanted = complex(1.0, 0.0) if (i == j) else complex(0.0, 0.0)
            assert near(values[i][j], wanted, 1e-2)


@given(pair=square_complex_pair())
def test_the_trace_of_a_product_does_not_change_when_the_order_does(lib,
                                                                    pair):
    """A product changes when its order changes. Its trace does not.

    This holds for every pair of square matrices and for no obvious reason,
    thus it catches a fault in the multiplying that a check of the shape would
    not.
    """
    first_rows, second_rows = pair
    first = make(lib, first_rows)
    second = make(lib, second_rows)

    forward = lib.cmatrix_multiply(REFERENCE(first), REFERENCE(second))
    backward = lib.cmatrix_multiply(REFERENCE(second), REFERENCE(first))

    one = to_complex(lib.cmatrix_trace(REFERENCE(forward)))
    other = to_complex(lib.cmatrix_trace(REFERENCE(backward)))

    for item in (first, second, forward, backward):
        lib.cmatrix_free(REFERENCE(item))

    assert near(one, other, 1e-2)


@given(pair=complex_pair_of_the_same_order())
def test_the_trace_of_a_sum_is_the_sum_of_the_traces(lib, pair):
    first_rows, second_rows = pair
    assume(len(first_rows) == len(first_rows[0]))

    first = make(lib, first_rows)
    second = make(lib, second_rows)
    total = lib.cmatrix_add(REFERENCE(first), REFERENCE(second))

    together = to_complex(lib.cmatrix_trace(REFERENCE(total)))
    apart = (to_complex(lib.cmatrix_trace(REFERENCE(first)))
             + to_complex(lib.cmatrix_trace(REFERENCE(second))))

    for item in (first, second, total):
        lib.cmatrix_free(REFERENCE(item))

    assert near(together, apart)


@given(pair=complex_pair_of_the_same_order())
def test_adding_and_subtracting_the_same_matrix_changes_nothing(lib, pair):
    first_rows, second_rows = pair
    first = make(lib, first_rows)
    second = make(lib, second_rows)

    total = lib.cmatrix_add(REFERENCE(first), REFERENCE(second))
    back = lib.cmatrix_subtract(REFERENCE(total), REFERENCE(second))

    assert rows_near(held(lib, back), first_rows)

    for item in (first, second, total, back):
        lib.cmatrix_free(REFERENCE(item))


@given(rows=complex_rows(), scalar=st.tuples(parts, parts))
def test_a_scalar_multiplies_every_element(lib, rows, scalar):
    value = complex(sp.to_float32(scalar[0]), sp.to_float32(scalar[1]))
    matrix = make(lib, rows)
    scaled = lib.cmatrix_multiply_scalar(
        REFERENCE(matrix), lib.cnum_make(sp.to_float32(value.real),
                                         sp.to_float32(value.imag)))

    answer = held(lib, scaled)

    for item in (matrix, scaled):
        lib.cmatrix_free(REFERENCE(item))

    assert rows_near(answer, [[element * value for element in row]
                              for row in rows])


@given(rows=square_complex_rows())
def test_the_unit_matrix_changes_nothing(lib, rows):
    matrix = make(lib, rows)
    unit = lib.cmatrix_create_unit_matrix(len(rows))
    product = lib.cmatrix_multiply(REFERENCE(matrix), REFERENCE(unit))

    assert rows_near(held(lib, product), rows)
    assert lib.cmatrix_is_unit(REFERENCE(unit))

    for item in (matrix, unit, product):
        lib.cmatrix_free(REFERENCE(item))


@given(rows=square_complex_rows(min_side=2),
       first=st.integers(min_value=0, max_value=2),
       second=st.integers(min_value=0, max_value=2))
def test_two_rows_that_are_the_same_leave_no_determinant(lib, rows, first,
                                                          second):
    size = len(rows)
    first %= size
    second %= size
    assume(first != second)

    rows = [list(row) for row in rows]
    rows[second] = list(rows[first])

    matrix = make(lib, rows)
    answer = to_complex(lib.cmatrix_determinant(REFERENCE(matrix)))
    lib.cmatrix_free(REFERENCE(matrix))

    assert abs(answer) <= 1e-2


@given(pair=complex_pair_of_the_same_order())
def test_writing_into_room_that_is_held_gives_what_getting_room_gives(lib,
                                                                      pair):
    """The library must run with no heap at all, thus every operation that
    gets memory has a twin that does not. The two must give the same answer or
    the small targets are running a different library.
    """
    first_rows, second_rows = pair
    first = make(lib, first_rows)
    second = make(lib, second_rows)

    got = lib.cmatrix_add(REFERENCE(first), REFERENCE(second))
    room = lib.cmatrix_alloc(len(first_rows), len(first_rows[0]))
    lib.cmatrix_add_into(REFERENCE(first), REFERENCE(second), REFERENCE(room))

    assert held(lib, room) == held(lib, got)

    for item in (first, second, got, room):
        lib.cmatrix_free(REFERENCE(item))


@given(pair=multipliable_complex_pair())
def test_the_same_holds_for_the_multiplying(lib, pair):
    first_rows, second_rows = pair
    first = make(lib, first_rows)
    second = make(lib, second_rows)

    got = lib.cmatrix_multiply(REFERENCE(first), REFERENCE(second))
    room = lib.cmatrix_alloc(len(first_rows), len(second_rows[0]))
    lib.cmatrix_multiply_into(REFERENCE(first), REFERENCE(second),
                              REFERENCE(room))

    assert held(lib, room) == held(lib, got)

    for item in (first, second, got, room):
        lib.cmatrix_free(REFERENCE(item))


@given(rows=complex_rows())
def test_a_matrix_is_equal_to_itself_and_near_itself(lib, rows):
    matrix = make(lib, rows)
    copy = lib.cmatrix_alloc(len(rows), len(rows[0]))
    lib.cmatrix_copy(REFERENCE(matrix), REFERENCE(copy))

    assert lib.cmatrix_is_equal(REFERENCE(matrix), REFERENCE(copy))
    assert lib.cmatrix_is_near(REFERENCE(matrix), REFERENCE(copy),
                               sp.to_float32(0.0))

    for item in (matrix, copy):
        lib.cmatrix_free(REFERENCE(item))
