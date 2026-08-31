"""Rules that a power spectral density must keep.

A density is not a spectrum with a scale on it. It is power for each hertz, and
what makes it that rather than something else is one identity: ADDED UP ACROSS
THE WHOLE BAND IT COMES TO THE POWER OF THE SIGNAL. That is Parseval's rule, it
is what the window scaling exists to make true, and it is what this file is
built around.

Everything else here is a consequence: a tone of a known height carries a known
power, noise spreads its power evenly, and averaging more blocks steadies the
answer without moving it.
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

BLOCKS = st.sampled_from([64, 128, 256])
WINDOWS = st.sampled_from(sptk.WINDOWS_WITHOUT_A_PARAMETER)


def designed(lib, block, window, overlap=None):
    psd = lib.psd_alloc(block)

    if overlap is None:
        overlap = block // 2

    assert lib.psd_design(psd, overlap, window, sp.to_float32(0.0))

    return psd


def density_of(lib, psd, values, block):
    bins = lib.psd_bin_count(psd)
    out = sptk.real_buffer(bins)

    assert lib.psd_estimate(psd, sptk.float_array(values), len(values),
                            sp.to_float32(RATE), out)

    return [out[index] for index in range(bins)]


def noise(count, seed=1):
    state = seed
    out = []

    for _ in range(count):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        out.append(sp.to_float32((((state >> 16) % 20000) / 10000.0) - 1.0))

    return out


def mean_power(values):
    return sum(value * value for value in values) / len(values)


@given(BLOCKS, WINDOWS, st.integers(1, 64))
@RUNS
def test_the_density_added_across_the_band_is_the_power_of_the_signal(
        lib, block, window, seed):
    """PARSEVAL'S RULE, AND THE WHOLE REASON THIS IS A DENSITY.

    The density is power for each hertz. Added up across every hertz there is,
    from nothing to half the sample rate, it must come to the power the signal
    actually holds. A number that did not would be a spectrum with an arbitrary
    scale on it, and no caller could turn it into anything they could measure.

    THIS IS WHAT THE WINDOW SCALING IS FOR. A window takes power out of the
    block, and the module divides it back out again. If it did not, every answer
    would be low by an amount that depended on which window was chosen."""
    size = block * 16
    values = noise(size, seed)

    psd = designed(lib, block, window)

    try:
        density = density_of(lib, psd, values, block)

        gathered = lib.psd_band_power(psd, sptk.float_array(density),
                                      sp.to_float32(RATE), sp.to_float32(0.0),
                                      sp.to_float32(RATE / 2.0))
    finally:
        lib.psd_free(psd)

    wanted = mean_power(values)

    assert abs(gathered - wanted) <= 0.1 * wanted


@given(BLOCKS, WINDOWS, st.integers(min_value=4, max_value=20),
       st.floats(min_value=0.25, max_value=4.0, width=32))
@RUNS
def test_a_tone_carries_half_the_square_of_its_height(lib, block, window,
                                                      bin_index, height):
    """THE SAME RULE ON A SIGNAL WHOSE POWER IS KNOWN WITHOUT MEASURING IT. A
    tone of height a has a mean power of a squared over two, thus that is what
    the density must add up to. Anything else and a caller reading a level off
    a spectrum would read the wrong one."""
    assume(bin_index < block // 4)

    size = block * 16

    values = [sp.to_float32(height * math.sin(TWO_PI * bin_index * index
                                              / block))
              for index in range(size)]

    psd = designed(lib, block, window)

    try:
        density = density_of(lib, psd, values, block)

        gathered = lib.psd_band_power(psd, sptk.float_array(density),
                                      sp.to_float32(RATE), sp.to_float32(0.0),
                                      sp.to_float32(RATE / 2.0))
    finally:
        lib.psd_free(psd)

    wanted = (height * height) / 2.0

    assert abs(gathered - wanted) <= 0.1 * wanted


@given(BLOCKS, WINDOWS, st.integers(min_value=4, max_value=20))
@RUNS
def test_a_tone_puts_its_power_where_the_tone_is(lib, block, window,
                                                 bin_index):
    """A DENSITY THAT ADDED UP RIGHT AND PUT THE POWER IN THE WRONG PLACE WOULD
    BE NO USE AT ALL. The band around the tone must hold nearly all of it, and
    the rest of the spectrum nearly none."""
    assume(bin_index < block // 4)

    size = block * 16
    frequency = (bin_index * RATE) / block

    values = [sp.to_float32(math.sin(TWO_PI * bin_index * index / block))
              for index in range(size)]

    psd = designed(lib, block, window)

    try:
        density = sptk.float_array(density_of(lib, psd, values, block))
        width = lib.psd_bin_width(psd, sp.to_float32(RATE))

        near = lib.psd_band_power(psd, density, sp.to_float32(RATE),
                                  sp.to_float32(frequency - (3.0 * width)),
                                  sp.to_float32(frequency + (3.0 * width)))
        whole = lib.psd_band_power(psd, density, sp.to_float32(RATE),
                                   sp.to_float32(0.0),
                                   sp.to_float32(RATE / 2.0))
    finally:
        lib.psd_free(psd)

    assert near > (0.9 * whole)


@given(BLOCKS, WINDOWS, st.floats(min_value=0.25, max_value=8.0, width=32),
       st.integers(1, 64))
@RUNS
def test_turning_the_signal_up_squares_into_the_density(lib, block, window,
                                                        louder, seed):
    """Power is the square of a height, thus a signal turned up by some amount
    has a density turned up by the SQUARE of it. A density that scaled linearly
    would not be a power at all."""
    size = block * 16
    values = noise(size, seed)
    louder_values = [sp.to_float32(value * louder) for value in values]

    psd = designed(lib, block, window)

    try:
        plain = density_of(lib, psd, values, block)
        scaled = density_of(lib, psd, louder_values, block)
    finally:
        lib.psd_free(psd)

    for one, other in zip(plain, scaled):
        wanted = one * louder * louder

        assert abs(other - wanted) <= 1e-2 * (1.0 + wanted)


@given(BLOCKS, WINDOWS, st.integers(1, 64))
@RUNS
def test_noise_spreads_its_power_evenly_across_the_band(lib, block, window,
                                                        seed):
    """WHAT WHITE NOISE MEANS. It holds the same power at every frequency, thus
    its density is flat. A density that leaned would be describing a colour the
    noise does not have."""
    size = block * 64
    values = noise(size, seed)

    psd = designed(lib, block, window)

    try:
        density = density_of(lib, psd, values, block)
    finally:
        lib.psd_free(psd)

    # Bin 0 holds the level and the last bin the highest frequency the rate can
    # carry; both are read from fewer numbers than the rest.
    middle = density[1:-1]
    mean = sum(middle) / len(middle)

    assume(mean > 0.0)

    # Every bin stands within a factor of three of the mean, which a flat
    # spectrum measured from a finite run does.
    for value in middle:
        assert (mean / 3.0) < value < (mean * 3.0)


@given(BLOCKS, WINDOWS, st.integers(1, 64))
@RUNS
def test_averaging_more_blocks_steadies_the_answer_without_moving_it(
        lib, block, window, seed):
    """THE WHOLE REASON THE METHOD CUTS THE SIGNAL INTO BLOCKS AT ALL.

    One block gives a density whose every bin is as uncertain as the last. Cut
    the signal into many and average them, and the answer settles: that is
    Welch's method and it is what this module is. The mean must not move as the
    blocks are added, and the spread must fall.

    THE MEAN AND THE SPREAD ARE TAKEN ACROSS THE BINS OF ONE ANSWER, which for
    noise is a flat spectrum, thus the spread across bins IS the uncertainty of
    the estimate."""
    values = noise(block * 128, seed)

    psd = designed(lib, block, window)

    def spread_of(count):
        density = density_of(lib, psd, values[:block * count], block)
        middle = density[1:-1]
        mean = sum(middle) / len(middle)
        spread = math.sqrt(sum((value - mean) ** 2 for value in middle)
                           / len(middle))

        return mean, spread

    try:
        few_mean, few_spread = spread_of(4)
        many_mean, many_spread = spread_of(128)
    finally:
        lib.psd_free(psd)

    assume(few_mean > 0.0)

    # The answer does not move.
    assert abs(many_mean - few_mean) <= 0.25 * few_mean

    # And it steadies.
    assert many_spread < few_spread


@given(BLOCKS, WINDOWS)
@RUNS
def test_the_bins_cover_the_band_from_nothing_to_half_the_rate(lib, block,
                                                               window):
    """The bins are what the density is read at, thus they must span the whole
    of what a sampled signal can hold and no more. A bin past half the rate
    would stand for a frequency that folds."""
    psd = designed(lib, block, window)

    try:
        bins = lib.psd_bin_count(psd)
        width = lib.psd_bin_width(psd, sp.to_float32(RATE))

        assert bins == (block // 2) + 1

        first = lib.psd_bin_frequency(psd, 0, sp.to_float32(RATE))
        last = lib.psd_bin_frequency(psd, bins - 1, sp.to_float32(RATE))

        assert abs(first) <= 1e-6
        assert abs(last - (RATE / 2.0)) <= 1e-3 * RATE

        # Evenly spaced, by exactly the width the module reports.
        for index in range(1, bins):
            step = (lib.psd_bin_frequency(psd, index, sp.to_float32(RATE))
                    - lib.psd_bin_frequency(psd, index - 1,
                                            sp.to_float32(RATE)))

            assert abs(step - width) <= 1e-3 * width
    finally:
        lib.psd_free(psd)


@given(BLOCKS, WINDOWS, st.integers(1, 64))
@RUNS
def test_the_density_is_never_below_nothing(lib, block, window, seed):
    """It is a power, and a power cannot be negative however the arithmetic is
    arranged."""
    values = noise(block * 16, seed)

    psd = designed(lib, block, window)

    try:
        for value in density_of(lib, psd, values, block):
            assert math.isfinite(value)
            assert value >= 0.0
    finally:
        lib.psd_free(psd)


@given(BLOCKS, WINDOWS)
@RUNS
def test_a_signal_shorter_than_one_block_is_refused(lib, block, window):
    """There is nothing to transform, thus there is no density to give."""
    psd = designed(lib, block, window)

    try:
        out = sptk.real_buffer(lib.psd_bin_count(psd))
        values = sptk.float_array(noise(block))

        assert not lib.psd_estimate(psd, values, block - 1,
                                    sp.to_float32(RATE), out)
        assert lib.psd_estimate(psd, values, block, sp.to_float32(RATE), out)
    finally:
        lib.psd_free(psd)


@given(st.integers(min_value=0, max_value=2048))
def test_only_a_block_the_transform_can_take_and_that_holds_a_bin_is_taken(
        lib, block):
    """The block goes to the transform, thus it must be one the transform can
    take. AND THIS MODULE ASKS FOR MORE THAN THAT: a block of two holds one bin
    above zero frequency, which is the least that could mean anything, thus four
    is the smallest it will answer for.

    Written against both bounds rather than against fft alone. csd, which is
    built the same way, takes whatever the transform takes and so accepts a
    block of two where this refuses it."""
    assert lib.psd_is_valid_block(block) == (lib.fft_is_valid_size(block)
                                             and block >= 4)
