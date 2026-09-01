"""Rules that the lattice filter must keep.

Each stage takes out what the stages before it could already explain, and its
reflection coefficient says how much of one stage is left in the next. The
bounds on that coefficient are what keeps the whole chain from running away,
thus most of these tests are about it.
"""

import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402

RUNS = settings(max_examples=30, deadline=None)

SAMPLES = 500

STAGES = st.integers(min_value=1, max_value=8)

# Values a float of 32 bits holds exactly, so that the strategy asks for the
# same numbers at either width.
RATES = st.sampled_from([0.001953125, 0.0078125, 0.03125, 0.125, 0.5, 1.0])
FORGETTING = st.sampled_from([0.5, 0.9375, 0.984375, 0.99609375, 1.0])

LARGEST_REFLECTION = 0.99


def noise(count, seed=1):
    """A repeatable rough signal, made here rather than by the library."""
    state = seed
    out = []

    for _ in range(count):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        out.append(sp.to_float32((((state >> 16) % 2000) / 1000.0) - 1.0))

    return out


def path(reference, taps):
    out = []

    for index in range(len(reference)):
        total = 0.0
        for tap, weight in enumerate(taps):
            if index >= tap:
                total += weight * reference[index - tap]
        out.append(sp.to_float32(total))

    return out


def run(lib, stages, rate, forgetting, reference, wanted):
    filt = lib.lattice_alloc(stages)
    assert lib.lattice_design(filt, sp.to_float32(rate),
                              sp.to_float32(forgetting))

    left = [lib.lattice_process_sample(filt, reference[index], wanted[index])
            for index in range(len(reference))]

    return filt, left


@given(STAGES, RATES, FORGETTING)
@RUNS
def test_no_reflection_ever_reaches_the_bound(lib, stages, rate, forgetting):
    """THE RULE THAT KEEPS THE CHAIN STANDING. A reflection of one means a
    stage that sends all of its input on to the next, and the chain then has a
    pole on the circle. The module holds every one of them inside the bound,
    whatever the signal did."""
    reference = noise(SAMPLES)
    wanted = path(reference, [0.6, -0.3, 0.2])

    filt, left = run(lib, stages, rate, forgetting, reference, wanted)

    try:
        for stage in range(stages):
            value = lib.lattice_get_reflection(filt, stage)

            assert math.isfinite(value)
            assert abs(value) <= LARGEST_REFLECTION + 1e-6

        for value in left:
            assert math.isfinite(value)
    finally:
        lib.lattice_free(filt)


@given(STAGES, RATES, FORGETTING)
@RUNS
def test_the_error_after_a_step_is_no_larger_than_the_one_before(lib, stages,
                                                                 rate,
                                                                 forgetting):
    """A PRIORI AND A POSTERIORI. The step is taken to make what is left
    smaller. A step that made it larger has overshot, and a chain of stages
    each overshooting is how this filter fails."""
    reference = noise(SAMPLES)
    wanted = path(reference, [0.6, -0.3, 0.2])

    filt, _ = run(lib, stages, rate, forgetting, reference, wanted)

    try:
        before = lib.lattice_error_before(filt)
        after = lib.lattice_error_after(filt)

        assert abs(after) <= abs(before) + 1e-4
    finally:
        lib.lattice_free(filt)


@given(STAGES, RATES, FORGETTING)
@RUNS
def test_every_sample_leaves_the_step_bounded(lib, stages, rate, forgetting):
    """Held over the whole run and not only at its end, because the sample the
    step overshoots on is the first one, when the energies are still small."""
    reference = noise(SAMPLES)
    wanted = path(reference, [0.5, 0.25])

    filt = lib.lattice_alloc(stages)

    try:
        assert lib.lattice_design(filt, sp.to_float32(rate),
                                  sp.to_float32(forgetting))

        for index in range(SAMPLES):
            lib.lattice_process_sample(filt, reference[index], wanted[index])

            assert (abs(lib.lattice_error_after(filt))
                    <= abs(lib.lattice_error_before(filt)) + 1e-4)
    finally:
        lib.lattice_free(filt)


@given(RATES, FORGETTING)
@RUNS
def test_a_chain_long_enough_follows_what_it_was_shown(lib, rate, forgetting):
    """THE REASON THE MODULE EXISTS. What is left over at the end of a run must
    be smaller than what was wanted, or the filter learned nothing."""
    taps = [0.6, -0.3, 0.2]
    reference = noise(SAMPLES * 6)
    wanted = path(reference, taps)

    filt, left = run(lib, 6, rate, forgetting, reference, wanted)

    try:
        last = len(left) // 4

        loud = math.sqrt(sum(value * value for value in wanted[-last:])
                         / last)
        over = math.sqrt(sum(value * value for value in left[-last:]) / last)

        assert over < loud
    finally:
        lib.lattice_free(filt)


