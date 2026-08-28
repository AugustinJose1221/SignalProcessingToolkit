"""Rules that the wavelet transform must keep.

A wavelet transform is worth having for two reasons, and both are properties of
the arithmetic rather than of the interface.

IT LOSES NOTHING. The approximation and the detail together hold as many values
as the signal, and putting them back gives the signal again. A transform that
lost anything could not be used to take noise out and put the signal back.

AND IT MOVES NO ENERGY ABOUT. Both wavelets here are orthogonal, thus the sum of
the squares of what comes out is the sum of the squares of what went in. That is
what lets a caller judge a value of the detail as large or small at all: without
it, a small value might be small because the transform shrank it.
"""

import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

RUNS = settings(max_examples=40, deadline=None)

WAVELETS = st.sampled_from(sptk.DWT_WAVELETS)

# Even sizes, which is what one level asks for.
SIZES = st.sampled_from([8, 16, 32, 64, 128])


@st.composite
def signal(draw, size=None):
    if size is None:
        size = draw(SIZES)

    return draw(st.lists(sp.elements(8.0), min_size=size, max_size=size))


def forward(lib, wavelet, values):
    size = len(values)
    half = size // 2

    dwt = lib.dwt_init(wavelet)
    approximation = sptk.real_buffer(half)
    detail = sptk.real_buffer(half)

    lib.dwt_forward(dwt, sptk.float_array(values), size, approximation,
                    detail)

    return ([approximation[index] for index in range(half)],
            [detail[index] for index in range(half)])


def inverse(lib, wavelet, approximation, detail):
    size = len(approximation) * 2
    dwt = lib.dwt_init(wavelet)
    out = sptk.real_buffer(size)

    lib.dwt_inverse(dwt, sptk.float_array(approximation),
                    sptk.float_array(detail), size, out)

    return [out[index] for index in range(size)]


def energy(values):
    return sum(value * value for value in values)


@given(WAVELETS, signal())
@RUNS
def test_taking_it_apart_and_putting_it_back_gives_the_signal(lib, wavelet,
                                                              values):
    """THE FIRST OF THE TWO REASONS TO HAVE IT. Every use of this module takes
    the transform, changes the detail, and takes the inverse. If the round trip
    lost anything, that loss would be in the answer of every one of them and
    nothing would say so."""
    approximation, detail = forward(lib, wavelet, values)
    back = inverse(lib, wavelet, approximation, detail)

    scale = 1.0 + max(abs(value) for value in values)

    for one, other in zip(values, back):
        assert abs(one - other) <= 1e-4 * scale


@given(WAVELETS, signal())
@RUNS
def test_it_moves_no_energy_about(lib, wavelet, values):
    """THE SECOND, AND THE ONE THAT MAKES A THRESHOLD MEAN ANYTHING. Both
    wavelets here are orthogonal, thus the sum of the squares of what comes out
    is the sum of the squares of what went in.

    Without it a caller could not judge a value of the detail as large or small:
    a small value might be small because the transform had shrunk it rather than
    because the signal is quiet there."""
    approximation, detail = forward(lib, wavelet, values)

    went_in = energy(values)
    came_out = energy(approximation) + energy(detail)

    assume(went_in > 0.01)

    assert abs(came_out - went_in) <= 1e-3 * went_in


@given(WAVELETS, SIZES, sp.elements(8.0))
@RUNS
def test_a_signal_that_does_not_change_leaves_the_detail_empty(lib, wavelet,
                                                               size, level):
    """WHAT THE DETAIL IS. It holds what CHANGES, thus a signal that does not
    change puts nothing in it. Both wavelets here answer to a level, which is
    the first thing a wavelet must do: a detail that held something for a flat
    signal would call a flat stretch an event."""
    values = [level] * size

    approximation, detail = forward(lib, wavelet, values)

    scale = 1.0 + abs(level)

    for value in detail:
        assert abs(value) <= 1e-3 * scale

    # And the approximation holds the whole of it.
    assert abs(energy(approximation) - energy(values)) <= 1e-3 * (
        1.0 + energy(values))


