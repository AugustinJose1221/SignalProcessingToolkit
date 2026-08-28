"""Rules that the analytic signal must keep.

The analytic signal is not a shape the module chose. It is the one signal whose
spectrum holds no negative frequencies and whose real part is the signal it came
from, and everything below is a consequence of that rather than of how the
module is written.

What is held here is the arithmetic of it: the real part comes back, the
imaginary part is the signal turned a quarter turn, the size follows the
envelope, and the phase moves at the frequency of the tone.
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

RUNS = settings(max_examples=30, deadline=None)

TWO_PI = 2.0 * math.pi

SIZES = st.sampled_from([64, 128, 256])

# Bins well inside the band, so that neither end of the spectrum is involved.
BINS = st.integers(min_value=3, max_value=12)


def analytic(lib, values):
    size = len(values)
    fft = lib.fft_alloc(size)
    out = (sptk.Cnum * size)()

    try:
        lib.hilbert_analytic_signal(fft, sptk.float_array(values), out)
    finally:
        lib.fft_free(fft)

    return [complex(out[index].re, out[index].im) for index in range(size)]


def amplitude(lib, values):
    size = len(values)
    fft = lib.fft_alloc(size)
    work = (sptk.Cnum * size)()
    out = sptk.real_buffer(size)

    try:
        lib.hilbert_analytic_signal(fft, sptk.float_array(values), work)
        lib.hilbert_amplitude(work, out, size)
    finally:
        lib.fft_free(fft)

    return [out[index] for index in range(size)]


def tone(size, bin_index, height=1.0, turn=0.0):
    return [sp.to_float32(height * math.cos((TWO_PI * bin_index * index / size)
                                            + (TWO_PI * turn)))
            for index in range(size)]


def noise(count, seed=1):
    state = seed
    out = []

    for _ in range(count):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        out.append(sp.to_float32((((state >> 16) % 20000) / 10000.0) - 1.0))

    return out


@given(SIZES, st.integers(1, 64))
@RUNS
def test_the_real_part_is_the_signal_it_came_from(lib, size, seed):
    """THE FIRST HALF OF WHAT THE ANALYTIC SIGNAL IS. Whatever else it does, the
    real part must give the signal back: an analytic signal that changed the
    signal would be describing something else."""
    values = noise(size, seed)
    got = analytic(lib, values)

    scale = 1.0 + max(abs(value) for value in values)

    for one, other in zip(values, got):
        assert abs(other.real - one) <= 1e-4 * scale


@given(SIZES, BINS, st.floats(min_value=0.25, max_value=4.0, width=32),
       st.floats(min_value=0.0, max_value=1.0, width=32))
@RUNS
def test_the_imaginary_part_is_the_signal_turned_a_quarter_turn(lib, size,
                                                                bin_index,
                                                                height, turn):
    """THE SECOND HALF, AND THE WHOLE OF WHAT THE HILBERT TRANSFORM IS. It turns
    every frequency by a quarter of a turn and leaves its size alone, thus a
    cosine becomes a sine.

    Held on a tone that sits exactly on a bin, because a tone between two bins
    is not a whole number of turns in the block and its ends do not meet."""
    values = tone(size, bin_index, height, turn)
    got = analytic(lib, values)

    for index in range(size):
        wanted = height * math.sin((TWO_PI * bin_index * index / size)
                                   + (TWO_PI * turn))

        assert abs(got[index].imag - wanted) <= 1e-3 * (1.0 + height)


@given(SIZES, BINS, st.floats(min_value=0.25, max_value=4.0, width=32))
@RUNS
def test_the_size_of_a_steady_tone_is_the_height_of_that_tone(lib, size,
                                                              bin_index,
                                                              height):
    """WHAT THE AMPLITUDE MEANS. A tone of a fixed height has a fixed envelope,
    thus the size of the analytic signal must be that height at every sample and
    not swing with the tone. That is the thing an envelope follower is for, and
    it is what taking the size of the signal itself cannot give."""
    values = tone(size, bin_index, height)
    got = amplitude(lib, values)

    for value in got:
        assert abs(value - height) <= 1e-3 * (1.0 + height)


@given(SIZES, BINS, st.integers(min_value=1, max_value=3))
@RUNS
def test_the_size_follows_an_envelope_that_moves(lib, size, bin_index, slow):
    """THE REASON THE MODULE EXISTS. A tone whose height is being changed
    slowly has an envelope, and the size of the analytic signal follows it.
    Reading the height off the signal itself would give something that swings
    at the tone; this gives the envelope alone."""
    assume(slow * 4 < bin_index)

    values = []

    for index in range(size):
        envelope = 1.0 + (0.5 * math.sin(TWO_PI * slow * index / size))
        values.append(sp.to_float32(
            envelope * math.cos(TWO_PI * bin_index * index / size)))

    got = amplitude(lib, values)

    # Away from the two ends, where a block that is treated as one turn of
    # something repeating has its own edge.
    for index in range(size // 8, size - (size // 8)):
        wanted = 1.0 + (0.5 * math.sin(TWO_PI * slow * index / size))

        assert abs(got[index] - wanted) <= 0.05


@given(SIZES, BINS)
@RUNS
def test_the_signal_and_its_quarter_turn_stand_at_right_angles(lib, size,
                                                               bin_index):
    """A CONSEQUENCE THAT NEEDS NO TONE AT ALL. Turning every frequency by a
    quarter of a turn leaves a signal that shares nothing with the one it came
    from: multiplied together and added up across the block they come to
    nothing. That is what being at right angles MEANS, and it holds for any
    signal and not only for a tone."""
    values = noise(size, 11)
    got = analytic(lib, values)

    together = sum(value.real * value.imag for value in got)
    loudness = sum(value.real * value.real for value in got)

    assume(loudness > 1.0)

    assert abs(together) <= 0.05 * loudness


@given(SIZES, BINS, st.floats(min_value=0.25, max_value=8.0, width=32))
@RUNS
def test_turning_the_signal_up_turns_the_size_up_by_as_much(lib, size,
                                                            bin_index,
                                                            louder):
    """The transform is linear, thus the envelope of a signal turned up is the
    envelope turned up. A caller measuring a level relies on it."""
    values = tone(size, bin_index)
    louder_values = [sp.to_float32(value * louder) for value in values]

    plain = amplitude(lib, values)
    scaled = amplitude(lib, louder_values)

    for one, other in zip(plain, scaled):
        assert abs(other - (one * louder)) <= 1e-3 * (1.0 + abs(other))


@given(SIZES, st.integers(1, 64))
@RUNS
def test_the_size_is_never_below_nothing(lib, size, seed):
    """It is a distance from the origin, thus it cannot be."""
    for value in amplitude(lib, noise(size, seed)):
        assert math.isfinite(value)
        assert value >= 0.0


@given(SIZES, BINS)
@RUNS
def test_the_phase_of_a_steady_tone_moves_at_the_rate_of_that_tone(lib, size,
                                                                   bin_index):
    """WHAT THE INSTANTANEOUS FREQUENCY IS. The phase of the analytic signal of
    a tone turns by the same amount at every sample, and that amount IS the
    frequency. A phase that turned unevenly would give a frequency that moved
    where nothing moved."""
    values = tone(size, bin_index)
    fft = lib.fft_alloc(size)
    work = (sptk.Cnum * size)()
    out = sptk.real_buffer(size)

    rate = 1024.0

    try:
        lib.hilbert_analytic_signal(fft, sptk.float_array(values), work)
        lib.hilbert_frequency(work, out, size, sp.to_float32(rate))
    finally:
        lib.fft_free(fft)

    wanted = (bin_index * rate) / size

    # The list holds one value less than the signal, because a change needs two
    # points. The ends are left out for the reason the envelope test gives.
    for index in range(size // 8, size - (size // 8) - 1):
        assert abs(out[index] - wanted) <= 1e-2 * wanted


@given(SIZES, BINS, BINS)
@RUNS
def test_the_analytic_signal_of_a_sum_is_the_sum_of_the_analytic_signals(
        lib, size, first, second):
    """LINEARITY. The transform is a transform, a doubling, and another
    transform, and every one of those three is linear, thus so is the whole. A
    caller may therefore work on parts of a signal and add them up."""
    assume(first != second)

    one = tone(size, first)
    other = tone(size, second)
    both = [sp.to_float32(a + b) for a, b in zip(one, other)]

    from_sum = analytic(lib, both)
    added = [a + b for a, b in zip(analytic(lib, one), analytic(lib, other))]

    for got, wanted in zip(from_sum, added):
        assert abs(got - wanted) <= 1e-3 * (1.0 + abs(wanted))


@given(SIZES)
@RUNS
def test_a_signal_that_does_not_change_has_no_quarter_turn_to_take(lib, size):
    """A level holds one frequency and that frequency is nothing. Turning
    nothing by a quarter of a turn leaves nothing, thus the imaginary part is
    nothing and the size is the level itself."""
    values = [sp.to_float32(2.5)] * size
    got = analytic(lib, values)

    for value in got:
        assert abs(value.real - 2.5) <= 1e-3
        assert abs(value.imag) <= 1e-3
