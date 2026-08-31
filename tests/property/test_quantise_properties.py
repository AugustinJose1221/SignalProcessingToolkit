"""Rules that rounding a signal to a number of bits must keep.

The three ways differ in what they do with the error, not in where the steps
stand. What holds for all three is that the answer sits on a step and stays
inside the reach; what parts them is where the error goes.
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

WAYS = st.sampled_from([ffitt.QUANTISE_PLAIN, ffitt.QUANTISE_DITHER,
                        ffitt.QUANTISE_SHAPED])

BITS = st.integers(min_value=2, max_value=16)

# Reaches a float of 32 bits holds exactly.
REACHES = st.sampled_from([0.25, 1.0, 2.0, 8.0, 1024.0])


def designed(lib, way, bits, reach, seed=1):
    quantiser = lib.quantise_make()
    assert lib.quantise_design(quantiser, way, bits, sp.to_float32(reach))
    lib.quantise_set_seed(quantiser, seed)
    return quantiser


def through(lib, quantiser, values):
    given_values = ffitt.float_array(values)
    out = ffitt.real_buffer(len(values))
    assert lib.quantise_block(quantiser, given_values, out, len(values))
    return [out[index] for index in range(len(values))]


@given(WAYS, BITS, REACHES, st.lists(sp.elements(4.0), min_size=1,
                                     max_size=64))
@RUNS
def test_nothing_ever_leaves_the_reach(lib, way, bits, reach, values):
    """THE RULE THAT MATTERS MOST. A quantiser stands for a number of bits, and
    a value outside the reach has no pattern of bits to be. It must be held at
    the reach and never allowed to wrap, because a wrap turns the loudest
    sample into the quietest and sounds like a bang."""
    quantiser = designed(lib, way, bits, reach)

    for value in through(lib, quantiser, values):
        assert math.isfinite(value)
        assert -reach - 1e-4 <= value <= reach + 1e-4


@given(WAYS, BITS, REACHES, st.lists(sp.elements(4.0), min_size=1,
                                     max_size=64))
@RUNS
def test_every_answer_sits_on_a_step(lib, way, bits, reach, values):
    """Whatever a way does with the error, what comes out must be a value the
    bits can hold. An answer between two steps could not be written down."""
    quantiser = designed(lib, way, bits, reach)
    step = lib.quantise_step_of(quantiser)

    for value in through(lib, quantiser, values):
        steps = value / step
        assert abs(steps - round(steps)) <= 1e-3


@given(BITS, REACHES, st.lists(sp.elements(1.0), min_size=1, max_size=64))
@RUNS
def test_plain_rounding_never_moves_a_sample_by_half_a_step(lib, bits, reach,
                                                            values):
    """Plain rounding goes to the nearest step and to no other, thus the error
    is at most half a step. This is the bound the noise floor is worked out
    from, and a way that broke it would make that figure a fiction.

    Only samples inside the reach are examined, because a sample outside it is
    held at the reach on purpose and moves as far as it must."""
    quantiser = designed(lib, ffitt.QUANTISE_PLAIN, bits, reach)
    step = lib.quantise_step_of(quantiser)

    inside = [value for value in values if abs(value) < reach]
    assume(inside)

    for before, after in zip(inside, through(lib, quantiser, inside)):
        assert abs(after - before) <= (step / 2.0) + (1e-4 * step)


@given(WAYS, BITS, REACHES, st.lists(sp.elements(1.0), min_size=1,
                                     max_size=64))
@RUNS
def test_a_block_is_the_samples_it_would_have_given_one_at_a_time(lib, way,
                                                                  bits, reach,
                                                                  values):
    """The dither and the shaping both carry state from one sample to the next,
    thus a block that did not agree with the samples taken one at a time would
    be carrying that state differently."""
    together = through(lib, designed(lib, way, bits, reach), values)

    quantiser = designed(lib, way, bits, reach)
    apart = [lib.quantise_sample(quantiser, value) for value in values]

    assert together == apart


@given(WAYS, BITS, REACHES, st.lists(sp.elements(1.0), min_size=1,
                                     max_size=64))
@RUNS
def test_the_same_seed_gives_the_same_answer_twice(lib, way, bits, reach,
                                                   values):
    """The dither is random and must still be repeatable, or a measurement
    made through it cannot be made again."""
    first = through(lib, designed(lib, way, bits, reach, seed=42), values)
    second = through(lib, designed(lib, way, bits, reach, seed=42), values)

    assert first == second


@given(WAYS, BITS, REACHES, st.lists(sp.elements(1.0), min_size=1,
                                     max_size=64))
@RUNS
def test_a_reset_quantiser_answers_as_a_new_one_does(lib, way, bits, reach,
                                                     values):
    """The carried error of the shaping is what is easy to leave behind."""
    quantiser = designed(lib, way, bits, reach, seed=3)
    through(lib, quantiser, values)

    lib.quantise_reset(quantiser)
    lib.quantise_set_seed(quantiser, 3)
    after_reset = through(lib, quantiser, values)

    expected = through(lib, designed(lib, way, bits, reach, seed=3), values)

    assert after_reset == expected


@given(BITS, REACHES)
def test_one_more_bit_halves_the_step(lib, bits, reach):
    """The steps run across the whole reach, thus each bit added splits every
    step in two. Anything else and the count of bits does not mean what it
    says."""
    assume(bits < 16)

    coarse = lib.quantise_step_of(designed(lib, ffitt.QUANTISE_PLAIN, bits,
                                           reach))
    fine = lib.quantise_step_of(designed(lib, ffitt.QUANTISE_PLAIN, bits + 1,
                                         reach))

    assert abs((coarse / 2.0) - fine) <= 1e-6 * coarse


@given(BITS, REACHES)
def test_a_wider_reach_makes_a_larger_step_in_proportion(lib, bits, reach):
    """The bits say how many steps there are and the reach says how far they
    spread. Twice the reach over the same count of steps is twice the step,
    which is why more bits are needed to hold a louder signal as well."""
    narrow = lib.quantise_step_of(designed(lib, ffitt.QUANTISE_PLAIN, bits,
                                           reach))
    wide = lib.quantise_step_of(designed(lib, ffitt.QUANTISE_PLAIN, bits,
                                         reach * 2.0))

    assert abs((narrow * 2.0) - wide) <= 1e-6 * wide


@given(BITS, REACHES)
@RUNS
def test_dither_takes_the_error_off_the_signal(lib, bits, reach):
    """THE REASON DITHER EXISTS. Rounding a slow signal plainly gives an error
    that follows the signal, thus the error is a shape and not a hiss and a
    listener hears it as part of the sound. Dither makes the error even about
    nothing whatever the signal is doing, at the cost of a little more of it.

    Measured on a slow ramp that crosses many steps: rounded plainly, the mean
    error over a stretch that stays within one step is the whole of that step's
    offset. Dithered, it averages away."""
    step = reach / (1 << (bits - 1))

    # A ramp so slow that many samples in a row fall in the same step, which is
    # what makes the plain error lean.
    #
    # HOW SLOW MATTERS. A ramp that crosses exactly one step over the stretch
    # the lean is measured across sweeps the whole sawtooth of the plain error
    # within that stretch, thus the lean averages to nothing and the plain way
    # looks even. That is the measurement cancelling what it set out to see,
    # not the rounding being clean. This ramp crosses a quarter of a step over
    # the stretch, thus the samples in one stretch mostly share a step.
    count = 2000
    across = 64
    values = [sp.to_float32(-reach * 0.5 + (index * step / 256.0))
              for index in range(count)]
    assume(all(abs(value) < reach for value in values))

    plain = through(lib, designed(lib, ffitt.QUANTISE_PLAIN, bits, reach),
                    values)
    dithered = through(lib, designed(lib, ffitt.QUANTISE_DITHER, bits, reach,
                                     seed=7), values)

    def lean(answers):
        # How far the error leans over each short stretch, at its worst. A
        # leaning error is one that follows the signal.
        worst = 0.0
        for start in range(0, count - across, across):
            piece = [answers[index] - values[index]
                     for index in range(start, start + across)]
            worst = max(worst, abs(sum(piece) / len(piece)))
        return worst

    assert lean(dithered) < lean(plain)


@given(st.integers(min_value=1, max_value=24))
def test_the_noise_floor_falls_by_about_six_decibels_for_each_bit(lib, bits):
    """The figure a caller chooses a number of bits by. Each bit halves the
    step, and halving the step is six decibels."""
    assume(bits < 24)

    here = lib.quantise_noise_floor(bits)
    next_bit = lib.quantise_noise_floor(bits + 1)

    assert abs((here - next_bit) - 6.02) <= 0.01


@given(st.integers(min_value=-4, max_value=8))
def test_only_the_three_ways_are_taken(lib, way):
    assert lib.quantise_is_valid_way(way) == (0 <= way <= 2)


@given(st.integers(min_value=0, max_value=40))
def test_only_a_count_of_bits_the_step_can_be_worked_out_from_is_taken(lib,
                                                                       bits):
    """Below one bit there are no steps at all. Above the bound the step falls
    under what a float holds beside the reach, thus rounding to it would give
    the sample back unchanged and the quantiser would be doing nothing."""
    assert lib.quantise_is_valid_bits(bits) == (1 <= bits <= 24)


@given(WAYS, BITS, st.floats(min_value=-8.0, max_value=0.0, width=32))
def test_a_reach_of_nothing_or_below_it_is_refused(lib, way, bits, reach):
    """The step is the reach divided by the count of steps. A reach of nothing
    gives a step of nothing and every sample would round to nothing."""
    quantiser = lib.quantise_make()

    assert not lib.quantise_design(quantiser, way, bits, sp.to_float32(reach))


@given(sp.elements(4.0))
def test_a_quantiser_that_was_never_designed_gives_the_sample_back(lib,
                                                                   value):
    """Rather than rounding to a step nobody chose."""
    quantiser = lib.quantise_make()

    assert lib.quantise_sample(quantiser, value) == value
