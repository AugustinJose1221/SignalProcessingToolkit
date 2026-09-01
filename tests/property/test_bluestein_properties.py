"""Rules that a transform of any size must keep.

The bluestein module answers a question no other module in the library can, and
its input space is the widest of any of them: every size from 2 upwards, not
just the powers of two. These tests measure it against a transform written
plainly in Python, and against the fft module wherever both can be used.
"""

import cmath
import ctypes
import math
import os
import sys

from hypothesis import given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402

# Every size, and not only the comfortable ones. The awkward sizes are the
# reason the module exists: 60 is a period of the mains, 7 and 11 are primes,
# and 5 is odd.
ANY_SIZE = st.integers(min_value=2, max_value=40)
POWERS_OF_TWO = st.sampled_from([2, 4, 8, 16, 32])

TRANSFORM = settings(max_examples=30)


@st.composite
def signals(draw, size, magnitude=10.0):
    real = draw(st.lists(sp.elements(magnitude), min_size=size, max_size=size))
    imaginary = draw(st.lists(sp.elements(magnitude), min_size=size,
                              max_size=size))
    return list(zip(real, imaginary))


def to_c(values):
    array = (ffitt.Cnum * len(values))()
    for index, (re, im) in enumerate(values):
        array[index].re = re
        array[index].im = im
    return array


def from_c(array, size):
    return [complex(array[index].re, array[index].im) for index in range(size)]


def plain_transform(values):
    size = len(values)
    answer = []
    for k in range(size):
        total = 0j
        for n, (re, im) in enumerate(values):
            angle = -2.0 * math.pi * ((n * k) % size) / size
            total += complex(re, im) * cmath.exp(1j * angle)
        answer.append(total)
    return answer


def room_for(values):
    size = len(values)
    largest = max((abs(complex(re, im)) for re, im in values), default=1.0)
    return 1e-3 * size * max(largest, 1.0) * max(math.log2(size), 1.0)


@given(ANY_SIZE, st.data())
@TRANSFORM
def test_it_matches_a_transform_written_plainly_at_any_size(lib, size, data):
    """THE CROSS CHECK, at sizes no power of two can reach."""
    values = data.draw(signals(size))

    bluestein = lib.bluestein_alloc(size)
    assert bluestein.size == size

    array = to_c(values)
    lib.bluestein_forward(ctypes.byref(bluestein), array)

    truth = plain_transform(values)
    room = room_for(values)

    for got, wanted in zip(from_c(array, size), truth):
        assert abs(got - wanted) <= room

    lib.bluestein_free(ctypes.byref(bluestein))


@given(POWERS_OF_TWO, st.data())
@TRANSFORM
def test_where_the_fft_can_be_used_the_two_agree(lib, size, data):
    """Two ways to the same answer must not give two answers.

    The fft module is the one the library uses everywhere else, thus agreement
    with it is the strongest thing that can be said about this module.
    """
    values = data.draw(signals(size))

    fft = lib.fft_alloc(size)
    bluestein = lib.bluestein_alloc(size)

    by_fft = to_c(values)
    by_bluestein = to_c(values)

    lib.fft_forward(ctypes.byref(fft), by_fft)
    lib.bluestein_forward(ctypes.byref(bluestein), by_bluestein)

    room = room_for(values)

    for got, wanted in zip(from_c(by_bluestein, size), from_c(by_fft, size)):
        assert abs(got - wanted) <= room

    lib.fft_free(ctypes.byref(fft))
    lib.bluestein_free(ctypes.byref(bluestein))


@given(ANY_SIZE, st.data())
@TRANSFORM
def test_the_transform_and_its_opposite_give_the_signal_back(lib, size, data):
    """A round trip must return what it was given, at any size."""
    values = data.draw(signals(size))

    bluestein = lib.bluestein_alloc(size)
    array = to_c(values)

    lib.bluestein_forward(ctypes.byref(bluestein), array)
    lib.bluestein_inverse(ctypes.byref(bluestein), array)

    room = room_for(values)

    for index, (re, im) in enumerate(values):
        assert abs(complex(array[index].re, array[index].im)
                   - complex(re, im)) <= room

    lib.bluestein_free(ctypes.byref(bluestein))


