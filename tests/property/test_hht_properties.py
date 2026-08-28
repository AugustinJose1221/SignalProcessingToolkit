"""Rules that the Hilbert-Huang transform must keep.

A Fourier transform gives the frequencies of a whole block and says nothing
about WHEN each of them was there. This gives a frequency and an amplitude at
every sample, which is the thing a Fourier transform cannot give, and the tests
below are built on that difference rather than on the shape of the interface.

The strongest of them is the chirp: a signal whose frequency climbs steadily
reads as one wide band to a transform and reads as a climbing line here. If that
ever stopped holding, the module would have no reason to exist.
"""

import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

RUNS = settings(max_examples=25, deadline=None)

TWO_PI = 2.0 * math.pi
RATE = 1024.0

SIZES = st.sampled_from([128, 256, 512])
BINS = st.integers(min_value=4, max_value=16)


def made(lib, size, values):
    """An intrinsic mode function holding the given samples."""
    imf = lib.imf_alloc(size)

    for index in range(size):
        imf.x[index] = sp.to_float32(float(index))
        imf.y[index] = sp.to_float32(values[index])

    return imf


def transformed(lib, size, values, rate=RATE):
    """Give the amplitude and the frequency at every point."""
    imf = lib.imf_alloc(size)

    for index in range(size):
        imf.x[index] = sp.to_float32(float(index))
        imf.y[index] = sp.to_float32(values[index])

    fft = lib.fft_alloc(size)
    work = (sptk.Cnum * size)()
    amplitude = sptk.real_buffer(size)
    frequency = sptk.real_buffer(size)

    try:
        lib.hht_transform_imf(fft, imf, work, amplitude, frequency,
                              sp.to_float32(rate))
    finally:
        lib.fft_free(fft)
        lib.imf_free(imf)

    return ([amplitude[index] for index in range(size)],
            [frequency[index] for index in range(size - 1)])


def tone(size, bin_index, height=1.0):
    return [height * math.cos(TWO_PI * bin_index * index / size)
            for index in range(size)]


def inside(values, size):
    """The middle of a run, away from the two ends.

    The transform treats the block as one turn of something repeating, thus the
    first and last few samples see the end meeting the beginning. Every reading
    here is taken away from them, which is what a caller must do as well."""
    edge = size // 8

    return values[edge:len(values) - edge]


@given(SIZES, BINS, st.floats(min_value=0.25, max_value=4.0, width=32))
@RUNS
def test_a_steady_tone_reads_one_frequency_at_every_point(lib, size,
                                                           bin_index, height):
    """THE FIRST THING IT MUST DO. A function holding one frequency must read
    that frequency at every sample and not merely on average. A reading that
    wandered would say the frequency was changing where nothing changed."""
    wanted = (bin_index * RATE) / size

    amplitude, frequency = transformed(lib, size, tone(size, bin_index,
                                                       height))

    for value in inside(frequency, size):
        assert abs(value - wanted) <= 1e-2 * wanted

    for value in inside(amplitude, size):
        assert abs(value - height) <= 1e-2 * (1.0 + height)


@given(SIZES, BINS, st.floats(min_value=0.25, max_value=4.0, width=32))
@RUNS
def test_the_mean_frequency_of_a_steady_tone_is_that_frequency(lib, size,
                                                                bin_index,
                                                                height):
    """The mean weighs every point by the square of its amplitude. For a tone
    of a fixed height every point weighs the same, thus the mean must be the
    frequency itself."""
    wanted = (bin_index * RATE) / size

    amplitude, frequency = transformed(lib, size, tone(size, bin_index,
                                                       height))

    got = lib.hht_mean_frequency(sptk.float_array(amplitude),
                                 sptk.float_array(frequency), size)

    assert abs(got - wanted) <= 0.05 * wanted


@given(SIZES, st.integers(min_value=3, max_value=6),
       st.integers(min_value=10, max_value=20))
