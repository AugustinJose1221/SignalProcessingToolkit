"""Rules that a made signal must keep.

Every shape here is a function of one number: where in the turn the generator
stands. Most of what must hold follows from that, and holds whatever the shape
and whatever the frequency.
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

SHAPES = st.sampled_from([sptk.GENERATE_SINE, sptk.GENERATE_SQUARE,
                          sptk.GENERATE_SAWTOOTH, sptk.GENERATE_TRIANGLE])

NOISES = st.sampled_from([sptk.GENERATE_WHITE_NOISE, sptk.GENERATE_PINK_NOISE])

KINDS = st.sampled_from([sptk.GENERATE_SINE, sptk.GENERATE_SQUARE,
                         sptk.GENERATE_SAWTOOTH, sptk.GENERATE_TRIANGLE,
                         sptk.GENERATE_WHITE_NOISE, sptk.GENERATE_PINK_NOISE])

SAMPLE_RATE = 8192.0

# Frequencies a float of 32 bits holds exactly, well under half the rate.
FREQUENCIES = st.sampled_from([1.0, 7.0, 64.0, 300.0, 1000.0, 3000.0])


def block(lib, generator, count):
    out = sptk.real_buffer(count)
    assert lib.generate_block(generator, out, count)
    return [out[index] for index in range(count)]


def made(lib, kind, frequency, count, seed=7):
    generator = lib.generate_make(kind)
    assert lib.generate_design(generator, sp.to_float32(frequency),
                               sp.to_float32(SAMPLE_RATE))
    lib.generate_set_seed(generator, seed)
    return generator, block(lib, generator, count)


@given(KINDS, FREQUENCIES)
@RUNS
def test_nothing_ever_leaves_the_range_of_one(lib, kind, frequency):
    """Every shape is made to stand between minus one and one, and a caller
    scales from there. A shape that reached further would clip whatever it was
    scaled into, and the square wave did once reach two."""
    _, values = made(lib, kind, frequency, 4000)

    for value in values:
        assert math.isfinite(value)
        assert -1.0 - 1e-5 <= value <= 1.0 + 1e-5


@given(SHAPES, FREQUENCIES)
@RUNS
def test_a_shape_repeats_itself_after_a_whole_turn(lib, shape, frequency):
    """THE RULE THAT MAKES A SHAPE A SHAPE. The value depends on where in the
    turn the generator stands and on nothing else, thus setting the phase back
    to where it was must give the same value again."""
    generator = lib.generate_make(shape)
    assert lib.generate_design(generator, sp.to_float32(frequency),
                               sp.to_float32(SAMPLE_RATE))

    for place in [0.0, 0.125, 0.25, 0.5, 0.75, 0.875]:
        lib.generate_set_phase(generator, sp.to_float32(place))
        first = lib.generate_sample(generator)

        lib.generate_set_phase(generator, sp.to_float32(place))
        again = lib.generate_sample(generator)

        assert first == again


@given(SHAPES, FREQUENCIES)
@RUNS
def test_the_phase_stays_inside_one_turn(lib, shape, frequency):
    """The phase runs from nothing to one and wraps. A phase let grow instead
    would lose its low digits to its high ones, and a long run would then step
    unevenly."""
    generator, _ = made(lib, shape, frequency, 20000)

    place = lib.generate_get_phase(generator)

    assert 0.0 <= place < 1.0


@given(KINDS, FREQUENCIES, st.integers(min_value=1, max_value=64))
@RUNS
def test_a_block_is_the_samples_it_would_have_given_one_at_a_time(lib, kind,
                                                                  frequency,
                                                                  count):
    """A block is there for speed and must not be there for anything else. If
    the two parted, a caller could not mix them."""
    _, together = made(lib, kind, frequency, count)

    generator = lib.generate_make(kind)
    assert lib.generate_design(generator, sp.to_float32(frequency),
                               sp.to_float32(SAMPLE_RATE))
    lib.generate_set_seed(generator, 7)

    apart = [lib.generate_sample(generator) for _ in range(count)]

    assert together == apart


@given(KINDS, FREQUENCIES)
@RUNS
def test_the_same_seed_gives_the_same_signal_twice(lib, kind, frequency):
    """The noises must be repeatable, or a measurement made with them cannot
    be made again."""
    _, first = made(lib, kind, frequency, 500, seed=1234)
    _, second = made(lib, kind, frequency, 500, seed=1234)

    assert first == second


@given(NOISES, FREQUENCIES)
@RUNS
def test_two_seeds_give_two_different_noises(lib, noise, frequency):
    """A generator that gave the same noise whatever its seed would not be
    seeded at all, and two channels of it would be one channel twice."""
    _, first = made(lib, noise, frequency, 500, seed=1)
    _, second = made(lib, noise, frequency, 500, seed=99)

    assert first != second


@given(KINDS, FREQUENCIES)
@RUNS
def test_a_reset_generator_gives_what_a_new_one_gives(lib, kind, frequency):
    """The running parts of the pink noise are what is easy to leave behind,
    and a part left behind colours the whole of the next run."""
    generator = lib.generate_make(kind)
    assert lib.generate_design(generator, sp.to_float32(frequency),
                               sp.to_float32(SAMPLE_RATE))
    lib.generate_set_seed(generator, 5)

    block(lib, generator, 1000)

    lib.generate_reset(generator)
    lib.generate_set_seed(generator, 5)
    after_reset = block(lib, generator, 500)

    _, expected = made(lib, kind, frequency, 500, seed=5)

    assert after_reset == expected


@given(SHAPES)
@RUNS
def test_a_shape_crosses_nothing_as_often_as_its_frequency_says(lib, shape):
    """WHAT SAYS THE FREQUENCY IS THE FREQUENCY. Each of these shapes goes
    above nothing once a turn and below once, thus a run of a whole number of
    turns crosses nothing twice for each turn. Measuring this rather than a
    spectrum keeps the test free of any other module."""
    turns = 20
    frequency = 64.0
    count = int(turns * SAMPLE_RATE / frequency)

    _, values = made(lib, shape, frequency, count)

    crossings = 0
    for index in range(1, len(values)):
        if (values[index - 1] < 0.0) != (values[index] < 0.0):
            crossings += 1

    assert abs(crossings - (2 * turns)) <= 2


@given(SHAPES, FREQUENCIES)
@RUNS
def test_a_shape_spends_as_long_above_nothing_as_below(lib, shape, frequency):
    """These shapes carry no level of their own, thus over a whole number of
    turns what they add up to is nothing. A shape with a level in it would push
    that level into whatever it was added to and would not be taken out again.

    A WHOLE NUMBER OF TURNS AND NOT A FIXED COUNT OF SAMPLES. A run that stops
    part way through a turn leaves that part unbalanced, and at a low frequency
    the part is most of the run: measured at 1 Hz with a fixed count, a square
    wave added up to 0.024 with nothing wrong with it.

    AND THE RUN IS ALWAYS ABOUT THE SAME LENGTH, whatever the frequency. A turn
    holds a whole number of samples only where the rate divides by the
    frequency, thus at 3000 Hz the nearest whole number of samples is still
    part of a turn out. Making the run long leaves that part a small share of
    it; a run of twenty turns at 3000 Hz is 55 samples, and one sample of 55
    added up to 0.0056."""
    per_turn = SAMPLE_RATE / frequency
    count = int(round(per_turn * round(40000.0 / per_turn)))

    _, values = made(lib, shape, frequency, count)

    mean = sum(values) / len(values)

    assert abs(mean) < 0.005


@given(FREQUENCIES, FREQUENCIES)
@RUNS
def test_a_sweep_begins_and_ends_where_it_was_told_to(lib, start, finish):
    """A sweep is for measuring a system across a band. If it did not reach the
    frequency it was told to, the measurement would stop short of the band and
    nothing would say so.

    THE FREQUENCY IS READ FROM THE PHASE AND NOT FROM THE SAMPLES. How far the
    phase moves in one sample IS the frequency divided by the sample rate, thus
    reading it gives the answer exactly at any frequency. Counting how often
    the samples cross nothing gives the same answer only where the stretch
    counted over holds many turns, and at 1 Hz a short stretch holds none."""
    assume(start != finish)

    samples = 4000

    generator = lib.generate_make(sptk.GENERATE_SINE)
    assert lib.generate_design_sweep(generator, sp.to_float32(start),
                                     sp.to_float32(finish),
                                     sp.to_float32(SAMPLE_RATE), samples)

    def frequency_now():
        was = lib.generate_get_phase(generator)
        lib.generate_sample(generator)
        now = lib.generate_get_phase(generator)

        moved = now - was
        if moved < 0.0:
            moved += 1.0

        return moved * SAMPLE_RATE

    at_the_start = frequency_now()

    for _ in range(samples - 2):
        lib.generate_sample(generator)

    at_the_end = frequency_now()

    # One sample of sweep, because the frequency is read across a sample and
    # the sweep moves the step within it. At a sweep of 300 Hz down to 1 over
    # 4000 samples that is 0.075 Hz, which is the whole of what was measured.
    one_sample = abs(finish - start) / samples

    assert abs(at_the_start - start) <= 0.01 * start + one_sample + 0.01
    assert abs(at_the_end - finish) <= 0.01 * finish + one_sample + 0.01


@given(st.integers(min_value=-4, max_value=10))
def test_only_the_six_shapes_are_taken(lib, kind):
    assert lib.generate_is_valid_kind(kind) == (0 <= kind <= 5)


@given(st.floats(min_value=-8192.0, max_value=8192.0, width=32),
       st.floats(min_value=-100.0, max_value=8192.0, width=32))
def test_only_a_frequency_under_half_the_rate_is_taken(lib, frequency, rate):
    """A frequency at or above half the sample rate does not come out as
    itself: it folds and comes out as another frequency, and nothing in the
    samples says which one it was."""
    expected = (rate > 0.0) and (0.0 < frequency < (rate / 2.0))

    assert lib.generate_is_valid_frequency(sp.to_float32(frequency),
                                           sp.to_float32(rate)) == expected


@given(KINDS)
def test_a_generator_that_was_never_designed_gives_nothing(lib, kind):
    """Rather than a shape at some frequency nobody chose."""
    generator = lib.generate_make(kind)

    for _ in range(20):
        assert lib.generate_sample(generator) == 0.0
