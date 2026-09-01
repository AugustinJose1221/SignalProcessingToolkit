"""Rules that the transform must keep, for every signal.

The fft module is what eight other modules stand on, thus a fault here is a
fault everywhere. These tests measure it against a transform written plainly in
Python, and hold the rules that any transform must keep.
"""

import cmath
import ctypes
import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402

# Sizes are kept small. A transform of 1024 through ctypes costs far more than
# the rule it examines is worth, and every rule here holds at 8 as it holds at
# 8192.
SIZES = st.sampled_from([2, 4, 8, 16, 32])

# A transform costs more than most calls, thus fewer examples than the default.
TRANSFORM = settings(max_examples=40)


@st.composite
def signals(draw, size=None, magnitude=10.0):
    """Give a list of complex values as pairs of real numbers."""
    if size is None:
        size = draw(SIZES)
    real = draw(st.lists(sp.elements(magnitude), min_size=size,
                         max_size=size))
    imaginary = draw(st.lists(sp.elements(magnitude), min_size=size,
                              max_size=size))
    return list(zip(real, imaginary))


def to_c(values):
    """Give a C array of complex numbers holding these pairs."""
    array = (ffitt.Cnum * len(values))()
    for index, (re, im) in enumerate(values):
        array[index].re = re
        array[index].im = im
    return array


def from_c(array, size):
    return [complex(array[index].re, array[index].im) for index in range(size)]


def plain_transform(values):
    """The transform written the slowest and plainest way there is."""
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
    """How far a transform of these values may be out.

    The error of a transform grows with the size of what goes into it and with
    the logarithm of the size, thus the room follows both.
    """
    size = len(values)
    largest = max((abs(complex(re, im)) for re, im in values), default=1.0)
    return 1e-4 * size * max(largest, 1.0) * max(math.log2(size), 1.0)


@given(signals())
@TRANSFORM
def test_the_transform_matches_one_written_plainly(lib, values):
    """THE CROSS CHECK, against an answer that is obviously right."""
    size = len(values)
    fft = lib.fft_alloc(size)
    data = to_c(values)

    lib.fft_forward(ctypes.byref(fft), data)
    answer = from_c(data, size)
    truth = plain_transform(values)
    room = room_for(values)

    for got, wanted in zip(answer, truth):
        assert abs(got - wanted) <= room

    lib.fft_free(ctypes.byref(fft))


@given(signals())
@TRANSFORM
def test_the_transform_and_its_opposite_give_the_signal_back(lib, values):
    """A round trip must return what it was given."""
    size = len(values)
    fft = lib.fft_alloc(size)
    data = to_c(values)

    lib.fft_forward(ctypes.byref(fft), data)
    lib.fft_inverse(ctypes.byref(fft), data)

    room = room_for(values)

    for index, (re, im) in enumerate(values):
        assert abs(complex(data[index].re, data[index].im)
                   - complex(re, im)) <= room

    lib.fft_free(ctypes.byref(fft))


@given(signals())
@TRANSFORM
def test_the_energy_of_the_signal_is_the_energy_of_the_transform(lib, values):
    """The rule of Parseval. Nothing is created and nothing is lost.

    The sum of the squares of the bins is the sum of the squares of the samples
    multiplied by the size. A transform that failed this would be adding energy
    to the signal or taking it away.
    """
    size = len(values)
    fft = lib.fft_alloc(size)
    data = to_c(values)

    before = sum(abs(complex(re, im)) ** 2 for re, im in values)
    lib.fft_forward(ctypes.byref(fft), data)
    after = sum(abs(value) ** 2 for value in from_c(data, size))

    assert abs(after - (before * size)) <= 1e-3 * size * (1.0 + before)

    lib.fft_free(ctypes.byref(fft))


@given(SIZES, signals(), signals())
@TRANSFORM
def test_two_signals_added_give_two_transforms_added(lib, size, first, second):
    """The transform is linear, thus it must add the way its input adds."""
    first = first[:size] + [(0.0, 0.0)] * max(0, size - len(first))
    second = second[:size] + [(0.0, 0.0)] * max(0, size - len(second))
    both = [(sp.to_float32(a[0] + b[0]), sp.to_float32(a[1] + b[1]))
            for a, b in zip(first, second)]

    fft = lib.fft_alloc(size)

    def transformed(values):
        data = to_c(values)
        lib.fft_forward(ctypes.byref(fft), data)
        return from_c(data, size)

    apart = [a + b for a, b in zip(transformed(first), transformed(second))]
    together = transformed(both)

    room = room_for(first) + room_for(second)

    for got, wanted in zip(together, apart):
        assert abs(got - wanted) <= room

    lib.fft_free(ctypes.byref(fft))


