"""Rules that taking a signal apart and putting it back must keep.

The guide of the module measures the rebuild for four windows and three hops.
These tests run every pairing that stft_can_rebuild accepts, and hold the
promise that the module makes: exact inside the solid stretch, nothing outside
it.
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

BLOCKS = st.sampled_from([8, 16, 32])
KINDS = st.sampled_from(ffitt.WINDOWS_WITHOUT_A_PARAMETER)

TRANSFORM = settings(max_examples=40)


@st.composite
def taken_apart(draw):
    """Give a block, a hop, a window and a signal long enough to put back.

    Long enough matters. A sample in the middle of a signal is under as many
    blocks as fit across it, thus the block divided by the hop is the fewest
    frames that leave any sample covered fully. Below that there is nothing to
    give back and the module rightly refuses; another test holds that.
    """
    block = draw(BLOCKS)
    hop = draw(st.sampled_from([block // 4, block // 2, (3 * block) // 4,
                                block]))
    assume(hop >= 1)
    kind = draw(KINDS)

    fewest = -(-block // hop)
    frames = draw(st.integers(min_value=fewest, max_value=fewest + 4))
    # The module must agree about how many that is.
    size = ((frames - 1) * hop) + block
    signal = draw(st.lists(sp.elements(20.0), min_size=size, max_size=size))
    return block, hop, kind, signal


def bins_of(block):
    return (block // 2) + 1


def designed(lib, block, hop, kind):
    stft = lib.stft_alloc(block)
    assert lib.stft_design(ctypes.byref(stft), hop, kind, 0.0)
    return stft


def forward(lib, stft, signal, block, hop):
    count = lib.stft_frame_count(len(signal), block, hop)
    frames = (ffitt.Cnum * (count * bins_of(block)))()
    assert lib.stft_forward(ctypes.byref(stft), ffitt.float_array(signal),
                            len(signal), frames, count * bins_of(block))
    return frames, count


@given(taken_apart())
@TRANSFORM
def test_a_signal_taken_apart_comes_back_inside_the_solid_stretch(lib,
                                                                  setup):
    """THE PROMISE OF THE MODULE, for every pairing it accepts.

    Wherever stft_can_rebuild says yes, the rebuild must be exact inside the
    stretch that stft_solid_range gives, to the rounding of the transform and
    no more.
    """
    block, hop, kind, signal = setup
    stft = designed(lib, block, hop, kind)
    assume(lib.stft_can_rebuild(ctypes.byref(stft)))

    frames, count = forward(lib, stft, signal, block, hop)
    size = lib.stft_signal_size(count, block, hop)

    output = ffitt.real_buffer(size)
    weight = ffitt.real_buffer(size)

    assert lib.stft_inverse(ctypes.byref(stft), frames, count, output, size,
                            weight)

    first = ctypes.c_uint32()
    solid = ctypes.c_uint32()
    assert lib.stft_solid_range(ctypes.byref(stft), count,
                                ctypes.byref(first), ctypes.byref(solid))

    room = 1e-3 * max(max(abs(v) for v in signal), 1.0) * max(
        math.log2(block), 1.0)

    for index in range(first.value, first.value + solid.value):
        assert abs(output[index] - signal[index]) <= room

    lib.stft_free(ctypes.byref(stft))


@given(taken_apart())
@TRANSFORM
def test_outside_the_solid_stretch_nothing_is_claimed(lib, setup):
    """The ends must be nothing, and not a number that looks like an answer.

    The first sample of a signal is under the first block alone. A window that
    is zero at its first sample has taken it away for good, and the module must
    say so by writing nothing rather than by writing what is left.
    """
    block, hop, kind, signal = setup
    stft = designed(lib, block, hop, kind)
    assume(lib.stft_can_rebuild(ctypes.byref(stft)))

    frames, count = forward(lib, stft, signal, block, hop)
    size = lib.stft_signal_size(count, block, hop)

    output = ffitt.real_buffer(size)
    weight = ffitt.real_buffer(size)
    lib.stft_inverse(ctypes.byref(stft), frames, count, output, size, weight)

    first = ctypes.c_uint32()
    solid = ctypes.c_uint32()
    lib.stft_solid_range(ctypes.byref(stft), count, ctypes.byref(first),
                         ctypes.byref(solid))

    for index in range(0, first.value):
        assert output[index] == 0.0

    for index in range(first.value + solid.value, size):
        assert output[index] == 0.0

    lib.stft_free(ctypes.byref(stft))


@given(taken_apart())
@TRANSFORM
def test_the_solid_stretch_lies_inside_the_answer(lib, setup):
    """A stretch that reached past the output would be read out of bounds."""
    block, hop, kind, signal = setup
    stft = designed(lib, block, hop, kind)

    count = lib.stft_frame_count(len(signal), block, hop)
    assume(count > 0)

    size = lib.stft_signal_size(count, block, hop)
    first = ctypes.c_uint32()
    solid = ctypes.c_uint32()

    if lib.stft_solid_range(ctypes.byref(stft), count, ctypes.byref(first),
                            ctypes.byref(solid)):
        assert first.value + solid.value <= size
        assert solid.value > 0

    lib.stft_free(ctypes.byref(stft))


@given(taken_apart())
@TRANSFORM
def test_a_rectangular_window_can_always_be_put_back(lib, setup):
    """It takes nothing away, thus there is nothing to fail to recover.

    Whatever the hop, a rectangular window must be accepted. If it were ever
    refused, the guard would be refusing something it need not.
    """
    block, hop, _, _ = setup
    stft = designed(lib, block, hop, ffitt.WINDOW_RECTANGULAR)

    assert lib.stft_can_rebuild(ctypes.byref(stft))

    lib.stft_free(ctypes.byref(stft))


@given(BLOCKS, KINDS)
def test_which_windows_can_be_put_back_at_a_hop_of_the_whole_block(lib, block,
                                                                   kind):
    """The guard must follow what the window really does at its ends.

    A hann window reaches exactly nothing at its first sample, thus at a hop of
    the whole block that sample is gone and the module must refuse.

    A HAMMING WINDOW IS THE EXCEPTION AND IT IS WORTH KNOWING. It stops at 0.08
    rather than at nothing, thus every sample still carries weight and the
    rebuild is possible, if noisy. The guard must not refuse it merely for
    being tapered.
    """
    stft = designed(lib, block, block, kind)
    can = lib.stft_can_rebuild(ctypes.byref(stft))

    if kind in (ffitt.WINDOW_RECTANGULAR, ffitt.WINDOW_HAMMING):
        assert can
    else:
        assert not can

    lib.stft_free(ctypes.byref(stft))


@given(BLOCKS)
def test_too_few_frames_to_cover_any_sample_are_refused(lib, block):
    """Nothing could be given back, thus nothing is.

    An answer here would be a buffer of zeros wearing the look of a signal,
    which is worse than a refusal.
    """
    hop = block // 4
    fewest = lib.stft_fewest_frames(block, hop)

    assert fewest == -(-block // hop)

    stft = designed(lib, block, hop, ffitt.WINDOW_HANN)
    first = ctypes.c_uint32()
    solid = ctypes.c_uint32()

    for count in range(1, fewest):
        assert not lib.stft_solid_range(ctypes.byref(stft), count,
                                        ctypes.byref(first),
                                        ctypes.byref(solid))

    assert lib.stft_solid_range(ctypes.byref(stft), fewest,
                                ctypes.byref(first), ctypes.byref(solid))

    lib.stft_free(ctypes.byref(stft))


@given(st.integers(min_value=0, max_value=200),
       st.integers(min_value=0, max_value=64),
       st.integers(min_value=0, max_value=64))
def test_the_frames_that_are_promised_all_fit_inside_the_signal(lib, size,
                                                                block, hop):
    """Only whole blocks are taken, thus the last one must not reach past the
    end of the signal."""
    count = lib.stft_frame_count(size, block, hop)

    if count == 0:
        return

    assert lib.stft_is_valid_hop(block, hop)
    # The last block starts one hop short of the count and must fit whole.
    assert ((count - 1) * hop) + block <= size
    # And one more frame would not have fitted.
    assert (count * hop) + block > size


@given(taken_apart())
@TRANSFORM
def test_two_signals_added_give_two_sets_of_frames_added(lib, setup):
    """The transform of each block is linear, thus the whole of it is."""
    block, hop, kind, signal = setup
    stft = designed(lib, block, hop, kind)

    other = [sp.to_float32(value * 0.5) for value in signal]
    both = [sp.to_float32(a + b) for a, b in zip(signal, other)]

    first, count = forward(lib, stft, signal, block, hop)
    second, _ = forward(lib, stft, other, block, hop)
    together, _ = forward(lib, stft, both, block, hop)

    room = 1e-3 * max(max(abs(v) for v in both), 1.0) * block

    for index in range(count * bins_of(block)):
        assert abs(together[index].re
                   - (first[index].re + second[index].re)) <= room
        assert abs(together[index].im
                   - (first[index].im + second[index].im)) <= room

    lib.stft_free(ctypes.byref(stft))


@given(BLOCKS, st.integers(min_value=0, max_value=8),
       st.floats(min_value=1.0, max_value=48000.0, width=32))
def test_the_time_of_a_frame_moves_by_one_hop_at_a_time(lib, block, frame,
                                                        rate):
    """Each frame starts one hop after the one before it."""
    hop = block // 2
    stft = designed(lib, block, hop, ffitt.WINDOW_HANN)

    first = lib.stft_frame_time(ctypes.byref(stft), frame, rate)
    second = lib.stft_frame_time(ctypes.byref(stft), frame + 1, rate)

    assert sp.close(second - first, hop / rate, relative=1e-3,
                    absolute=1e-3 / rate)

    lib.stft_free(ctypes.byref(stft))


@given(st.integers(min_value=0, max_value=200),
       st.integers(min_value=0, max_value=200))
def test_a_hop_must_be_from_one_sample_up_to_the_whole_block(lib, block, hop):
    """A hop of nothing would never move along; one longer than the block
    would step over samples and never look at them."""
    expected = lib.stft_is_valid_block(block) and (1 <= hop <= block)
    assert lib.stft_is_valid_hop(block, hop) == expected
