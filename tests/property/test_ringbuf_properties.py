"""Properties of the buffer that holds the last samples.

The buffer makes one promise and it is a promise about MEMORY, not about
arithmetic: it holds the newest samples and it forgets the rest. Nothing is
copied when a sample arrives, only one position changes, and that is exactly
the kind of code where an index that is one out gives an answer that looks
reasonable for a long time and is wrong.

Every test here is written against a plain Python list, which is slow, wasteful
and obviously right. The buffer must agree with it at every step.
"""

import ctypes
import os
import sys

from hypothesis import given
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402


def newest_first(arrived, size):
    """The model: the samples the buffer should hold, the newest one first."""
    return list(reversed(arrived[-size:]))


@given(signal=st.lists(sp.elements(), min_size=1, max_size=60),
       size=st.integers(min_value=1, max_value=20))
def test_an_age_names_the_sample_that_arrived_that_many_steps_ago(lib, signal,
                                                                 size):
    """The whole of what the buffer is for, held at every step of the way.

    An age is checked and not a position. That choice is the point of the
    module: the meaning of a number must not change as the buffer fills, and a
    position would change with it.
    """
    values = [sp.to_float32(value) for value in signal]
    ringbuf = lib.ringbuf_alloc(size)
    try:
        for step, value in enumerate(values):
            lib.ringbuf_put(ctypes.byref(ringbuf), value)
            held = newest_first(values[:step + 1], size)
            for age, expected in enumerate(held):
                assert lib.ringbuf_get(ctypes.byref(ringbuf), age) == expected
    finally:
        lib.ringbuf_free(ctypes.byref(ringbuf))


@given(signal=st.lists(sp.elements(), min_size=1, max_size=80),
       size=st.integers(min_value=1, max_value=16))
def test_everything_older_than_the_size_is_gone_beyond_recovery(lib, signal,
                                                               size):
    """The forgetting is the feature, thus it is tested as one.

    A buffer fed a long signal must be indistinguishable from a fresh buffer
    fed only the last size samples of it. If any trace of the older samples
    could still be read, the buffer would not be bounded and the promise that
    it costs the same whatever it holds would be worth nothing.
    """
    values = [sp.to_float32(value) for value in signal]

    fed_everything = lib.ringbuf_alloc(size)
    fed_the_tail = lib.ringbuf_alloc(size)
    try:
        for value in values:
            lib.ringbuf_put(ctypes.byref(fed_everything), value)
        for value in values[-size:]:
            lib.ringbuf_put(ctypes.byref(fed_the_tail), value)

        assert (lib.ringbuf_count(ctypes.byref(fed_everything))
                == lib.ringbuf_count(ctypes.byref(fed_the_tail)))
        for age in range(size):
            assert (lib.ringbuf_get(ctypes.byref(fed_everything), age)
                    == lib.ringbuf_get(ctypes.byref(fed_the_tail), age))
    finally:
        lib.ringbuf_free(ctypes.byref(fed_everything))
        lib.ringbuf_free(ctypes.byref(fed_the_tail))


@given(signal=st.lists(sp.elements(), min_size=0, max_size=60),
       size=st.integers(min_value=1, max_value=20))
def test_the_count_climbs_to_the_size_and_then_stands_still(lib, signal, size):
    values = [sp.to_float32(value) for value in signal]
    ringbuf = lib.ringbuf_alloc(size)
    try:
        assert lib.ringbuf_count(ctypes.byref(ringbuf)) == 0
        for step, value in enumerate(values):
            lib.ringbuf_put(ctypes.byref(ringbuf), value)
            assert (lib.ringbuf_count(ctypes.byref(ringbuf))
                    == min(step + 1, size))
    finally:
        lib.ringbuf_free(ctypes.byref(ringbuf))


@given(signal=st.lists(sp.elements(), min_size=0, max_size=40),
       size=st.integers(min_value=1, max_value=20))
def test_full_says_the_same_thing_as_the_count(lib, signal, size):
    values = [sp.to_float32(value) for value in signal]
    ringbuf = lib.ringbuf_alloc(size)
    try:
        for value in values:
            lib.ringbuf_put(ctypes.byref(ringbuf), value)
            full = lib.ringbuf_is_full(ctypes.byref(ringbuf))
            assert full == (lib.ringbuf_count(ctypes.byref(ringbuf)) == size)
    finally:
        lib.ringbuf_free(ctypes.byref(ringbuf))


@given(signal=st.lists(sp.elements(), min_size=0, max_size=60),
       size=st.integers(min_value=1, max_value=20))
def test_a_copy_runs_the_other_way_round_from_an_age(lib, signal, size):
    """copy writes the oldest first. get counts from the newest.

    The two therefore run in opposite directions and a caller who mixes them up
    reads the block backwards. The relation between them is fixed here so that
    it cannot drift.
    """
    values = [sp.to_float32(value) for value in signal]
    ringbuf = lib.ringbuf_alloc(size)
    room = sptk.real_buffer(size)
    try:
        for step, value in enumerate(values):
            lib.ringbuf_put(ctypes.byref(ringbuf), value)
            count = lib.ringbuf_count(ctypes.byref(ringbuf))
            written = lib.ringbuf_copy(ctypes.byref(ringbuf), room)
            assert written == count
            for age in range(count):
                assert (room[count - 1 - age]
                        == lib.ringbuf_get(ctypes.byref(ringbuf), age))
            assert list(room[:count]) == list(reversed(
                newest_first(values[:step + 1], size)))
    finally:
        lib.ringbuf_free(ctypes.byref(ringbuf))