@given(STAGES, RATES, FORGETTING)
@RUNS
def test_the_same_signal_twice_gives_the_same_answer(lib, stages, rate,
                                                     forgetting):
    """Nothing may be carried between runs."""
    reference = noise(SAMPLES)
    wanted = path(reference, [0.5, 0.25])

    first, one = run(lib, stages, rate, forgetting, reference, wanted)
    second, other = run(lib, stages, rate, forgetting, reference, wanted)

    try:
        assert one == other
    finally:
        lib.lattice_free(first)
        lib.lattice_free(second)


@given(STAGES, RATES, FORGETTING)
@RUNS
def test_a_reset_filter_answers_as_a_new_one_does(lib, stages, rate,
                                                  forgetting):
    """The energies are the part easy to leave behind, and an energy left
    behind makes the next run take smaller steps than a new filter would."""
    reference = noise(SAMPLES)
    wanted = path(reference, [0.5, 0.25])

    filt = lib.lattice_alloc(stages)

    try:
        assert lib.lattice_design(filt, sp.to_float32(rate),
                                  sp.to_float32(forgetting))

        for index in range(SAMPLES):
            lib.lattice_process_sample(filt, reference[index], wanted[index])

        lib.lattice_reset(filt)

        after_reset = [lib.lattice_process_sample(filt, reference[index],
                                                  wanted[index])
                       for index in range(SAMPLES)]

        fresh, expected = run(lib, stages, rate, forgetting, reference, wanted)
        lib.lattice_free(fresh)

        assert after_reset == expected
    finally:
        lib.lattice_free(filt)


@given(STAGES, RATES, FORGETTING)
@RUNS
def test_a_reference_of_nothing_leaves_the_reflections_where_they_were(
        lib, stages, rate, forgetting):
    """With nothing coming in there is nothing for a stage to explain. A
    reflection that moved would be following the floor that keeps the division
    safe rather than a signal."""
    filt = lib.lattice_alloc(stages)

    try:
        assert lib.lattice_design(filt, sp.to_float32(rate),
                                  sp.to_float32(forgetting))

        for _ in range(200):
            left = lib.lattice_process_sample(filt, sp.to_float32(0.0),
                                              sp.to_float32(1.0))
            assert math.isfinite(left)

        for stage in range(stages):
            assert abs(lib.lattice_get_reflection(filt, stage)) <= 1e-6
    finally:
        lib.lattice_free(filt)


@given(st.floats(min_value=-2.0, max_value=4.0, width=32))
def test_only_a_rate_above_nothing_and_within_the_bound_is_taken(lib, rate):
    """A rate of nothing never learns; a rate above the bound moves further
    than the whole of the error it was correcting."""
    expected = 0.0 < rate <= 1.0

    assert lib.lattice_is_valid_rate(sp.to_float32(rate)) == expected


@given(st.floats(min_value=-2.0, max_value=4.0, width=32))
def test_only_forgetting_above_nothing_and_within_one_is_taken(lib,
                                                               forgetting):
    """Forgetting above one would weigh the past more than the present."""
    expected = 0.0 < forgetting <= 1.0

    assert (lib.lattice_is_valid_forgetting(sp.to_float32(forgetting))
            == expected)


@given(STAGES, RATES, FORGETTING, sp.elements(4.0))
@RUNS
def test_the_answer_scales_with_what_was_wanted(lib, stages, rate, forgetting,
                                                scale):
    """The reflections describe the reference alone, thus making what is
    wanted twice as loud must make what is left twice as loud and leave the
    reflections where they were."""
    assume(abs(scale) > 0.25)

    reference = noise(SAMPLES)
    wanted = path(reference, [0.5, 0.25])
    louder = [sp.to_float32(value * scale) for value in wanted]

    plain, one = run(lib, stages, rate, forgetting, reference, wanted)
    big, other = run(lib, stages, rate, forgetting, reference, louder)

    try:
        for index in range(stages):
            assert abs(lib.lattice_get_reflection(plain, index)
                       - lib.lattice_get_reflection(big, index)) <= 1e-4

        for index in range(SAMPLES):
            room = 1e-3 * (1.0 + abs(scale) * (1.0 + abs(one[index])))
            assert abs(other[index] - (one[index] * scale)) <= room
    finally:
        lib.lattice_free(plain)
        lib.lattice_free(big)
