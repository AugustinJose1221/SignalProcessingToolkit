"""Rules that the cross spectrum must keep.

csd answers three questions about a pair of signals: what they share at each
frequency, how much of one explains the other, and what turns one into the
other. The second of those -- the coherence -- is a number the survey example
and every decision about whether a canceller is worth writing rest on, thus
what must hold of it matters beyond this module.
"""

import cmath
import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

RUNS = settings(max_examples=25, deadline=None)

# Blocks the transform can use. Small ones keep the runs short.
BLOCKS = st.sampled_from([32, 64, 128])

WINDOWS = st.sampled_from(sptk.WINDOWS_WITHOUT_A_PARAMETER)

SAMPLE_RATE = 8192.0


def noise(count, seed=1):
    state = seed
    out = []

    for _ in range(count):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        out.append(sp.to_float32((((state >> 16) % 20000) / 10000.0) - 1.0))

    return out


def designed(lib, block, window, overlap=None):
    csd = lib.csd_alloc(block)

    if overlap is None:
        overlap = block // 2

    assert lib.csd_design(csd, overlap, window, sp.to_float32(0.0))

    return csd


def coherence_of(lib, csd, first, second, size, bins):
    out = sptk.real_buffer(bins)

    assert lib.csd_coherence(csd, sptk.float_array(first),
                             sptk.float_array(second), size, out)

    return [out[index] for index in range(bins)]