@given(SIZES, sp.elements(4.0), sp.elements(4.0))
@RUNS
def test_daubechies_leaves_a_straight_slope_out_of_the_detail_as_well(
        lib, size, level, slope):
    """WHAT PARTS THE TWO WAVELETS. Haar answers to a level and to nothing
    else; Daubechies of four answers to a level AND to a straight slope, which
    is what having two vanishing moments MEANS.

    A signal that climbs steadily is not an event, and a transform that put it
    into the detail would call every ramp one. This is the reason to pay for the
    longer wavelet."""
    assume(size >= 16)
    assume(abs(slope) > 0.1)

    values = [sp.to_float32(level + (slope * index)) for index in range(size)]

    _, haar_detail = forward(lib, sptk.DWT_HAAR, values)
    _, daub_detail = forward(lib, sptk.DWT_DAUBECHIES4, values)

    # Away from the two ends, where the block wraps round on itself and a ramp
    # meets its own beginning as a step.
    inside = slice(2, (size // 2) - 2)

    haar_inside = energy(haar_detail[inside])
    daub_inside = energy(daub_detail[inside])

    # Haar sees the slope: every pair of samples differs by it.
    assert haar_inside > 0.0

    # Daubechies of four does not.
    assert daub_inside < (1e-4 * haar_inside)


@given(WAVELETS, signal(), st.floats(min_value=0.25, max_value=8.0, width=32))
@RUNS
def test_turning_the_signal_up_turns_both_parts_up_by_as_much(lib, wavelet,
                                                              values, louder):
    """The transform is a pair of filters, thus it is linear. A caller
    thresholding a signal at some level relies on the level meaning the same
    thing whatever the signal is scaled to."""
    plain_approximation, plain_detail = forward(lib, wavelet, values)
    scaled_approximation, scaled_detail = forward(
        lib, wavelet, [sp.to_float32(value * louder) for value in values])

    for one, other in zip(plain_approximation, scaled_approximation):
        assert abs(other - (one * louder)) <= 1e-3 * (1.0 + abs(other))

    for one, other in zip(plain_detail, scaled_detail):
        assert abs(other - (one * louder)) <= 1e-3 * (1.0 + abs(other))


@given(WAVELETS, signal(), signal())
@RUNS
def test_the_transform_of_a_sum_is_the_sum_of_the_transforms(lib, wavelet,
                                                             first, second):
    """LINEARITY, held on the parts themselves rather than on their size."""
    size = min(len(first), len(second))
    first = first[:size]
    second = second[:size]
    both = [sp.to_float32(a + b) for a, b in zip(first, second)]

    together = forward(lib, wavelet, both)
    one = forward(lib, wavelet, first)
    other = forward(lib, wavelet, second)

    for part in (0, 1):
        for got, a, b in zip(together[part], one[part], other[part]):
            assert abs(got - (a + b)) <= 1e-3 * (1.0 + abs(a) + abs(b))


@given(WAVELETS, st.sampled_from([16, 32, 64, 128]),
       st.integers(min_value=1, max_value=3))
@RUNS
def test_several_levels_undo_as_cleanly_as_one(lib, wavelet, size, levels):
    """The transform is taken again on the approximation, and again on that.
    Every level is another chance to lose something, thus the round trip is
    held across as many as the size allows."""
    assume(size >= (1 << (levels + 2)))

    dwt = lib.dwt_init(wavelet)

    values = []
    state = 7

    for _ in range(size):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        values.append(sp.to_float32((((state >> 16) % 20000) / 10000.0) - 1.0))

    working = sptk.float_array(values)
    room = sptk.real_buffer(size)

    lib.dwt_forward_multi(dwt, working, size, levels, room)
    lib.dwt_inverse_multi(dwt, working, size, levels, room)

    scale = 1.0 + max(abs(value) for value in values)

    for index in range(size):
        assert abs(working[index] - values[index]) <= 1e-3 * scale


@given(signal(), st.floats(min_value=0.0625, max_value=4.0, width=32))
@RUNS
def test_the_threshold_takes_away_the_small_and_keeps_the_large(lib, values,
                                                                limit):
    """WHAT TAKING NOISE OUT IS. Noise spreads itself over every value of the
    detail and the signal holds few large ones, thus setting the small ones to
    nothing takes away much of the noise and keeps the signal.

    What must hold is that it takes away everything below the limit and leaves
    everything else exactly where it was. A threshold that moved the large
    values as well would round the edges the transform was chosen to keep."""
    data = sptk.float_array(values)
    size = len(values)

    lib.dwt_threshold(data, size, sp.to_float32(limit))

    for index, was in enumerate(values):
        if abs(was) < limit:
            assert data[index] == 0.0
        else:
            assert data[index] == was


@given(st.integers(min_value=0, max_value=256),
       st.integers(min_value=1, max_value=5))
def test_only_a_size_that_halves_as_often_as_there_are_levels_is_taken(
        lib, size, levels):
    """Each level halves the signal, thus a size that cannot be halved that
    many times has a level with nothing to work on."""
    fits = size > 0

    left = size
    for _ in range(levels):
        if (left % 2) != 0 or left == 0:
            fits = False
            break
        left //= 2

    assert lib.dwt_is_valid_size(size, levels) == fits