@RUNS
def test_a_frequency_that_climbs_is_read_as_climbing(lib, size, low, high):
    """THE REASON THE MODULE EXISTS, AND THE THING A FOURIER TRANSFORM CANNOT
    DO.

    A signal whose frequency climbs steadily holds every frequency between the
    two ends, thus a transform of the whole block gives a wide band and says
    nothing about when each of them was there. This gives a frequency at every
    sample, and for a climbing signal those readings must climb: low at the
    start, high at the end, and rising in between."""
    assume(high > low + 4)

    # A sweep from the low bin to the high one across the block. The phase is
    # the integral of the frequency, thus a frequency that climbs steadily
    # gives a phase that grows as the square.
    values = []

    for index in range(size):
        part = index / size
        turns = (low * part) + ((high - low) * part * part / 2.0)
        values.append(math.cos(TWO_PI * turns))

    amplitude, frequency = transformed(lib, size, values)

    # THE READING IS SET AGAINST THE RAMP ITSELF AND NOT MERELY AGAINST BEING
    # LARGER AT THE END. The instantaneous frequency IS the rate the phase
    # turns at, and the phase here was built as the integral of a straight
    # ramp, thus the frequency at each sample must be that ramp read at that
    # sample. Asking only that it ends higher than it began would pass for a
    # module that got the slope wrong by half.
    #
    # A QUARTER IS CUT FROM EACH END AND NOT AN EIGHTH. The transform treats
    # the block as one turn of something repeating, and a chirp does not
    # repeat: its end meets its beginning at a different frequency, and that
    # mismatch reaches further in than the wrap of a steady tone does.
    # Measured at a size of 128 sweeping 3 bins to 10, an eighth left a sample
    # reading 25.6 where 31.2 was wanted.
    edge = size // 4

    for index in range(edge, size - edge - 1):
        part = (index + 0.5) / size
        wanted = (low + ((high - low) * part)) * RATE / size

        assert abs(frequency[index] - wanted) <= 0.08 * wanted


@given(SIZES, BINS, st.integers(min_value=1, max_value=3))
@RUNS
def test_the_amplitude_follows_an_envelope_that_moves(lib, size, bin_index,
                                                      slow):
    """The other half of what it gives. A function whose height is being
    changed slowly has an envelope, and the amplitude follows it rather than
    swinging with the tone underneath."""
    assume(slow * 4 < bin_index)

    values = []

    for index in range(size):
        envelope = 1.0 + (0.5 * math.sin(TWO_PI * slow * index / size))
        values.append(envelope * math.cos(TWO_PI * bin_index * index / size))

    amplitude, _ = transformed(lib, size, values)

    edge = size // 8

    for index in range(edge, size - edge):
        wanted = 1.0 + (0.5 * math.sin(TWO_PI * slow * index / size))

        assert abs(amplitude[index] - wanted) <= 0.05


@given(st.lists(st.floats(min_value=0.0, max_value=4.0, width=32),
                min_size=4, max_size=32),
       st.lists(st.floats(min_value=1.0, max_value=200.0, width=32),
                min_size=4, max_size=32))
@RUNS
def test_the_mean_weighs_every_point_by_the_square_of_its_amplitude(
        lib, amplitudes, frequencies):
    """WHAT THE MEAN IS, held to its definition rather than to a tendency.

    A point with a small amplitude holds a phase that noise moves easily, thus
    the mean gives such a point little say. The rule is exact: each frequency
    counts as much as the SQUARE of the amplitude there.

    This is fed amplitude and frequency lists directly rather than a signal,
    because that is what the function takes and it is the only way to hold the
    rule itself rather than something a chosen signal happens to do."""
    count = min(len(amplitudes), len(frequencies)) + 1
    amplitudes = amplitudes[:count]
    frequencies = frequencies[:count - 1]

    weight = sum(value * value for value in amplitudes[:count - 1])
    assume(weight > 0.01)

    wanted = sum((a * a) * f for a, f
                 in zip(amplitudes[:count - 1], frequencies)) / weight

    got = lib.hht_mean_frequency(sptk.float_array(amplitudes),
                                 sptk.float_array(frequencies), count)

    assert abs(got - wanted) <= 1e-3 * (1.0 + abs(wanted))


