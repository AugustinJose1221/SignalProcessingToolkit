"""Rules that taking the level off a reading must keep.

A level blocker is judged by two numbers and nothing else: HOW COMPLETELY IT
REMOVES A LEVEL, and HOW LITTLE IT TOUCHES THE SIGNAL ABOVE ITS CUTOFF. A filter
that did the first and not the second would take the measurement away with the
level; one that did the second and not the first would leave the thing it exists
to remove.

Everything below is one of those two, or a consequence of them.
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

TWO_PI = 2.0 * math.pi

# Cutoffs as a part of the sample rate, all held exactly by a float of 32 bits.
CUTOFFS = st.sampled_from([0.00390625, 0.0078125, 0.015625, 0.03125])


def through(lib, cutoff, values):
    dcblock = lib.dcblock_init(sp.to_float32(cutoff))
    out = sptk.real_buffer(len(values))

    lib.dcblock_process_block(dcblock, sptk.float_array(values), out,
                              len(values))

    return [out[index] for index in range(len(values))], dcblock


@given(CUTOFFS, sp.elements(1000.0))
@RUNS
def test_a_level_is_removed_completely(lib, cutoff, level):
    """THE FIRST OF THE TWO NUMBERS. A reading that never changes is all level
    and no signal, thus nothing at all may come out of it. Not almost nothing:
    a level that survived would be a level added to every answer for ever."""
    assume(abs(level) > 0.01)

    count = int(40.0 / cutoff)
    got, _ = through(lib, cutoff, [level] * count)

    # Once the tracker has caught up, which takes about one over the cutoff.
    settled = got[count // 2:]

    for value in settled:
        assert abs(value) <= 1e-3 * abs(level)


@given(CUTOFFS, st.sampled_from([0.1, 0.2, 0.3]),
       st.floats(min_value=0.25, max_value=4.0, width=32))
@RUNS
def test_a_signal_well_above_the_cutoff_keeps_the_gain_the_design_gives(
        lib, cutoff, frequency, height):
    """THE SECOND OF THE TWO NUMBERS, AND IT IS NOT ONE.

    A filter that removed the level and the measurement with it would be no
    use, thus a wave well above the cutoff must come through nearly whole. It
    does not come through EXACTLY whole, and the amount is worth knowing.

    The tracker writes its level from the sample that has just arrived and then
    takes it off, thus what comes out is (1 - p) times the difference between
    the sample and the level as it stood BEFORE. That leaves a passband gain of

        2 (1 - p) / (2 - p)        where p is 2 pi times the cutoff

    which climbs to one as the cutoff falls and is 0.975 at a cutoff of 0.0078.
    Measured, the formula holds to four decimal places at every cutoff tried.
    A caller reading a height through this filter at a wide cutoff is reading
    low by that much.

    MEASURED BY THE ROOT MEAN SQUARE AND NOT BY THE LARGEST SAMPLE. A sampled
    wave has a largest SAMPLE and not a largest value: at ten samples to a turn
    the nearest sample to the crest sits a twentieth below it, and that
    shortfall is the sampling and not the filter."""
    assume(frequency > (cutoff * 10.0))

    count = int(60.0 / cutoff)
    values = [sp.to_float32(height * math.sin(TWO_PI * frequency * index))
              for index in range(count)]

    got, _ = through(lib, cutoff, values)

    settled = got[count // 2:]

    loudness = math.sqrt(sum(value * value for value in settled)
                         / len(settled))
    measured = loudness * math.sqrt(2.0)

    pole = TWO_PI * cutoff
    wanted = height * 2.0 * (1.0 - pole) / (2.0 - pole)

    assert abs(measured - wanted) <= 0.03 * height


@given(CUTOFFS, st.sampled_from([0.1, 0.2]),
       st.floats(min_value=0.25, max_value=4.0, width=32),
       sp.elements(100.0))
@RUNS
def test_it_takes_the_level_off_and_leaves_the_wave(lib, cutoff, frequency,
                                                    height, level):
    """THE TWO TOGETHER, WHICH IS WHAT IT IS ACTUALLY ASKED TO DO. A wave
    sitting on a level: the level goes and the wave stays. Either alone is easy
    and neither alone is the job."""
    assume(frequency > (cutoff * 10.0))

    count = int(60.0 / cutoff)
    values = [sp.to_float32(level
                            + (height * math.sin(TWO_PI * frequency * index)))
              for index in range(count)]

    got, _ = through(lib, cutoff, values)

    settled = got[count // 2:]

    # The level has gone.
    mean = sum(settled) / len(settled)
    assert abs(mean) <= 0.02 * (1.0 + abs(level))

    # And the wave is still there, at the gain the design gives rather than at
    # exactly one, for the reason the test above sets out.
    loudness = math.sqrt(sum(value * value for value in settled)
                         / len(settled))

    pole = TWO_PI * cutoff
    wanted = height * 2.0 * (1.0 - pole) / (2.0 - pole)

    assert abs((loudness * math.sqrt(2.0)) - wanted) <= 0.05 * height


@given(CUTOFFS, sp.elements(100.0))
@RUNS
def test_the_tracker_follows_the_level_it_is_taking_off(lib, cutoff, level):
    """The level it reports is the thing it is subtracting, thus a caller
    watching a slow drift can read it from here rather than working it out
    again. It must therefore really be the level of the reading."""
    assume(abs(level) > 0.01)

    count = int(40.0 / cutoff)
    _, dcblock = through(lib, cutoff, [level] * count)

    assert abs(lib.dcblock_get_level(dcblock)
               - level) <= 1e-2 * (1.0 + abs(level))


@given(CUTOFFS, sp.elements(100.0), st.integers(1, 64))
@RUNS
def test_the_first_sample_sets_the_level_rather_than_being_a_step(lib, cutoff,
                                                                  level,
                                                                  seed):
    """WHAT A FILTER STARTED AT NOTHING WOULD DO INSTEAD. A reading that begins
    at 8000 would look to it like a step from nothing to 8000, and it would
    spend one over the cutoff recovering from a step that never happened. The
    first sample sets the level instead, thus the first answer is already
    about nothing."""
    assume(abs(level) > 1.0)

    got, _ = through(lib, cutoff, [level] * 8)

    assert abs(got[0]) <= 1e-3 * abs(level)


@given(CUTOFFS, st.integers(1, 64), st.floats(min_value=0.25, max_value=8.0,
                                              width=32))
@RUNS
def test_it_is_linear_in_the_signal(lib, cutoff, seed, louder):
    """It is a filter of the first order, thus twice the reading gives twice
    the answer."""
    state = seed
    values = []

    for _ in range(400):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        values.append(sp.to_float32((((state >> 16) % 20000) / 10000.0) - 1.0))

    louder_values = [sp.to_float32(value * louder) for value in values]

    plain, _ = through(lib, cutoff, values)
    scaled, _ = through(lib, cutoff, louder_values)

    for one, other in zip(plain, scaled):
        assert abs(other - (one * louder)) <= 1e-3 * (1.0 + abs(other))


@given(CUTOFFS, sp.elements(100.0))
@RUNS
def test_a_level_set_by_hand_is_the_level_it_takes_off(lib, cutoff, level):
    """A caller who already knows where the reading sits can say so and skip
    the settling. What is set must be what is used."""
    dcblock = lib.dcblock_init(sp.to_float32(cutoff))

    lib.dcblock_set_level(dcblock, sp.to_float32(level))

    assert abs(lib.dcblock_get_level(dcblock)
               - level) <= 1e-4 * (1.0 + abs(level))

    # A reading at exactly that level gives nothing at once.
    assert abs(lib.dcblock_process_sample(dcblock,
                                          sp.to_float32(level))) <= 1e-3 * (
                                              1.0 + abs(level))


@given(CUTOFFS, st.integers(1, 64))
@RUNS
def test_a_reset_filter_answers_as_a_new_one_does(lib, cutoff, seed):
    """The level and the flag that says whether the first sample has arrived
    are the whole of its memory."""
    state = seed
    values = []

    for _ in range(200):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        values.append(sp.to_float32(5.0 + ((((state >> 16) % 20000) / 10000.0)
                                           - 1.0)))

    dcblock = lib.dcblock_init(sp.to_float32(cutoff))

    for value in values:
        lib.dcblock_process_sample(dcblock, value)

    lib.dcblock_reset(dcblock)

    after = [lib.dcblock_process_sample(dcblock, value) for value in values]
    fresh, _ = through(lib, cutoff, values)

    assert after == fresh


@given(st.floats(min_value=-0.5, max_value=1.5, width=32))
def test_only_a_cutoff_the_width_can_hold_is_taken(lib, cutoff):
    """A cutoff of nothing never follows the level at all, and one at or above
    a half is above what a sampled signal can hold. The floor is set by the
    width, because the tracker's pole comes from the cutoff and a cutoff too
    small rounds that pole to exactly one."""
    smallest = 1e-6 if not sptk.REAL_64 else 1e-9

    assert lib.dcblock_is_valid_cutoff(sp.to_float32(cutoff)) == (
        smallest <= cutoff < 0.5)
