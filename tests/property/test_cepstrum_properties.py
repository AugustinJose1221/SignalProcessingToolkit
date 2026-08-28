"""Rules that finding what repeats in a spectrum must keep.

The module answers two questions correlation cannot: the period of a note whose
fundamental is missing, and the delay of an echo. What must hold is that it
answers them across the notes and the echoes it might be given, and that it says
when there is nothing there.
"""

import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

RUNS = settings(max_examples=20, deadline=None)

SIZE = 1024
TWO_PI = 2.0 * math.pi

# Periods that fit several times into the block and leave room for harmonics
# well below half the sample rate.
PERIODS = st.sampled_from([32.0, 40.0, 50.0, 64.0, 80.0, 100.0, 128.0])
NOISES = st.sampled_from([0.0, 0.01, 0.05])


def noise_from(state, count):
    out = []
    for _ in range(count):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        out.append((((state >> 16) % 2000) / 1000.0) - 1.0)
    return out


def note(period, lowest, highest, noise_level, seed=1):
    drawn = noise_from(seed, SIZE)
    out = []

    for index in range(SIZE):
        total = noise_level * drawn[index]

        for harmonic in range(lowest, highest + 1):
            total += (1.0 / harmonic) * math.sin(
                TWO_PI * harmonic * index / period)

        out.append(sp.to_float32(total))

    return out


def cepstrum_of(lib, values):
    ceps = lib.cepstrum_alloc(SIZE)
    out = sptk.real_buffer(SIZE)

    try:
        assert lib.cepstrum_real(ceps, sptk.float_array(values), out)
    finally:
        lib.cepstrum_free(ceps)

    return out


def best(lib, ceps, low=20, high=300):
    strength = sptk.real_buffer(1)
    where = lib.cepstrum_best_quefrency(ceps, SIZE, low, high, strength)

    return where, strength[0]


@given(PERIODS, NOISES)
@RUNS
def test_the_period_of_a_note_comes_back_within_a_sample(lib, period, noise):
    """THE REASON THE MODULE EXISTS. A row of evenly spaced peaks in the
    spectrum comes out as one peak here, and where it stands is the period.

    WITHIN A SAMPLE AND NOT EXACTLY. The quefrency axis is a whole number of
    samples and a real period rarely is, thus a period of 100 comes back as 99.
    That is the axis being sampled and not an error of the method."""
    # The harmonics must fit below half the sample rate.
    highest = min(12, int(period / 2) - 1)
    assume(highest >= 8)

    ceps = cepstrum_of(lib, note(period, 1, highest, noise))
    where, strength = best(lib, ceps)

    assert abs(where - period) <= 1.0
    assert strength > 0.1


@given(PERIODS, NOISES)
@RUNS
def test_a_note_with_no_fundamental_is_still_found(lib, period, noise):
    """THE ONE CORRELATION CANNOT DO. A small loudspeaker cannot make the
    lowest note it is asked for, thus the note arrives with its fundamental
    missing. The ear still hears it and so does this."""
    highest = min(12, int(period / 2) - 1)
    assume(highest >= 8)

    ceps = cepstrum_of(lib, note(period, 2, highest, noise))
    where, strength = best(lib, ceps)

    assert abs(where - period) <= 1.0
    assert strength > 0.1


@given(st.sampled_from([60, 80, 100, 140, 200]),
       st.sampled_from([0.4, 0.7, 0.9]))
@RUNS
def test_the_delay_of_an_echo_is_found(lib, delay, loudness):
    """A sound and the same sound again a little later multiply the spectrum by
    a ripple, and a ripple in the spectrum is a peak here."""
    burst = noise_from(1, SIZE)

    values = []

    for index in range(SIZE):
        total = burst[index]

        if index >= delay:
            total += loudness * burst[index - delay]

        values.append(sp.to_float32(total))

    ceps = cepstrum_of(lib, values)
    where, strength = best(lib, ceps, 20, 300)

    assert abs(where - delay) <= 1.0
    assert strength > 0.05


@given(st.integers(min_value=1, max_value=64))
@RUNS
def test_a_block_of_noise_says_there_is_nothing_to_find(lib, seed):
    """THE STRENGTH MUST BE READ. Every block has a highest place between the
    two bounds, and a block with nothing in it has one too."""
    values = [sp.to_float32(value) for value in noise_from(seed, SIZE)]

    ceps = cepstrum_of(lib, values)
    where, strength = best(lib, ceps)

    # It still gives an answer, and the strength says not to believe it.
    assert 20 <= where <= 300
    assert strength < 0.15


@given(PERIODS, NOISES)
@RUNS
def test_a_note_stands_far_above_noise_in_the_strength(lib, period, noise):
    """The whole worth of the strength is that the two can be told apart by
    it."""
    highest = min(12, int(period / 2) - 1)
    assume(highest >= 8)

    ceps = cepstrum_of(lib, note(period, 1, highest, noise))
    _, from_note = best(lib, ceps)

    ceps = cepstrum_of(lib, [sp.to_float32(value)
                             for value in noise_from(7, SIZE)])
    _, from_noise = best(lib, ceps)

    assert from_note > (2.0 * from_noise)


@given(PERIODS, st.floats(min_value=0.25, max_value=8.0, width=32))
@RUNS
def test_how_loud_the_note_is_does_not_move_the_answer(lib, period, louder):
    """The logarithm turns a scale into a level, and a level lands at quefrency
    nothing. Turning the note up therefore cannot move where the peak stands."""
    highest = min(12, int(period / 2) - 1)
    assume(highest >= 8)

    quiet = note(period, 1, highest, 0.0)
    loud = [sp.to_float32(value * louder) for value in quiet]

    one, _ = best(lib, cepstrum_of(lib, quiet))
    other, _ = best(lib, cepstrum_of(lib, loud))

    assert one == other


@given(PERIODS, NOISES)
@RUNS
def test_the_answer_is_the_same_at_either_width(lib, period, noise):
    """Held here because it was not always so. Written without a window the
    answer moved with the width of the build on a clean signal, which is how
    the fault was first seen."""
    highest = min(12, int(period / 2) - 1)
    assume(highest >= 8)

    ceps = cepstrum_of(lib, note(period, 2, highest, noise))
    where, _ = best(lib, ceps)

    # The truth, which both widths must land on within a sample.
    assert abs(where - period) <= 1.0


@given(st.integers(min_value=0, max_value=2048))
def test_only_a_block_the_transform_can_take_is_taken(lib, size):
    """The transform is taken twice, thus what this takes is exactly what the
    transform takes."""
    assert lib.cepstrum_is_valid_size(size) == lib.fft_is_valid_size(size)


@given(st.integers(min_value=0, max_value=600),
       st.integers(min_value=0, max_value=600))
def test_a_range_that_does_not_fit_is_refused(lib, low, high):
    """The first few places hold the shape of the spectrum, which is always
    large and always there, and the second half is the mirror of the first."""
    ceps = sptk.real_buffer(SIZE)

    for index in range(SIZE):
        ceps[index] = sp.to_float32(float(index))

    strength = sptk.real_buffer(1)
    strength[0] = sp.to_float32(7.0)

    fits = (low >= 1) and (high > low) and (high < SIZE // 2)
    where = lib.cepstrum_best_quefrency(ceps, SIZE, low, high, strength)

    if fits:
        assert low <= where <= high
    else:
        assert where == 0
        assert strength[0] == 0.0
