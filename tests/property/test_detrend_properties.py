"""Rules that taking a trend out of a block must keep, for every block."""

import os
import sys

from hypothesis import assume, given
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

BLOCKS = st.lists(sp.elements(50.0), min_size=2, max_size=64)


def detrended(lib, block, kind):
    """Give the block with its trend taken out."""
    size = len(block)
    output = sptk.real_buffer(size)
    assert lib.detrend_block(sptk.float_array(block), output, size, kind)
    return list(output)


@given(BLOCKS)
def test_a_block_with_the_level_out_has_no_level_left(lib, block):
    """What is left must add up to nothing."""
    answer = detrended(lib, block, sptk.DETREND_CONSTANT)
    total = sum(answer)
    assert abs(total) <= 1e-3 * (1.0 + sum(abs(value) for value in block))


@given(BLOCKS)
def test_a_block_with_the_drift_out_has_no_level_and_no_drift_left(lib, block):
    """A straight line taken out must leave nothing for a second one to find.

    The line of least squared error is the one that leaves no level and no
    slope behind. If either is left, the line that was taken was not that one.
    """
    answer = detrended(lib, block, sptk.DETREND_LINEAR)
    size = len(answer)
    scale = 1.0 + sum(abs(value) for value in block)

    assert abs(sum(answer)) <= 1e-3 * scale

    # The slope is the answer weighed by how far each sample sits from the
    # middle. Numbering from the middle is what the module does, and it is what
    # makes this sum the slope.
    middle = (size - 1) / 2.0
    tilt = sum((index - middle) * value for index, value in enumerate(answer))
    assert abs(tilt) <= 1e-3 * scale * size


@given(BLOCKS, st.sampled_from([sptk.DETREND_CONSTANT, sptk.DETREND_LINEAR]))
def test_taking_a_trend_out_twice_is_the_same_as_taking_it_out_once(lib, block,
                                                                   kind):
    """There is nothing left to take, thus the second pass must change nothing.

    A module that took a little more each time would quietly eat the signal of
    a caller that ran it in a loop.
    """
    once = detrended(lib, block, kind)
    twice = detrended(lib, once, kind)

    scale = 1.0 + max(abs(value) for value in block)

    for first, second in zip(once, twice):
        assert abs(first - second) <= 1e-4 * scale


@given(BLOCKS)
def test_the_trend_that_is_reported_is_the_trend_that_is_taken(lib, block):
    """What detrend_trend gives must be exactly what detrend_block removes."""
    size = len(block)
    offset = sptk.real_buffer(1)
    slope = sptk.real_buffer(1)

    assert lib.detrend_trend(sptk.float_array(block), size,
                             sptk.DETREND_LINEAR, offset, slope)

    answer = detrended(lib, block, sptk.DETREND_LINEAR)
    scale = 1.0 + max(abs(value) for value in block)

    for index in range(size):
        trend = lib.detrend_trend_at(offset[0], slope[0], size, index)
        assert abs((block[index] - trend) - answer[index]) <= 1e-3 * scale


@given(BLOCKS,
       st.floats(min_value=-20.0, max_value=20.0, width=32),
       st.floats(min_value=-2.0, max_value=2.0, width=32))
def test_adding_a_trend_to_a_block_does_not_change_what_is_left(lib, block,
                                                               offset, slope):
    """THE PROPERTY THE MODULE EXISTS FOR.

    Putting a level and a drift onto a block and then taking the trend out must
    give back what taking the trend out of the plain block gives. If it does
    not, the module is removing something of the signal along with the drift.
    """
    size = len(block)
    middle = (size - 1) / 2.0
    tilted = [sp.to_float32(value + offset + (slope * (index - middle)))
              for index, value in enumerate(block)]

    plain = detrended(lib, block, sptk.DETREND_LINEAR)
    after = detrended(lib, tilted, sptk.DETREND_LINEAR)

    scale = 1.0 + max(abs(value) for value in tilted)

    for first, second in zip(plain, after):
        assert abs(first - second) <= 1e-3 * scale


@given(BLOCKS)
def test_removing_a_known_trend_and_putting_it_back_gives_the_block(lib, block):
    """detrend_remove must be exactly reversible, since it only subtracts."""
    size = len(block)
    offset, slope = 3.5, 0.25
    taken = sptk.real_buffer(size)

    assert lib.detrend_remove(sptk.float_array(block), taken, size, offset,
                              slope)

    for index in range(size):
        trend = lib.detrend_trend_at(offset, slope, size, index)
        assert sp.close(taken[index] + trend, block[index],
                        relative=1e-3, absolute=1e-3)


@given(BLOCKS)
def test_a_block_of_one_has_a_level_and_no_direction(lib, block):
    """One sample cannot have a slope, and the module must say so."""
    one = sptk.float_array([block[0]])
    output = sptk.real_buffer(1)

    assert lib.detrend_block(one, output, 1, sptk.DETREND_CONSTANT)
    assert not lib.detrend_block(one, output, 1, sptk.DETREND_LINEAR)


@given(st.integers(min_value=-5, max_value=8))
def test_only_the_two_kinds_that_exist_are_taken(lib, kind):
    """A kind outside the two must be refused, whatever number it carries."""
    expected = kind in (sptk.DETREND_CONSTANT, sptk.DETREND_LINEAR)
    assert lib.detrend_is_valid_kind(kind) == expected