@given(st.integers(min_value=8, max_value=24),
       st.floats(min_value=50.0, max_value=200.0, width=32))
@RUNS
def test_a_point_with_no_height_has_no_say_at_all(lib, count, wild):
    """THE CONSEQUENCE THAT MATTERS. Where the amplitude is nothing the phase
    is whatever noise made it, and the mean must not listen to it. Putting any
    frequency at all at a point of no height must leave the answer exactly where
    it was."""
    amplitudes = [1.0] * count
    frequencies = [64.0] * (count - 1)

    before = lib.hht_mean_frequency(sptk.float_array(amplitudes),
                                    sptk.float_array(frequencies), count)

    # One point silenced, and given a frequency from nowhere.
    amplitudes[count // 2] = 0.0
    frequencies[count // 2] = wild

    after = lib.hht_mean_frequency(sptk.float_array(amplitudes),
                                   sptk.float_array(frequencies), count)

    assert abs(after - before) <= 1e-4 * (1.0 + before)


@given(SIZES, BINS, st.floats(min_value=0.25, max_value=8.0, width=32))
@RUNS
def test_turning_the_function_up_moves_the_amplitude_and_not_the_frequency(
        lib, size, bin_index, louder):
    """A frequency is a statement about time and a height is not. Turning the
    function up must move one and leave the other exactly where it was."""
    quiet = tone(size, bin_index)
    loud = [value * louder for value in quiet]

    quiet_amplitude, quiet_frequency = transformed(lib, size, quiet)
    loud_amplitude, loud_frequency = transformed(lib, size, loud)

    for one, other in zip(quiet_amplitude, loud_amplitude):
        assert abs(other - (one * louder)) <= 1e-3 * (1.0 + abs(other))

    for one, other in zip(inside(quiet_frequency, size),
                          inside(loud_frequency, size)):
        assert abs(other - one) <= 1e-3 * (1.0 + abs(one))


@given(st.sampled_from([128, 256]), BINS, BINS)
@RUNS
def test_transforming_several_functions_is_transforming_each_of_them(
        lib, size, first, second):
    """The list form exists for speed and must exist for nothing else. It
    writes the answers one after the other, thus an error in the offsets would
    give one function the answer of another."""
    assume(first != second)

    one = tone(size, first)
    other = tone(size, second)

    imfs = (sptk.Imf * 2)()

    for which, values in enumerate((one, other)):
        imfs[which] = lib.imf_alloc(size)

        for index in range(size):
            imfs[which].x[index] = sp.to_float32(float(index))
            imfs[which].y[index] = sp.to_float32(values[index])

    fft = lib.fft_alloc(size)
    work = (sptk.Cnum * size)()
    amplitude = sptk.real_buffer(2 * size)
    frequency = sptk.real_buffer(2 * (size - 1))

    try:
        lib.hht_transform(fft, imfs, 2, work, amplitude, frequency,
                          sp.to_float32(RATE))
    finally:
        lib.fft_free(fft)
        for which in range(2):
            lib.imf_free(imfs[which])

    for which, values in enumerate((one, other)):
        alone_amplitude, alone_frequency = transformed(lib, size, values)

        for index in range(size):
            assert abs(amplitude[(which * size) + index]
                       - alone_amplitude[index]) <= 1e-4 * (
                           1.0 + abs(alone_amplitude[index]))

        for index in range(size - 1):
            got = frequency[(which * (size - 1)) + index]

            assert abs(got - alone_frequency[index]) <= 1e-3 * (
                1.0 + abs(alone_frequency[index]))


@given(SIZES, BINS)
@RUNS
def test_the_amplitude_is_never_below_nothing(lib, size, bin_index):
    """It is a distance from the origin, thus it cannot be."""
    amplitude, frequency = transformed(lib, size, tone(size, bin_index))

    for value in amplitude:
        assert math.isfinite(value)
        assert value >= 0.0

    for value in frequency:
        assert math.isfinite(value)
