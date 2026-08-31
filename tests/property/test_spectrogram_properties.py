"""Rules that turning frames into a readable unit must keep.

A short-time transform gives complex numbers whose size depends on how long the
block is, on which window was laid on it, and on the fact that half the power
sits in a mirrored half that is not there. THE WHOLE OF THIS MODULE IS THE
CORRECTION OF THOSE THREE, and the wrong answer looks perfectly reasonable.

Thus the property this file is built around is not a shape of the interface. It
is that THE SAME TONE READS THE SAME NUMBER whatever block and whatever window
it was measured with. A module that failed it would give an answer that meant
nothing outside the one program that made it.
"""

import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402

RUNS = settings(max_examples=25, deadline=None)

TWO_PI = 2.0 * math.pi
RATE = 1024.0

BLOCKS = st.sampled_from([64, 128, 256])
WINDOWS = st.sampled_from(ffitt.WINDOWS_WITHOUT_A_PARAMETER)


def measured(lib, values, block, window, kind, hop=None):
    """Take the frames of a signal and turn them into the given unit."""
    if hop is None:
        hop = block // 2

    stft = lib.stft_alloc(block)

    try:
        assert lib.stft_design(stft, hop, window, sp.to_float32(0.0))

        size = len(values)
        frames = lib.stft_frame_count(size, block, hop)
        assume(frames >= 2)

        bins = (block // 2) + 1
        room = frames * bins

        spectrum = (ffitt.Cnum * room)()

        # The last argument is the room in the OUTPUT and not the count of
        # frames: the output holds one complex number for each bin of each
        # frame.
        assert lib.stft_forward(stft, ffitt.float_array(values), size,
                                spectrum, room)

        out = ffitt.real_buffer(room)

        assert lib.spectrogram_build(stft, spectrum, frames, kind,
                                     sp.to_float32(RATE), out, room)

        return [[out[(frame * bins) + bin_index] for bin_index in range(bins)]
                for frame in range(frames)], bins
    finally:
        lib.stft_free(stft)


def tone(count, frequency, height=1.0):
    return [sp.to_float32(height * math.sin(TWO_PI * frequency * index / RATE))
            for index in range(count)]


@given(BLOCKS, WINDOWS, st.sampled_from([64.0, 128.0, 192.0]),
       st.floats(min_value=0.25, max_value=4.0, width=32))
@RUNS
def test_a_tone_reads_its_own_height_whatever_the_block_and_window(
        lib, block, window, frequency, height):
    """THE CLAIM THE WHOLE MODULE EXISTS TO MAKE.

    A transform of a longer block gives larger numbers for the same signal, a
    window makes them smaller, and half of the power sits in a mirrored half
    that is not there. Left uncorrected the same tone reads differently for
    every choice of block and window.

    In the amplitude unit a tone of height a must read a, and it must read a
    whatever it was measured with. This is held across three blocks and every
    window that takes no parameter."""
    # The tone is put on a bin of the block, so that it is not spread across
    # two and read low in both.
    bin_index = round(frequency * block / RATE)
    assume(0 < bin_index < block // 2)
    frequency = bin_index * RATE / block

    values = tone(block * 8, frequency, height)

    rows, bins = measured(lib, values, block, window,
                          ffitt.SPECTROGRAM_AMPLITUDE)

    # The middle frame, where the block is full of the tone rather than of the
    # start of the signal.
    row = rows[len(rows) // 2]

    assert abs(row[bin_index] - height) <= 0.05 * height


@given(BLOCKS, WINDOWS, st.sampled_from([64.0, 128.0]),
       st.floats(min_value=0.25, max_value=4.0, width=32))
@RUNS
def test_the_power_is_the_amplitude_squared_and_halved(lib, block, window,
                                                       frequency, height):
    """WHAT POWER MEANS FOR A WAVE. A wave of height a has a mean power of a
    squared over two, because a sine spends half its time above and half below.
    The two units must therefore stand in exactly that relation at every bin of
    every frame, and not merely at the loud ones."""
    bin_index = round(frequency * block / RATE)
    assume(0 < bin_index < block // 2)

    values = tone(block * 8, bin_index * RATE / block, height)

    amplitude, _ = measured(lib, values, block, window,
                            ffitt.SPECTROGRAM_AMPLITUDE)
    power, _ = measured(lib, values, block, window, ffitt.SPECTROGRAM_POWER)

    for row_a, row_p in zip(amplitude, power):
        for one, other in zip(row_a, row_p):
            wanted = (one * one) / 2.0

            assert abs(other - wanted) <= 1e-4 * (1.0 + wanted)


@given(WINDOWS, st.sampled_from([64.0, 128.0]))
@RUNS
def test_the_density_is_the_one_unit_that_does_not_move_with_the_block(
        lib, window, frequency):
    """THE REASON FOUR UNITS ARE OFFERED AND NOT ONE.

    A tone is one wave and its power sits in one bin, thus its POWER reading is
    the same whatever the block. Noise is spread across every bin, thus a longer
    block cuts the same power into more bins and the power in each of them
    falls. Power for each HERTZ does not move, because a longer block has
    narrower bins in exactly the same measure.

    Held on noise, where the difference shows: the density must agree across
    blocks and the power must not."""
    state = 5
    values = []

    for _ in range(4096):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        values.append(sp.to_float32((((state >> 16) % 20000) / 10000.0) - 1.0))

    def middle_mean(block, kind):
        rows, bins = measured(lib, values, block, window, kind)
        row = rows[len(rows) // 2]
        # Bin 0 and the last bin are read from fewer numbers than the rest.
        inside = row[1:-1]
        return sum(inside) / len(inside)

    small_density = middle_mean(64, ffitt.SPECTROGRAM_DENSITY)
    large_density = middle_mean(256, ffitt.SPECTROGRAM_DENSITY)

    small_power = middle_mean(64, ffitt.SPECTROGRAM_POWER)
    large_power = middle_mean(256, ffitt.SPECTROGRAM_POWER)

    assume(small_density > 0.0 and small_power > 0.0)

    # The density holds across a fourfold change of block.
    assert abs(large_density - small_density) <= 0.35 * small_density

    # The power does not, and falls by about the same fourfold.
    assert large_power < (0.5 * small_power)


@given(BLOCKS, WINDOWS, st.sampled_from([64.0, 128.0]))
@RUNS
def test_the_decibel_unit_is_the_logarithm_of_the_power(lib, block, window,
                                                        frequency):
    """Decibels are ten times the logarithm of a power against a reference of
    one, and nothing else. A caller reading a picture in decibels and a number
    in power must be looking at the same thing."""
    bin_index = round(frequency * block / RATE)
    assume(0 < bin_index < block // 2)

    values = tone(block * 8, bin_index * RATE / block)

    power, _ = measured(lib, values, block, window, ffitt.SPECTROGRAM_POWER)
    decibel, _ = measured(lib, values, block, window,
                          ffitt.SPECTROGRAM_DECIBEL)

    for row_p, row_d in zip(power, decibel):
        for one, other in zip(row_p, row_d):
            if one <= 0.0:
                # A bin holding nothing has no logarithm, and the module holds
                # it at the floor rather than at minus infinity.
                assert other <= ffitt.SPECTROGRAM_FLOOR_DECIBEL + 1e-3
                continue

            wanted = 10.0 * math.log10(one)

            if wanted < ffitt.SPECTROGRAM_FLOOR_DECIBEL:
                assert other <= ffitt.SPECTROGRAM_FLOOR_DECIBEL + 1e-3
            else:
                assert abs(other - wanted) <= 1e-2 * (1.0 + abs(wanted))


@given(BLOCKS, WINDOWS, st.sampled_from([64.0, 128.0]),
       st.floats(min_value=0.25, max_value=4.0, width=32))
@RUNS
def test_a_tone_puts_its_reading_at_its_own_frequency(lib, block, window,
                                                      frequency, height):
    """A unit that scaled right and put the tone at the wrong bin would be no
    use. The loudest bin of the middle frame must be the bin the tone sits
    on."""
    bin_index = round(frequency * block / RATE)
    assume(0 < bin_index < (block // 2) - 1)

    values = tone(block * 8, bin_index * RATE / block, height)

    rows, bins = measured(lib, values, block, window,
                          ffitt.SPECTROGRAM_AMPLITUDE)

    row = rows[len(rows) // 2]
    loudest = max(range(bins), key=lambda index: row[index])

    assert loudest == bin_index


@given(BLOCKS, WINDOWS, st.sampled_from([64.0, 128.0]),
       st.floats(min_value=0.25, max_value=8.0, width=32))
@RUNS
def test_turning_the_signal_up_turns_the_amplitude_up_by_as_much(
        lib, block, window, frequency, louder):
    """The amplitude is in the unit of the signal, thus it must scale with it.
    The power, being a square, must scale with the square."""
    bin_index = round(frequency * block / RATE)
    assume(0 < bin_index < block // 2)

    quiet = tone(block * 8, bin_index * RATE / block)
    loud = [sp.to_float32(value * louder) for value in quiet]

    quiet_rows, _ = measured(lib, quiet, block, window,
                             ffitt.SPECTROGRAM_AMPLITUDE)
    loud_rows, _ = measured(lib, loud, block, window,
                            ffitt.SPECTROGRAM_AMPLITUDE)

    for row_q, row_l in zip(quiet_rows, loud_rows):
        for one, other in zip(row_q, row_l):
            assert abs(other - (one * louder)) <= 1e-3 * (1.0 + abs(other))


@given(BLOCKS, WINDOWS, st.sampled_from([64.0, 128.0]))
@RUNS
def test_reading_against_the_largest_puts_the_largest_at_nothing(lib, block,
                                                                 window,
                                                                 frequency):
    """A picture is read against its own loudest point, thus that point becomes
    nothing and everything else is below it. Nothing may come out above."""
    bin_index = round(frequency * block / RATE)
    assume(0 < bin_index < block // 2)

    values = tone(block * 8, bin_index * RATE / block)

    rows, bins = measured(lib, values, block, window, ffitt.SPECTROGRAM_POWER)

    flat = [value for row in rows for value in row]
    count = len(flat)

    given_values = ffitt.float_array(flat)
    out = ffitt.real_buffer(count)

    largest = lib.spectrogram_largest(given_values, count)

    assert largest == max(flat)

    assert lib.spectrogram_against_the_largest(given_values, count, out)

    top = max(out[index] for index in range(count))

    assert abs(top) <= 1e-3

    for index in range(count):
        assert out[index] <= 1e-3


@given(BLOCKS, WINDOWS, st.sampled_from([64.0, 128.0]))
@RUNS
def test_the_amplitude_and_the_power_are_never_below_nothing(lib, block,
                                                             window,
                                                             frequency):
    """A size and a power cannot be negative however the arithmetic is
    arranged."""
    values = tone(block * 8, frequency)

    for kind in (ffitt.SPECTROGRAM_AMPLITUDE, ffitt.SPECTROGRAM_POWER,
                 ffitt.SPECTROGRAM_DENSITY):
        rows, _ = measured(lib, values, block, window, kind)

        for row in rows:
            for value in row:
                assert math.isfinite(value)
                assert value >= 0.0


@given(st.integers(min_value=-4, max_value=8))
def test_only_the_four_units_are_taken(lib, kind):
    assert lib.spectrogram_is_valid_kind(kind) == (0 <= kind <= 3)
