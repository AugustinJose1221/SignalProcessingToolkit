"""Properties of the vector with two values.

This module gives the operations of the vector module for the one size that a
program uses most, so that the caller does not repeat the size at every call.
It therefore makes ONE promise above all others: it must be the same module.

A convenience that quietly differs from the thing it is a convenience for is
worse than no convenience at all, because a caller who changes from one to the
other has no reason to look. Thus every test below sets the answer of this
module beside the answer of the general one for the same two values, and asks
for them to be the same to the last digit.

The rest of the tests are the laws a vector obeys, which hold whatever the size
is: the dot product does not care about the order, the length is the root of a
vector with itself, and the two ends of a triangle are never further apart than
the way round.
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

pairs = st.tuples(sp.elements(), sp.elements())


def make(lib, values):
    vector = lib.vector2d_alloc()
    for index, value in enumerate(values):
        lib.vector2d_add_point_at_index(REFERENCE(vector), index,
                                        sp.to_float32(value))
    return vector


def make_general(lib, values):
    vector = lib.vector_alloc(2)
    for index, value in enumerate(values):
        lib.vector_add_point_at_index(REFERENCE(vector), index,
                                      sp.to_float32(value))
    return vector


@given(values=pairs)
def test_it_holds_two_values_and_gives_back_what_was_put_in(lib, values):
    vector = make(lib, values)
    try:
        assert vector.size == 2
        for index, value in enumerate(values):
            assert lib.vector2d_get(REFERENCE(vector),
                                    index) == sp.to_float32(value)
    finally:
        lib.vector_free(REFERENCE(vector))


@given(first=pairs, second=pairs)
def test_it_gives_the_same_answers_as_the_module_it_stands_for(lib, first,
                                                               second):
    """The promise of the module, held to the last digit.

    Nothing here may be near. The two must agree exactly, because they are
    meant to be the same arithmetic written two ways.
    """
    short_first = make(lib, first)
    short_second = make(lib, second)
    long_first = make_general(lib, first)
    long_second = make_general(lib, second)
    try:
        assert (lib.vector2d_dot_product(REFERENCE(short_first),
                                         REFERENCE(short_second))
                == lib.vector_dot_product(REFERENCE(long_first),
                                          REFERENCE(long_second)))
        assert (lib.vector2d_norm(REFERENCE(short_first))
                == lib.vector_norm(REFERENCE(long_first)))
        for index in range(2):
            assert (lib.vector2d_get(REFERENCE(short_first), index)
                    == lib.vector_get(REFERENCE(long_first), index))
    finally:
        for item in (short_first, short_second, long_first, long_second):
            lib.vector_free(REFERENCE(item))


@given(values=pairs)
def test_writing_from_a_list_is_the_same_as_writing_one_at_a_time(lib,
                                                                  values):
    one_at_a_time = make(lib, values)
    from_a_list = lib.vector2d_alloc()
    try:
        room = ffitt.float_array([sp.to_float32(value) for value in values])
        lib.vector2d_add_from_array(REFERENCE(from_a_list), room)

        for index in range(2):
            assert (lib.vector2d_get(REFERENCE(from_a_list), index)
                    == lib.vector2d_get(REFERENCE(one_at_a_time), index))
    finally:
        for item in (one_at_a_time, from_a_list):
            lib.vector_free(REFERENCE(item))


@given(first=pairs, second=pairs)
def test_the_dot_product_does_not_care_which_vector_comes_first(lib, first,
                                                                second):
    one = make(lib, first)
    other = make(lib, second)
    try:
        assert (lib.vector2d_dot_product(REFERENCE(one), REFERENCE(other))
                == lib.vector2d_dot_product(REFERENCE(other), REFERENCE(one)))
    finally:
        for item in (one, other):
            lib.vector_free(REFERENCE(item))


@given(values=pairs)
def test_the_length_is_the_root_of_the_vector_with_itself(lib, values):
    """The definition of a length, and the one relation that ties the two
    functions of this module together.
    """
    vector = make(lib, values)
    try:
        length = lib.vector2d_norm(REFERENCE(vector))
        with_itself = lib.vector2d_dot_product(REFERENCE(vector),
                                               REFERENCE(vector))
        assert with_itself >= 0.0
        assert abs(length - math.sqrt(with_itself)) <= (1e-4 * (1.0 + length))
    finally:
        lib.vector_free(REFERENCE(vector))


@given(values=pairs)
def test_the_length_agrees_with_the_two_sides_of_a_right_angle(lib, values):
    vector = make(lib, values)
    try:
        wanted = math.hypot(sp.to_float32(values[0]), sp.to_float32(values[1]))
        length = lib.vector2d_norm(REFERENCE(vector))
        assert abs(length - wanted) <= (1e-4 * (1.0 + wanted))
    finally:
        lib.vector_free(REFERENCE(vector))


@given(first=pairs, second=pairs)
def test_two_vectors_never_stand_further_apart_than_their_lengths_added(
        lib, first, second):
    """The inequality of Cauchy and Schwarz, which every dot product obeys.

    The dot product of two vectors can never be larger than their lengths
    multiplied. A dot product worked out over the wrong number of values, or
    over one value twice, breaks this at once.
    """
    one = make(lib, first)
    other = make(lib, second)
    try:
        product = lib.vector2d_dot_product(REFERENCE(one), REFERENCE(other))
        lengths = (lib.vector2d_norm(REFERENCE(one))
                   * lib.vector2d_norm(REFERENCE(other)))
    finally:
        for item in (one, other):
            lib.vector_free(REFERENCE(item))

    assert abs(product) <= (lengths + 1e-3 * (1.0 + lengths))


@given(values=pairs, factor=st.integers(min_value=-8, max_value=8))
def test_making_a_vector_larger_makes_its_length_larger_by_as_much(lib,
                                                                   values,
                                                                   factor):
    scale = float(2 ** factor)
    plain = make(lib, values)
    scaled = make(lib, [sp.to_float32(value * scale) for value in values])
    try:
        short = lib.vector2d_norm(REFERENCE(plain))
        long = lib.vector2d_norm(REFERENCE(scaled))
    finally:
        for item in (plain, scaled):
            lib.vector_free(REFERENCE(item))

    assume(short > 1e-3)
    assert abs(long - short * scale) <= (1e-3 * short * scale)


@given(values=pairs)
def test_memory_of_the_caller_holds_what_memory_of_the_heap_holds(lib,
                                                                  values):
    room = ffitt.real_buffer(2)
    heap = make(lib, values)
    given_room = lib.vector2d_static_alloc(room)
    try:
        assert given_room.dynamic_alloc is False
        assert given_room.size == 2

        for index, value in enumerate(values):
            lib.vector2d_add_point_at_index(REFERENCE(given_room), index,
                                            sp.to_float32(value))
        for index in range(2):
            assert (lib.vector2d_get(REFERENCE(given_room), index)
                    == lib.vector2d_get(REFERENCE(heap), index))
        assert (lib.vector2d_norm(REFERENCE(given_room))
                == lib.vector2d_norm(REFERENCE(heap)))
    finally:
        for item in (heap, given_room):
            lib.vector_free(REFERENCE(item))
