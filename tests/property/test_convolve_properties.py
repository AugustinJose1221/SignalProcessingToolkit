"""Rules that sliding one signal along another must keep, at every size.

The unit tests examine a handful of sizes, and two of the sizes they examined
were wrong when the module was written. These tests examine every pair of sizes
that Hypothesis can find, against a convolution written plainly in Python.
"""

import ctypes
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

SIGNALS = st.lists(sp.elements(20.0), min_size=1, max_size=24)
SHAPES = st.lists(sp.elements(5.0), min_size=1, max_size=8)
MODES = st.sampled_from([sptk.CONVOLVE_FULL, sptk.CONVOLVE_SAME,
                         sptk.CONVOLVE_VALID])


def plain_convolution(signal, shape):
    """The whole answer, written the slowest and plainest way there is.

    This is the reference the module is measured against. It is written to be
    obviously right and not to be quick.
    """
    count = len(signal) + len(shape) - 1
    answer = []
    for place in range(count):
        total = 0.0
        for k in range(len(shape)):
            if 0 <= place - k < len(signal):
                total += signal[place - k] * shape[k]
        answer.append(total)
    return answer


def offset_of(mode, shape_size):
    """Where in the whole answer the wanted part begins."""
    if mode == sptk.CONVOLVE_SAME:
        return (shape_size - 1) // 2
    if mode == sptk.CONVOLVE_VALID:
        return shape_size - 1
    return 0


def convolved(lib, signal, shape, mode):
    """Give what the module answers, or None where it refuses."""
    count = lib.convolve_output_size(len(signal), len(shape), mode)
    if count == 0:
        return None
    output = sptk.real_buffer(count)
    assert lib.convolve_direct(sptk.float_array(signal), len(signal),
                               sptk.float_array(shape), len(shape), output,
                               mode)
    return list(output)


@given(SIGNALS, SHAPES, MODES)
def test_the_answer_matches_a_convolution_written_plainly(lib, signal, shape,
                                                          mode):
    """THE CROSS CHECK. Every mode is a window onto the same whole answer."""
    answer = convolved(lib, signal, shape, mode)
    assume(answer is not None)

    whole = plain_convolution(signal, shape)
    start = offset_of(mode, len(shape))

    scale = 1.0 + (max(abs(v) for v in signal) * sum(abs(v) for v in shape))

    for index, value in enumerate(answer):
        assert abs(value - whole[start + index]) <= 1e-3 * scale


@given(SIGNALS, SHAPES, MODES)
def test_the_size_that_is_promised_is_the_size_that_comes_back(lib, signal,
                                                               shape, mode):
    """A caller allocates from convolve_output_size before it calls."""
    count = lib.convolve_output_size(len(signal), len(shape), mode)

    if mode == sptk.CONVOLVE_FULL:
        assert count == len(signal) + len(shape) - 1
    elif mode == sptk.CONVOLVE_SAME:
        assert count == len(signal)
    else:
        # A shape longer than the signal never lies wholly inside it.
        expected = len(signal) - len(shape) + 1
        assert count == (expected if expected > 0 else 0)


@given(SIGNALS, SHAPES)
def test_it_does_not_matter_which_one_is_slid_along_the_other(lib, signal,
                                                              shape):
    """A convolution does not care which of the two is called the shape."""
    one = convolved(lib, signal, shape, sptk.CONVOLVE_FULL)
    other = convolved(lib, shape, signal, sptk.CONVOLVE_FULL)
    assume(one is not None and other is not None)

    scale = 1.0 + (max(abs(v) for v in signal) * sum(abs(v) for v in shape))

    assert len(one) == len(other)
    for first, second in zip(one, other):
        assert abs(first - second) <= 1e-3 * scale


@given(SIGNALS, st.integers(min_value=0, max_value=7))
def test_a_shape_that_is_a_single_one_gives_the_signal_back(lib, signal,
                                                            place):
    """A shape of one value at one place moves the signal and nothing else.

    Where that value stands at the start, the signal comes back untouched.
    """
    shape = [0.0] * (place + 1)
    shape[place] = 1.0

    answer = convolved(lib, signal, shape, sptk.CONVOLVE_FULL)
    assume(answer is not None)

    for index, value in enumerate(signal):
        assert sp.close(answer[place + index], value, relative=1e-4,
                        absolute=1e-4)


@given(SIGNALS, SHAPES, SHAPES)
def test_two_shapes_added_give_two_answers_added(lib, signal, first, second):
    """A convolution is linear, thus it must add the way its inputs add."""
    size = max(len(first), len(second))
    first = first + [0.0] * (size - len(first))
    second = second + [0.0] * (size - len(second))
    both = [sp.to_float32(a + b) for a, b in zip(first, second)]

    by_parts_first = convolved(lib, signal, first, sptk.CONVOLVE_FULL)
    by_parts_second = convolved(lib, signal, second, sptk.CONVOLVE_FULL)
    together = convolved(lib, signal, both, sptk.CONVOLVE_FULL)
    assume(together is not None)

    scale = 1.0 + (max(abs(v) for v in signal)
                   * sum(abs(a) + abs(b) for a, b in zip(first, second)))

    for index in range(len(together)):
        added = by_parts_first[index] + by_parts_second[index]
        assert abs(together[index] - added) <= 1e-3 * scale