@given(signal=st.lists(sp.elements(), min_size=0, max_size=30),
       size=st.integers(min_value=1, max_value=12),
       reach=st.integers(min_value=0, max_value=40))
def test_an_age_that_is_not_held_gives_nothing_rather_than_rubbish(lib, signal,
                                                                  size, reach):
    """Reading past the end must give 0, at any age, at any point of filling.

    This is the case that a wrong index reaches: it would give whatever value
    happened to lie in the memory, which for a buffer that has wrapped is a
    real sample from long ago and looks entirely believable.
    """
    values = [sp.to_float32(value) for value in signal]
    ringbuf = lib.ringbuf_alloc(size)
    try:
        for value in values:
            lib.ringbuf_put(ctypes.byref(ringbuf), value)
        count = lib.ringbuf_count(ctypes.byref(ringbuf))
        if reach >= count:
            assert lib.ringbuf_get(ctypes.byref(ringbuf), reach) == 0.0
    finally:
        lib.ringbuf_free(ctypes.byref(ringbuf))


@given(signal=st.lists(sp.elements(), min_size=1, max_size=40),
       size=st.integers(min_value=1, max_value=15),
       after=st.lists(sp.elements(), min_size=0, max_size=20))
def test_a_reset_leaves_a_buffer_that_cannot_be_told_from_a_new_one(lib,
                                                                   signal,
                                                                   size,
                                                                   after):
    values = [sp.to_float32(value) for value in signal]
    later = [sp.to_float32(value) for value in after]

    used = lib.ringbuf_alloc(size)
    fresh = lib.ringbuf_alloc(size)
    try:
        for value in values:
            lib.ringbuf_put(ctypes.byref(used), value)
        lib.ringbuf_reset(ctypes.byref(used))
        assert lib.ringbuf_count(ctypes.byref(used)) == 0

        for value in later:
            lib.ringbuf_put(ctypes.byref(used), value)
            lib.ringbuf_put(ctypes.byref(fresh), value)
        for age in range(size):
            assert (lib.ringbuf_get(ctypes.byref(used), age)
                    == lib.ringbuf_get(ctypes.byref(fresh), age))
    finally:
        lib.ringbuf_free(ctypes.byref(used))
        lib.ringbuf_free(ctypes.byref(fresh))


@given(signal=st.lists(sp.elements(), min_size=0, max_size=40),
       size=st.integers(min_value=1, max_value=15))
def test_memory_of_the_caller_holds_what_memory_of_the_heap_holds(lib, signal,
                                                                 size):
    """The two ways to build a buffer must give the same buffer.

    A library that can be used with no heap at all is only worth the claim if
    the two roads truly meet, and the static one is the road that the small
    machines take.
    """
    values = [sp.to_float32(value) for value in signal]
    room = sptk.real_buffer(size)

    heap = lib.ringbuf_alloc(size)
    given_room = lib.ringbuf_static_alloc(size, room)
    try:
        assert given_room.dynamic_alloc is False
        for value in values:
            lib.ringbuf_put(ctypes.byref(heap), value)
            lib.ringbuf_put(ctypes.byref(given_room), value)
            assert (lib.ringbuf_count(ctypes.byref(heap))
                    == lib.ringbuf_count(ctypes.byref(given_room)))
            for age in range(size):
                assert (lib.ringbuf_get(ctypes.byref(heap), age)
                        == lib.ringbuf_get(ctypes.byref(given_room), age))
    finally:
        lib.ringbuf_free(ctypes.byref(heap))
        lib.ringbuf_free(ctypes.byref(given_room))


@given(signal=st.lists(sp.elements(), min_size=1, max_size=50),
       size=st.integers(min_value=1, max_value=20))
def test_the_sample_just_put_in_is_always_the_one_of_age_zero(lib, signal,
                                                             size):
    values = [sp.to_float32(value) for value in signal]
    ringbuf = lib.ringbuf_alloc(size)
    try:
        for value in values:
            lib.ringbuf_put(ctypes.byref(ringbuf), value)
            assert lib.ringbuf_get(ctypes.byref(ringbuf), 0) == value
    finally:
        lib.ringbuf_free(ctypes.byref(ringbuf))


@given(size=st.integers(min_value=1, max_value=64),
       rounds=st.integers(min_value=1, max_value=6))
def test_wrapping_round_many_times_changes_nothing(lib, size, rounds):
    """A size that is not a power of two is the case the module allows.

    The header says so in as many words. A buffer whose position is masked
    rather than counted works for 16 and fails for 17, and it fails only after
    it has wrapped, thus not in any short test.
    """
    values = [sp.to_float32(float(index)) for index in range(size * rounds + 3)]
    ringbuf = lib.ringbuf_alloc(size)
    try:
        for value in values:
            lib.ringbuf_put(ctypes.byref(ringbuf), value)
        for age in range(size):
            assert (lib.ringbuf_get(ctypes.byref(ringbuf), age)
                    == values[-1 - age])
    finally:
        lib.ringbuf_free(ctypes.byref(ringbuf))
