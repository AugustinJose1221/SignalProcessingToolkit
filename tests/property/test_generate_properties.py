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
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402

RUNS = settings(max_examples=40, deadline=None)

# The shapes that swing either way about nothing, thus the ones a rule about
# crossing nothing or adding up to nothing may be given.
SHAPES = st.sampled_from(ffitt.GENERATE_WAVES)

NOISES = st.sampled_from(ffitt.GENERATE_NOISES)

KINDS = st.sampled_from(ffitt.GENERATE_KINDS)

# The kinds held inside the range of one. Three are not: the gaussian noise
# runs as far as its tails go, the brown noise is a walk with no bound, and the
# blue noise is a difference and reaches further than what it is taken of.
BOUNDED = st.sampled_from(ffitt.GENERATE_BOUNDED)

# Parts a float of 32 bits holds exactly.
PARTS = st.sampled_from([0.0625, 0.125, 0.25, 0.5, 0.75, 0.875])

SAMPLE_RATE = 8192.0

# Frequencies a float of 32 bits holds exactly, well under half the rate.
FREQUENCIES = st.sampled_from([1.0, 7.0, 64.0, 300.0, 1000.0, 3000.0])


def block(lib, generator, count):
    out = ffitt.real_buffer(count)
    assert lib.generate_block(generator, out, count)
    return [out[index] for index in range(count)]


def made(lib, kind, frequency, count, seed=7, part=None):
    generator = lib.generate_make(kind)
    assert lib.generate_design(generator, sp.to_float32(frequency),
                               sp.to_float32(SAMPLE_RATE))
    lib.generate_set_seed(generator, seed)

    if part is not None:
        assert lib.generate_set_part(generator, sp.to_float32(part))

    return generator, block(lib, generator, count)


@given(BOUNDED, FREQUENCIES, PARTS)
@RUNS
def test_nothing_ever_leaves_the_range_of_one(lib, kind, frequency, part):
    """Every shape here is made to stand between minus one and one, and a
    caller scales from there. A shape that reached further would clip whatever
    it was scaled into, and the square wave did once reach two, and the pulse
    did once reach two at 64 bits when a sample landed exactly on a corner."""
    _, values = made(lib, kind, frequency, 4000, part=part)

    for value in values:
        assert math.isfinite(value)
        assert -1.0 - 1e-5 <= value <= 1.0 + 1e-5


@given(KINDS, FREQUENCIES, PARTS)
@RUNS
def test_every_kind_gives_numbers(lib, kind, frequency, part):
    """Held of the three that are not bounded as well. How far they reach is
    their own business; that every sample is a number is not."""
    _, values = made(lib, kind, frequency, 4000, part=part)

    for value in values:
        assert math.isfinite(value)


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

    generator = lib.generate_make(ffitt.GENERATE_SINE)
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


@given(st.integers(min_value=-4, max_value=20))
def test_only_the_kinds_that_exist_are_taken(lib, kind):
    assert (lib.generate_is_valid_kind(kind)
            == (0 <= kind <= ffitt.GENERATE_LAST_KIND))


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


@given(FREQUENCIES, PARTS)
@RUNS
def test_a_pulse_of_half_a_turn_is_the_square_wave(lib, frequency, part):
    """THE RULE THAT SAYS THE PULSE IS THE SHAPE IT CLAIMS TO BE. A pulse high
    for half of each turn IS the square wave, thus the two must agree sample
    for sample. If they did not, one of the two would be wrong about where its
    corners stand."""
    _, pulse = made(lib, ffitt.GENERATE_PULSE, frequency, 2000, part=0.5)
    _, square = made(lib, ffitt.GENERATE_SQUARE, frequency, 2000)

    for one, other in zip(pulse, square):
        assert abs(one - other) <= 1e-5


@given(FREQUENCIES, PARTS)
@RUNS
def test_a_pulse_is_high_for_the_part_it_was_given(lib, frequency, part):
    """The part of the turn it is high for is the whole of what the part
    means, and what a run adds up to is how to see it: high at one for that
    part and low at minus one for the rest."""
    turns = 40
    per_turn = SAMPLE_RATE / frequency
    count = int(round(per_turn * turns))

    _, values = made(lib, ffitt.GENERATE_PULSE, frequency, count, part=part)

    mean = sum(values) / len(values)
    expected = part - (1.0 - part)

    # One corner of the turn is smoothed across a sample either side, thus a
    # turn holding few samples spends a larger share of itself in the corners.
    room = 0.02 + (4.0 / per_turn)

    assert abs(mean - expected) <= room


@given(FREQUENCIES, PARTS)
@RUNS
def test_a_gaussian_pulse_is_a_bump_that_never_goes_below_nothing(
        lib, frequency, part):
    """It is a pulse and not an oscillation: a thing that happens rather than a
    thing that swings. A caller adding it to a reading is adding a bump, and a
    bump that dipped below nothing on its way would be two events and not
    one."""
    _, values = made(lib, ffitt.GENERATE_GAUSSIAN_PULSE, frequency, 4000,
                     part=part)

    for value in values:
        assert 0.0 <= value <= 1.0

    # And it really reaches the top somewhere, or it is not a bump at all.
    assert max(values) > 0.9


