"""Properties of the vector module."""

import ctypes
import math
import os
import sys

from hypothesis import given
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

REFERENCE = ctypes.byref


@st.composite
def vector_pair(draw, min_size=1, max_size=8):
    size = draw(st.integers(min_value=min_size, max_value=max_size))
    first = draw(st.lists(sp.elements(), min_size=size, max_size=size))
    second = draw(st.lists(sp.elements(), min_size=size, max_size=size))
    return first, second


values = st.lists(sp.elements(), min_size=1, max_size=8)


@given(pair=vector_pair())
def test_the_dot_product_does_not_depend_on_the_order(lib, pair):
    first_values, second_values = pair
    first = sptk.make_vector(lib, first_values)
    second = sptk.make_vector(lib, second_values)

    forward = lib.vector_dot_product(REFERENCE(first), REFERENCE(second))
    backward = lib.vector_dot_product(REFERENCE(second), REFERENCE(first))

    assert forward == backward

    lib.vector_free(REFERENCE(first))
    lib.vector_free(REFERENCE(second))


@given(data=values)
def test_the_norm_is_never_less_than_zero(lib, data):
    vector = sptk.make_vector(lib, data)

    assert lib.vector_norm(REFERENCE(vector)) >= 0.0

    lib.vector_free(REFERENCE(vector))


@given(data=values)
def test_the_norm_is_the_root_of_the_dot_product_with_itself(lib, data):
    vector = sptk.make_vector(lib, data)

    norm = lib.vector_norm(REFERENCE(vector))
    dot = lib.vector_dot_product(REFERENCE(vector), REFERENCE(vector))

    assert sp.close(norm * norm, dot, relative=1e-3, absolute=1e-3)

    lib.vector_free(REFERENCE(vector))


@given(data=values)
def test_the_norm_of_a_zero_vector_is_zero(lib, data):
    vector = sptk.make_vector(lib, [0.0] * len(data))

    assert lib.vector_norm(REFERENCE(vector)) == 0.0

    lib.vector_free(REFERENCE(vector))


@given(pair=vector_pair())
def test_the_dot_product_is_not_larger_than_the_product_of_the_norms(lib, pair):
    # This is the rule of Cauchy and Schwarz.
    first_values, second_values = pair
    first = sptk.make_vector(lib, first_values)
    second = sptk.make_vector(lib, second_values)

    dot = lib.vector_dot_product(REFERENCE(first), REFERENCE(second))
    bound = lib.vector_norm(REFERENCE(first)) * lib.vector_norm(REFERENCE(second))

    assert abs(dot) <= bound + 1e-3 + (1e-3 * bound)

    lib.vector_free(REFERENCE(first))
    lib.vector_free(REFERENCE(second))


@given(data=values, scalar=sp.elements(10.0))
def test_a_scalar_changes_the_norm_by_the_size_of_the_scalar(lib, data, scalar):
    vector = sptk.make_vector(lib, data)
    scaled = sptk.make_vector(lib, [value * scalar for value in data])

    norm = lib.vector_norm(REFERENCE(vector))
    scaled_norm = lib.vector_norm(REFERENCE(scaled))

    assert sp.close(scaled_norm, abs(scalar) * norm, relative=1e-3, absolute=1e-3)

    lib.vector_free(REFERENCE(vector))
    lib.vector_free(REFERENCE(scaled))


@given(data=values)
def test_the_get_function_gives_the_value_that_went_in(lib, data):
    vector = sptk.make_vector(lib, data)

    for index, value in enumerate(data):
        assert lib.vector_get(REFERENCE(vector), index) == value

    lib.vector_free(REFERENCE(vector))


@given(data=values)
def test_the_dot_product_with_a_zero_vector_is_zero(lib, data):
    vector = sptk.make_vector(lib, data)
    zero = sptk.make_vector(lib, [0.0] * len(data))

    assert lib.vector_dot_product(REFERENCE(vector), REFERENCE(zero)) == 0.0

    lib.vector_free(REFERENCE(vector))
    lib.vector_free(REFERENCE(zero))


@given(data=values)
def test_the_norm_is_not_less_than_the_largest_element(lib, data):
    vector = sptk.make_vector(lib, data)
    largest = max(abs(value) for value in data)

    norm = lib.vector_norm(REFERENCE(vector))

    assert norm >= largest - 1e-3 - (1e-3 * largest)
    assert norm <= (math.sqrt(len(data)) * largest) + 1e-3

    lib.vector_free(REFERENCE(vector))
