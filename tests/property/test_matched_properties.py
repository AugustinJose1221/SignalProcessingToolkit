"""Rules that looking for a known shape must keep.

The whole worth of the module rests on two claims: that the score means the same
thing whatever shape was looked for, and that the threshold means what it says.
Both are claims about every shape and every reading, thus both are for generated
tests rather than for chosen ones.
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

RUNS = settings(max_examples=40, deadline=None)

READING = 400


@st.composite
def shape(draw, least=2, most=24):
    """A shape with energy in it. A shape of nothing is refused by the module
    and is examined on its own below."""
    length = draw(st.integers(min_value=least, max_value=most))
    values = draw(st.lists(sp.elements(4.0), min_size=length, max_size=length))

    assume(sum(value * value for value in values) > 0.25)

    return values


def designed(lib, values):
    """Give a filter and the array its shape lives in.

    THE ARRAY MUST BE KEPT. The module holds the pointer and does not copy the
    shape, thus an array that Python collects leaves the filter pointing at
    memory that is no longer there."""
    kept = ffitt.float_array(values)
    filt = lib.matched_make()

    assert lib.matched_design(filt, kept, len(values))

    return filt, kept


def noise(count, seed=1):
    state = seed
    out = []

    for _ in range(count):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        out.append(sp.to_float32((((state >> 16) % 20000) / 10000.0) - 1.0))

    return out


@given(shape(), st.integers(min_value=0, max_value=40))
@RUNS
def test_a_shape_standing_alone_in_the_reading_is_found_where_it_stands(
        lib, values, at):
    """THE REASON THE MODULE EXISTS, in the case where the answer is not in
    doubt. The reading holds the shape and nothing else, thus the largest score
    must stand where the shape begins and nowhere else."""
    filt, kept = designed(lib, values)

    reading = [0.0] * READING
    for index, value in enumerate(values):
        reading[at + index] = value

    signal = ffitt.float_array(reading)

    where = ctypes.c_uint32(0)
    best = ffitt.real_buffer(1)

    assert lib.matched_best(filt, signal, READING, ctypes.byref(where), best)
    assert where.value == at

    del kept


@given(shape(), st.floats(min_value=0.25, max_value=8.0, width=32))
@RUNS
def test_the_score_does_not_depend_on_how_loud_the_shape_is(lib, values,
                                                            louder):
    """THE CLAIM THAT LETS ONE THRESHOLD SERVE EVERY SHAPE. The same shape made
    louder is the same shape, thus scoring a reading against it must give the
    same score. Without this the caller would have to work out a threshold for
    each shape, and the false alarm rate would mean nothing."""
    plain, kept_plain = designed(lib, values)
    big, kept_big = designed(lib, [sp.to_float32(value * louder)
                                   for value in values])

    signal = ffitt.float_array(noise(READING))

    one = ffitt.real_buffer(READING)
    other = ffitt.real_buffer(READING)

    assert lib.matched_score_block(plain, signal, READING, one)
    assert lib.matched_score_block(big, signal, READING, other)

    offsets = READING - len(values) + 1

    for index in range(offsets):
        assert abs(one[index] - other[index]) <= 1e-3 * (1.0 + abs(one[index]))

    del kept_plain, kept_big


@given(shape(), sp.elements(4.0))
@RUNS
def test_making_the_reading_louder_makes_the_score_louder_by_as_much(lib,
                                                                     values,
                                                                     louder):
    """The other half of the same claim. The score stands in units of the
    reading, thus a reading twice as loud gives a score twice as large, and
    dividing by the noise of the reading takes both away together."""
    assume(abs(louder) > 0.25)

    filt, kept = designed(lib, values)

    quiet = noise(READING)
    loud = [sp.to_float32(value * louder) for value in quiet]

    one = ffitt.real_buffer(READING)
    other = ffitt.real_buffer(READING)

    assert lib.matched_score_block(filt, ffitt.float_array(quiet), READING, one)
    assert lib.matched_score_block(filt, ffitt.float_array(loud), READING,
                                   other)

    offsets = READING - len(values) + 1

    for index in range(offsets):
        room = 1e-3 * (1.0 + abs(louder) * (1.0 + abs(one[index])))
        assert abs(other[index] - (one[index] * louder)) <= room

    del kept


@given(shape())
@RUNS
def test_a_shape_scored_against_itself_gives_the_root_of_its_energy(lib,
                                                                    values):
    """The largest score any reading of a given loudness can give, and the
    number the whole scale rests on. A shape scored against a copy of itself
    gives the root of its own energy, because that is what the sum of the
    products divided by that root comes to."""
    filt, kept = designed(lib, values)

    energy = sum(value * value for value in values)

    score = lib.matched_score_at(filt, ffitt.float_array(values))

    assert abs(score - math.sqrt(energy)) <= 1e-3 * (1.0 + math.sqrt(energy))

    del kept


@given(shape())
@RUNS
def test_scoring_a_block_agrees_with_scoring_one_offset(lib, values):
    """The block is there for speed and must be there for nothing else."""
    filt, kept = designed(lib, values)

    reading = noise(READING)
    signal = ffitt.float_array(reading)

    block = ffitt.real_buffer(READING)
    assert lib.matched_score_block(filt, signal, READING, block)

    offsets = READING - len(values) + 1

    for index in range(offsets):
        apart = lib.matched_score_at(filt,
                                     ctypes.cast(
                                         ctypes.byref(signal,
                                                      index
                                                      * ctypes.sizeof(
                                                          ffitt.REAL_T)),
                                         ctypes.POINTER(ffitt.REAL_T)))

        assert abs(block[index] - apart) <= 1e-4 * (1.0 + abs(apart))

    del kept


@given(shape())
@RUNS
def test_the_best_is_the_largest_of_the_scores(lib, values):
    """matched_best is matched_score_block with the largest taken, thus the two
    must never disagree about which offset that is."""
    filt, kept = designed(lib, values)

    signal = ffitt.float_array(noise(READING))
    block = ffitt.real_buffer(READING)

    assert lib.matched_score_block(filt, signal, READING, block)

    where = ctypes.c_uint32(0)
    best = ffitt.real_buffer(1)

    assert lib.matched_best(filt, signal, READING, ctypes.byref(where), best)

    offsets = READING - len(values) + 1
    largest = max(block[index] for index in range(offsets))

    assert abs(best[0] - largest) <= 1e-5 * (1.0 + abs(largest))
    assert abs(block[where.value] - largest) <= 1e-5 * (1.0 + abs(largest))

    del kept


@given(st.floats(min_value=0.0009765625, max_value=0.25, width=32))
def test_a_smaller_rate_of_false_alarms_asks_for_a_higher_threshold(lib, rate):
    """The trade the threshold is. Anything else would mean asking to be wrong
    less often and being given a threshold that is wrong more often."""
    stricter = lib.matched_threshold_for(sp.to_float32(rate / 2.0), 1)
    looser = lib.matched_threshold_for(sp.to_float32(rate), 1)

    assert stricter > looser


@given(st.floats(min_value=0.0009765625, max_value=0.25, width=32),
       st.integers(min_value=1, max_value=100000))
def test_sharing_a_rate_among_offsets_is_asking_for_that_much_less_at_one(
        lib, rate, offsets):
    """WHAT THE OFFSET COUNT MEANS. Looking at more places is more chances to
    be wrong, thus the threshold must rise, and it must rise by exactly as much
    as asking for the shared rate at a single place."""
    many = lib.matched_threshold_for(sp.to_float32(rate), offsets)
    shared = lib.matched_threshold_for(sp.to_float32(rate / offsets), 1)

    assert abs(many - shared) <= 1e-3 * (1.0 + abs(shared))
    assert many >= lib.matched_threshold_for(sp.to_float32(rate), 1) - 1e-5


@given(st.floats(min_value=0.0009765625, max_value=0.25, width=32))
def test_the_threshold_agrees_with_the_share_of_a_normal_spread_above_it(
        lib, rate):
    """THE CLAIM THE THRESHOLD MAKES, held against the thing it claims about.
    The share of a normal spread standing above the threshold must be the rate
    that was asked for. The share is worked out here from the error function,
    which the module cannot use because it must need no other library, thus the
    two are worked out by different roads and compared."""
    threshold = lib.matched_threshold_for(sp.to_float32(rate), 1)

    above = 0.5 * math.erfc(threshold / math.sqrt(2.0))

    assert abs(above - rate) <= 1e-4 * (1.0 + rate)


@given(st.integers(min_value=0, max_value=100))
def test_only_a_shape_of_at_least_one_sample_is_taken(lib, length):
    assert lib.matched_is_valid_length(length) == (1 <= length <= 65536)


@given(st.integers(min_value=1, max_value=16))
def test_a_shape_that_holds_no_energy_is_refused(lib, length):
    """It would be found at every offset with the same strength, thus finding
    it would say nothing."""
    filt = lib.matched_make()
    empty = ffitt.float_array([0.0] * length)

    assert not lib.matched_design(filt, empty, length)
    assert not filt.designed


@given(shape(), st.integers(min_value=1, max_value=20))
@RUNS
def test_a_reading_shorter_than_the_shape_is_refused(lib, values, short):
    """There is no offset where the shape lies whole inside it, thus there is
    nothing to score and no answer to give."""
    filt, kept = designed(lib, values)

    count = max(1, len(values) - short)
    assume(count < len(values))

    signal = ffitt.float_array(noise(len(values)))
    block = ffitt.real_buffer(len(values))

    where = ctypes.c_uint32(7)
    best = ffitt.real_buffer(1)

    assert not lib.matched_score_block(filt, signal, count, block)
    assert not lib.matched_best(filt, signal, count, ctypes.byref(where), best)
    assert where.value == 7

    del kept