@given(FREQUENCIES, st.sampled_from([0.0625, 0.125, 0.25]))
@RUNS
def test_a_wider_gaussian_pulse_holds_more(lib, frequency, part):
    """The part is the width of the bump. A wider bump must carry more, or the
    parameter is not a width."""
    per_turn = SAMPLE_RATE / frequency
    count = int(round(per_turn * 20))

    _, narrow = made(lib, ffitt.GENERATE_GAUSSIAN_PULSE, frequency, count,
                     part=part)
    _, wide = made(lib, ffitt.GENERATE_GAUSSIAN_PULSE, frequency, count,
                   part=part * 2.0)

    assert sum(wide) > sum(narrow)


@given(FREQUENCIES)
@RUNS
def test_an_impulse_stands_once_each_turn_and_nowhere_else(lib, frequency):
    """One sample of one at the start of each turn and nothing between them.
    Any other value at all would mean it is not an impulse."""
    per_turn = SAMPLE_RATE / frequency
    turns = 20
    count = int(round(per_turn * turns))

    _, values = made(lib, ffitt.GENERATE_IMPULSE, frequency, count)

    standing = [index for index, value in enumerate(values) if value != 0.0]

    for index in standing:
        assert values[index] == 1.0

    # One for each whole turn the run holds, give or take the turn it ends
    # part way through.
    assert abs(len(standing) - turns) <= 1
    assert standing[0] == 0


@given(FREQUENCIES)
@RUNS
def test_the_gaussian_noise_really_has_the_tails_of_a_normal_spread(
        lib, frequency):
    """THE CLAIM THAT MAKES THIS KIND WORTH HAVING. A normal spread puts 4.55
    in every hundred past two standard deviations and 0.27 in every hundred
    past three. The even spread the module already had puts NONE past two, and
    the usual shortcut of adding a dozen even draws together has far too few
    past three. Everything in this library that turns a rate of false alarms
    into a threshold rests on those shares."""
    _, values = made(lib, ffitt.GENERATE_GAUSSIAN_NOISE, frequency, 200000)

    count = len(values)
    mean = sum(values) / count
    spread = math.sqrt(sum((value - mean) ** 2 for value in values) / count)

    assert abs(mean) < 0.02
    assert abs(spread - 1.0) < 0.02

    past_two = sum(1 for value in values if abs(value) > 2.0) / count
    past_three = sum(1 for value in values if abs(value) > 3.0) / count

    assert abs(past_two - 0.0455) < 0.005
    assert abs(past_three - 0.0027) < 0.001

    # And it must reach well past three, which no bounded spread does.
    assert max(abs(value) for value in values) > 4.0


@given(FREQUENCIES)
@RUNS
def test_the_brown_noise_wanders_and_the_blue_noise_jitters(lib, frequency):
    """The two new noises are opposites and must behave as opposites. How much
    a signal moves from one sample to the next is what its slope MEANS: a walk
    moves far less than the noise driving it, and a rising slope moves more
    than the falling one it is the mirror of."""
    count = 100000

    _, white = made(lib, ffitt.GENERATE_WHITE_NOISE, frequency, count, seed=11)
    _, brown = made(lib, ffitt.GENERATE_BROWN_NOISE, frequency, count, seed=11)
    _, pink = made(lib, ffitt.GENERATE_PINK_NOISE, frequency, count, seed=11)
    _, blue = made(lib, ffitt.GENERATE_BLUE_NOISE, frequency, count, seed=11)

    def moved(values):
        return sum((values[index] - values[index - 1]) ** 2
                   for index in range(1, len(values)))

    def loudness(values):
        return sum(value * value for value in values)

    # The walk moves far less from sample to sample than what drives it.
    assert moved(brown) < (0.05 * moved(white))

    # And it is no quieter overall, because what is added is scaled to keep
    # the spread the same.
    assert loudness(brown) > (0.5 * loudness(white))

    # The blue noise jitters more than the pink it is the mirror of, while
    # standing about as loud.
    assert moved(blue) > (2.0 * moved(pink))
    assert 0.5 < (loudness(blue) / loudness(pink)) < 2.0


@given(st.floats(min_value=-2.0, max_value=3.0, width=32))
def test_only_a_part_of_a_turn_that_leaves_two_corners_is_taken(lib, part):
    """A pulse filling none of the turn or the whole of it has no corners, and
    a shape with no corners is not a pulse."""
    assert lib.generate_is_valid_part(sp.to_float32(part)) == (0.0 < part < 1.0)


@given(KINDS, PARTS)
@RUNS
def test_a_part_that_is_refused_leaves_the_maker_as_it_was(lib, kind, part):
    """Refused rather than held at the nearest one it would take, because a
    caller that asked for a pulse filling the whole turn wanted something this
    module cannot make and should hear so."""
    generator = lib.generate_make(kind)

    assert lib.generate_set_part(generator, sp.to_float32(part))
    assert not lib.generate_set_part(generator, sp.to_float32(0.0))
    assert not lib.generate_set_part(generator, sp.to_float32(1.0))

    assert abs(lib.generate_get_part(generator) - part) <= 1e-6