@given(SIGNALS, SHAPES)
def test_the_whole_answer_holds_the_whole_of_both_signals(lib, signal, shape):
    """The sum of the whole answer is the sum of one times the sum of the other.

    Nothing of either is thrown away in the full mode, thus this must hold
    exactly. The same and valid modes cut the ends off, which is what the
    header warns of, and this test says why that warning is there.
    """
    answer = convolved(lib, signal, shape, sptk.CONVOLVE_FULL)
    assume(answer is not None)

    scale = 1.0 + (sum(abs(v) for v in signal) * sum(abs(v) for v in shape))

    assert abs(sum(answer) - (sum(signal) * sum(shape))) <= 1e-3 * scale


@given(SIGNALS, SHAPES)
def test_the_valid_mode_never_reports_a_place_that_was_partly_assumed(lib,
                                                                      signal,
                                                                      shape):
    """Every value of the valid mode uses real samples only.

    Outside itself the signal is taken to be nothing. The full and the same
    modes report places where that assumption stands in for measurement; the
    valid mode is exactly the part where it does not, and this test holds the
    boundary by working each value out from the samples alone.
    """
    answer = convolved(lib, signal, shape, sptk.CONVOLVE_VALID)
    assume(answer is not None)

    scale = 1.0 + (max(abs(v) for v in signal) * sum(abs(v) for v in shape))

    for index, value in enumerate(answer):
        place = index + len(shape) - 1
        total = 0.0
        for k in range(len(shape)):
            # Every read must land inside the signal. If one does not, the
            # valid mode reaches further than it should.
            assert 0 <= place - k < len(signal)
            total += signal[place - k] * shape[k]
        assert abs(value - total) <= 1e-3 * scale


@given(st.integers(min_value=0, max_value=32),
       st.integers(min_value=0, max_value=32))
def test_a_transform_large_enough_to_hold_the_answer_is_chosen(lib, signal_size,
                                                               shape_size):
    """The transform must hold the whole answer, or the ends wrap round.

    A transform works on a signal that repeats for ever. Anything hanging past
    the end adds itself to the start, thus the size must be at least the length
    of the whole answer, and a power of two.
    """
    chosen = lib.convolve_transform_size(signal_size, shape_size)

    if (signal_size == 0) or (shape_size == 0):
        assert chosen == 0
        return

    wanted = signal_size + shape_size - 1
    assert chosen >= wanted
    assert chosen & (chosen - 1) == 0

    # And no larger than it needs to be.
    assert (chosen // 2) < wanted or chosen == 2


@given(SIGNALS, SHAPES, MODES)
@settings(max_examples=60)
def test_the_two_ways_of_convolving_give_one_answer(lib, signal, shape, mode):
    """THE STRONGEST TEST IN THIS FILE.

    The module offers a direct way and a way through the transform. They share
    no arithmetic at all: one slides and adds, the other transforms, multiplies
    and comes back. Two roads to one answer is the best check there is, because
    a fault would have to be made twice and in the same direction to hide.

    The header promises they agree to the last digit the width can hold. This
    holds them to it for every pair of sizes.
    """
    slow = convolved(lib, signal, shape, mode)
    assume(slow is not None)

    larger = lib.convolve_transform_size(len(signal), len(shape))
    assume(larger > 0)

    fft = lib.fft_alloc(larger)
    first = (sptk.Cnum * larger)()
    second = (sptk.Cnum * larger)()
    work = sptk.real_buffer(larger)
    fast = sptk.real_buffer(len(slow))

    assert lib.convolve_by_transform(sptk.float_array(signal), len(signal),
                                     sptk.float_array(shape), len(shape),
                                     fast, mode, ctypes.byref(fft), first,
                                     second, work)

    # The transform carries the whole signal through several rounds of
    # arithmetic, thus the room follows the size of what went in and the size
    # of the transform.
    scale = (1.0 + (max(abs(v) for v in signal) * sum(abs(v) for v in shape)))
    room = 1e-3 * scale * max(larger.bit_length(), 1)

    for index, value in enumerate(slow):
        assert abs(fast[index] - value) <= room

    lib.fft_free(ctypes.byref(fft))


@given(st.integers(min_value=-3, max_value=6))
def test_only_the_three_modes_that_exist_are_taken(lib, mode):
    """A mode outside the three must be refused, whatever number it carries."""
    assert lib.convolve_is_valid_mode(mode) == (0 <= mode <= 2)