@given(SIZES, st.integers(min_value=0, max_value=31),
       sp.elements(5.0))
@TRANSFORM
def test_a_single_wave_stands_in_a_single_bin(lib, size, which, amplitude):
    """A wave of a whole number of periods belongs to one bin and no other.

    This is the rule that makes a transform useful at all: a signal made of one
    frequency must be reported as one frequency.
    """
    which = which % size
    values = []
    for index in range(size):
        angle = 2.0 * math.pi * ((index * which) % size) / size
        values.append((sp.to_float32(amplitude * math.cos(angle)),
                       sp.to_float32(amplitude * math.sin(angle))))

    fft = lib.fft_alloc(size)
    data = to_c(values)
    lib.fft_forward(ctypes.byref(fft), data)
    answer = from_c(data, size)

    room = room_for(values)

    assert abs(abs(answer[which]) - (abs(amplitude) * size)) <= room

    for index, value in enumerate(answer):
        if index != which:
            assert abs(value) <= room

    lib.fft_free(ctypes.byref(fft))


@given(signals(magnitude=10.0))
@TRANSFORM
def test_a_signal_of_real_values_gives_a_spectrum_that_mirrors(lib, values):
    """The upper half of the answer is the lower half turned the other way.

    This is why only half the bins and one more are kept anywhere in the
    library. If it did not hold, the modules that keep half a spectrum would be
    throwing away something real.
    """
    size = len(values)
    real_only = [value for value, _ in values]

    fft = lib.fft_alloc(size)
    spectrum = (ffitt.Cnum * size)()
    lib.fft_forward_real(ctypes.byref(fft), ffitt.float_array(real_only),
                         spectrum)
    answer = from_c(spectrum, size)

    room = room_for([(value, 0.0) for value in real_only])

    for index in range(1, size // 2):
        assert abs(answer[size - index] - answer[index].conjugate()) <= room

    lib.fft_free(ctypes.byref(fft))


@given(signals(magnitude=10.0))
@TRANSFORM
def test_a_real_signal_comes_back_from_half_a_spectrum(lib, values):
    """fft_inverse_real must rebuild the mirrored half without being told it."""
    size = len(values)
    real_only = [value for value, _ in values]

    fft = lib.fft_alloc(size)
    spectrum = (ffitt.Cnum * size)()
    work = (ffitt.Cnum * size)()
    output = ffitt.real_buffer(size)

    lib.fft_forward_real(ctypes.byref(fft), ffitt.float_array(real_only),
                         spectrum)
    lib.fft_inverse_real(ctypes.byref(fft), spectrum, output, work)

    room = room_for([(value, 0.0) for value in real_only])

    for index, value in enumerate(real_only):
        assert abs(output[index] - value) <= room

    lib.fft_free(ctypes.byref(fft))


@given(st.integers(min_value=0, max_value=200))
def test_only_a_power_of_two_above_one_is_taken(lib, size):
    """The module is radix two, thus nothing else can be transformed."""
    expected = (size > 1) and ((size & (size - 1)) == 0)
    assert lib.fft_is_valid_size(size) == expected


@given(SIZES, st.integers(min_value=0, max_value=31),
       st.floats(min_value=1.0, max_value=48000.0, width=32))
def test_the_frequency_of_a_bin_mirrors_about_the_middle(lib, size, bin_index,
                                                          rate):
    """A bin above the middle stands for a frequency below nothing.

    Bin k and bin size minus k are the same frequency at opposite turns, thus
    their frequencies must add to nothing.

    Two bins are their own mirror and stand outside this rule: bin 0, which is
    the level of the signal, and the bin at half the size, which stands at half
    the sample rate and has no partner to pair with.
    """
    bin_index = bin_index % size
    assume(bin_index != 0)
    assume(bin_index != (size // 2))

    low = lib.fft_bin_frequency(bin_index, size, rate)
    high = lib.fft_bin_frequency(size - bin_index, size, rate)

    assert sp.close(low + high, 0.0, relative=1e-4,
                    absolute=1e-3 * max(rate, 1.0))