@given(ANY_SIZE, st.integers(min_value=0, max_value=39), sp.elements(5.0))
@TRANSFORM
def test_a_single_wave_stands_in_a_single_bin_at_any_size(lib, size, which,
                                                          amplitude):
    """THE REASON THE MODULE EXISTS, held for every size.

    A size chosen so that a whole number of periods fits must put that
    frequency in one bin. If the turning factors were worked out carelessly,
    the tone would smear across the neighbours, and that is exactly what the
    fold of the square in the module prevents.
    """
    which = which % size
    values = []
    for index in range(size):
        angle = 2.0 * math.pi * ((index * which) % size) / size
        values.append((sp.to_float32(amplitude * math.cos(angle)),
                       sp.to_float32(amplitude * math.sin(angle))))

    bluestein = lib.bluestein_alloc(size)
    array = to_c(values)
    lib.bluestein_forward(ctypes.byref(bluestein), array)
    answer = from_c(array, size)

    room = room_for(values)

    assert abs(abs(answer[which]) - (abs(amplitude) * size)) <= room

    for index, value in enumerate(answer):
        if index != which:
            assert abs(value) <= room

    lib.bluestein_free(ctypes.byref(bluestein))


@given(ANY_SIZE, st.data())
@TRANSFORM
def test_the_energy_of_the_signal_is_the_energy_of_the_transform(lib, size,
                                                                  data):
    """The rule of Parseval, at any size."""
    values = data.draw(signals(size))

    bluestein = lib.bluestein_alloc(size)
    array = to_c(values)

    before = sum(abs(complex(re, im)) ** 2 for re, im in values)
    lib.bluestein_forward(ctypes.byref(bluestein), array)
    after = sum(abs(value) ** 2 for value in from_c(array, size))

    assert abs(after - (before * size)) <= 1e-2 * size * (1.0 + before)

    lib.bluestein_free(ctypes.byref(bluestein))


@given(st.integers(min_value=0, max_value=4096))
def test_the_transform_inside_holds_the_whole_convolution(lib, size):
    """The method is a convolution, and the room must be taken before it wraps.

    A transform works on a signal that repeats for ever. The convolution runs
    to twice the size less one, thus the transform inside must be at least that
    long and a power of two.
    """
    chosen = lib.bluestein_transform_size(size)

    if not lib.bluestein_is_valid_size(size):
        assert chosen == 0
        return

    wanted = (2 * size) - 1
    assert chosen >= wanted
    assert chosen & (chosen - 1) == 0
    assert (chosen // 2) < wanted


@given(st.integers(min_value=0, max_value=2000000))
def test_any_size_from_two_upwards_is_taken(lib, size):
    """A size of one has no frequency to speak of, and every other is served."""
    expected = 2 <= size <= (1 << 20)
    assert lib.bluestein_is_valid_size(size) == expected


@given(ANY_SIZE, st.integers(min_value=0, max_value=39),
       st.floats(min_value=1.0, max_value=48000.0, width=32))
def test_the_frequency_of_a_bin_mirrors_about_the_middle(lib, size, bin_index,
                                                          rate):
    """Bin k and bin size minus k stand at opposite frequencies."""
    bin_index = bin_index % size

    if (bin_index == 0) or ((size % 2 == 0) and (bin_index == size // 2)):
        return

    low = lib.bluestein_bin_frequency(bin_index, size, rate)
    high = lib.bluestein_bin_frequency(size - bin_index, size, rate)

    assert sp.close(low + high, 0.0, relative=1e-4,
                    absolute=1e-3 * max(rate, 1.0))