def bins_of(block):
    return (block // 2) + 1


@given(BLOCKS, WINDOWS)
@RUNS
def test_a_signal_is_wholly_coherent_with_itself(lib, block, window):
    """THE RULE THE WHOLE MEASUREMENT RESTS ON. Coherence says how much of one
    signal is explained by the other. A signal explains itself completely, thus
    every bin must come back at one. Anything less would mean the arithmetic
    loses coherence that is really there, and a caller reading 0.8 could not
    tell that from a pair that really shares four fifths."""
    size = block * 12
    values = noise(size)

    csd = designed(lib, block, window)

    try:
        found = coherence_of(lib, csd, values, values, size, bins_of(block))

        for value in found:
            assert math.isfinite(value)
            assert abs(value - 1.0) <= 1e-3
    finally:
        lib.csd_free(csd)


@given(BLOCKS, WINDOWS)
@RUNS
def test_coherence_never_leaves_the_range_of_one(lib, block, window):
    """It is a share, thus it stands between nothing and one whatever it is
    given. A caller deciding whether a canceller is worth writing judges it
    against a fixed number, and a share above one would be a share of more than
    everything."""
    size = block * 12

    first = noise(size, seed=3)
    second = noise(size, seed=99)

    csd = designed(lib, block, window)

    try:
        found = coherence_of(lib, csd, first, second, size, bins_of(block))

        for value in found:
            assert math.isfinite(value)
            assert -1e-4 <= value <= 1.0 + 1e-4
    finally:
        lib.csd_free(csd)


@given(BLOCKS, WINDOWS, st.floats(min_value=0.25, max_value=8.0, width=32))
@RUNS
def test_coherence_does_not_move_when_a_signal_is_scaled(lib, block, window,
                                                         louder):
    """THE CLAIM THAT MAKES COHERENCE A SHARE. Turning one signal up does not
    change how much of it the other explains. A coherence that moved with the
    loudness could not be judged against a fixed number at all."""
    size = block * 12

    first = noise(size, seed=5)
    second = [sp.to_float32((0.5 * first[index]) + (0.5 * value))
              for index, value in enumerate(noise(size, seed=77))]

    louder_second = [sp.to_float32(value * louder) for value in second]

    csd = designed(lib, block, window)

    try:
        plain = coherence_of(lib, csd, first, second, size, bins_of(block))
        scaled = coherence_of(lib, csd, first, louder_second, size,
                              bins_of(block))

        for one, other in zip(plain, scaled):
            assert abs(one - other) <= 1e-3
    finally:
        lib.csd_free(csd)


@given(BLOCKS, WINDOWS)
@RUNS
def test_two_signals_with_nothing_in_common_are_barely_coherent(lib, block,
                                                                window):
    """The other end of the same claim. Two runs of noise share nothing, thus
    the coherence must be small. IT IS NOT NOTHING, and that is worth knowing:
    a measurement made from a handful of blocks reads a coherence of about one
    divided by the number of blocks even where there is nothing at all to
    share. That floor is why csd.h asks for at least eight blocks."""
    blocks = 24
    size = block * blocks

    first = noise(size, seed=3)
    second = noise(size, seed=8191)

    csd = designed(lib, block, window)

    try:
        found = coherence_of(lib, csd, first, second, size, bins_of(block))

        # The floor is about one divided by the number of blocks, and the mean
        # across the bins must sit near it rather than near one.
        mean = sum(found) / len(found)

        assert mean < 0.4
    finally:
        lib.csd_free(csd)


@given(BLOCKS, WINDOWS)
@RUNS
def test_more_blocks_bring_an_unrelated_pair_closer_to_nothing(lib, block,
                                                               window):
    """WHY csd.h REFUSES TO ANSWER FROM TOO FEW BLOCKS. The coherence of a pair
    that shares nothing falls as the blocks are added up, thus a reading taken
    from few blocks is mostly the arithmetic looking at itself."""
    few = block * 8
    many = block * 48

    first = noise(many, seed=3)
    second = noise(many, seed=8191)

    csd = designed(lib, block, window)

    try:
        from_few = coherence_of(lib, csd, first[:few], second[:few], few,
                                bins_of(block))
        from_many = coherence_of(lib, csd, first, second, many,
                                 bins_of(block))

        assert (sum(from_many) / len(from_many)) < (sum(from_few)
                                                    / len(from_few))
    finally:
        lib.csd_free(csd)


@given(BLOCKS, WINDOWS)
@RUNS
def test_what_two_signals_share_is_the_same_either_way_round(lib, block,
                                                             window):
    """The cross spectrum of one pair given the other way round is its mirror,
    because what they share does not depend on which was named first: only the
    sign of the phase turns. The coherence, which throws the phase away, must
    not move at all."""
    size = block * 12

    first = noise(size, seed=5)
    second = noise(size, seed=6)

    csd = designed(lib, block, window)
    bins = bins_of(block)

    try:
        one_way = coherence_of(lib, csd, first, second, size, bins)
        other_way = coherence_of(lib, csd, second, first, size, bins)

        for one, other in zip(one_way, other_way):
            assert abs(one - other) <= 1e-4

        # And the cross spectrum turns its phase round and keeps its size.
        forward = (sptk.Cnum * bins)()
        backward = (sptk.Cnum * bins)()

        assert lib.csd_estimate(csd, sptk.float_array(first),
                                sptk.float_array(second), size,
                                sp.to_float32(SAMPLE_RATE), forward)
        assert lib.csd_estimate(csd, sptk.float_array(second),
                                sptk.float_array(first), size,
                                sp.to_float32(SAMPLE_RATE), backward)

        for index in range(bins):
            scale = 1e-4 * (1.0 + abs(forward[index].re)
                            + abs(forward[index].im))

            assert abs(forward[index].re - backward[index].re) <= scale
            assert abs(forward[index].im + backward[index].im) <= scale
    finally:
        lib.csd_free(csd)


@given(BLOCKS, WINDOWS, st.floats(min_value=0.25, max_value=4.0, width=32))
@RUNS
def test_the_transfer_of_a_signal_scaled_is_that_scale(lib, block, window,
                                                       louder):
    """THE REASON csd_transfer EXISTS. Given a signal and that same signal made
    louder, what turns one into the other is exactly how much louder it was
    made, at every frequency. A transfer that said anything else could not be
    used to describe a path."""
    size = block * 16

    first = noise(size, seed=13)
    second = [sp.to_float32(value * louder) for value in first]

    csd = designed(lib, block, window)
    bins = bins_of(block)
    out = (sptk.Cnum * bins)()

    try:
        assert lib.csd_transfer(csd, sptk.float_array(first),
                                sptk.float_array(second), size, out)

        # Bin 0 holds the level and the last bin the highest frequency; both
        # are read from fewer numbers than the rest and are left out.
        for index in range(1, bins - 1):
            assert abs(out[index].re - louder) <= 1e-2 * louder
            assert abs(out[index].im) <= 1e-2 * louder
    finally:
        lib.csd_free(csd)


@given(BLOCKS, WINDOWS)
@RUNS
def test_a_reading_shorter_than_a_block_is_refused(lib, block, window):
    """There is nothing to transform, thus there is no answer to give."""
    csd = designed(lib, block, window)
    bins = bins_of(block)

    values = noise(block)
    out = sptk.real_buffer(bins)

    try:
        assert not lib.csd_coherence(csd, sptk.float_array(values),
                                     sptk.float_array(values), block // 2,
                                     out)
    finally:
        lib.csd_free(csd)


@given(st.integers(min_value=0, max_value=2048))
def test_only_a_block_the_transform_can_use_is_taken(lib, block):
    """A block goes straight to the transform, thus what this takes is exactly
    what the transform takes AND NOT A BOUND OF ITS OWN. Written as a bound of
    its own it would have to be kept in step with fft by hand: this test was
    first written asking for a power of two of at least four, and the transform
    takes two."""
    assert lib.csd_is_valid_block(block) == lib.fft_is_valid_size(block)


@given(BLOCKS, WINDOWS, st.integers(min_value=1, max_value=64))
@RUNS
def test_the_count_of_blocks_follows_the_overlap(lib, block, window, blocks):
    """How many blocks a reading holds is what every answer here is averaged
    over, thus a caller judging whether it has enough must be told the truth
    about it."""
    csd = designed(lib, block, window, overlap=block // 2)

    try:
        size = block * blocks
        counted = lib.csd_block_count(csd, size)

        # Blocks half overlapped, thus about twice as many as would fit end to
        # end, less the one that would run off the end.
        assert counted == max(0, ((size - block) // (block // 2)) + 1)
    finally:
        lib.csd_free(csd)
