"""Rules that watching for a change must keep.

The module trades two things against each other: how often it is wrong and how
long it takes. Most of what follows holds that trade to what it says, and the
rest holds the arithmetic of the sums.
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

# Values a float of 32 bits holds exactly.
CHANGES = st.sampled_from([0.25, 0.5, 1.0, 2.0])
THRESHOLDS = st.sampled_from([1.0, 2.0, 4.0, 5.0, 8.0, 16.0])
SPREADS = st.sampled_from([0.125, 1.0, 8.0, 1024.0])
LEVELS = st.sampled_from([-1024.0, -1.0, 0.0, 1.0, 1024.0])


def watching(lib, level=0.0, spread=1.0, change=1.0, threshold=5.0):
    watcher = lib.changepoint_make()

    assert lib.changepoint_design(watcher, sp.to_float32(level),
                                  sp.to_float32(spread),
                                  sp.to_float32(change),
                                  sp.to_float32(threshold))

    return watcher


def noise(count, seed=1):
    """An even spread with a standard deviation of one."""
    state = seed
    out = []

    for _ in range(count):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        even = (((state >> 16) % 20000) / 10000.0) - 1.0
        out.append(sp.to_float32(even * 1.7320508))

    return out


@given(CHANGES, THRESHOLDS, SPREADS, LEVELS)
@RUNS
def test_neither_sum_ever_falls_below_nothing(lib, change, threshold, spread,
                                              level):
    """THE RULE THE ANSWER RESTS ON. The sums are held at nothing from below so
    that a long quiet spell cannot build up credit for a change that comes
    later. A sum let go below nothing would make the alarm arrive sooner after
    a long quiet than after a short one, and the same alarm would then mean two
    different things."""
    watcher = watching(lib, level, spread, change, threshold)

    values = noise(2000)

    for value in values:
        lib.changepoint_process_sample(watcher,
                                       sp.to_float32(level
                                                     + (spread * value)))

        assert lib.changepoint_running_high(watcher) >= 0.0
        assert lib.changepoint_running_low(watcher) >= 0.0
        assert math.isfinite(lib.changepoint_running_high(watcher))
        assert math.isfinite(lib.changepoint_running_low(watcher))


@given(CHANGES, THRESHOLDS, SPREADS, LEVELS)
@RUNS
def test_a_reading_exactly_where_it_should_be_moves_neither_sum(lib, change,
                                                                threshold,
                                                                spread, level):
    """Every sample takes half the smallest change off both sums. With the
    reading standing exactly where it was told to expect it, there is nothing
    to put back, thus both sums sit at nothing however long it runs."""
    watcher = watching(lib, level, spread, change, threshold)

    for _ in range(500):
        assert (lib.changepoint_process_sample(watcher, sp.to_float32(level))
                == sptk.CHANGEPOINT_NONE)

    assert lib.changepoint_running_high(watcher) == 0.0
    assert lib.changepoint_running_low(watcher) == 0.0


@given(CHANGES, THRESHOLDS, SPREADS, LEVELS,
       st.sampled_from([1.5, 2.0, 4.0, 8.0]))
@RUNS
def test_a_step_large_enough_is_always_found_and_in_the_time_promised(
        lib, change, threshold, spread, level, size):
    """THE REASON THE MODULE EXISTS, and the promise changepoint_delay_for
    makes. A clean step of a known size gains the sum a known amount at every
    sample, thus how long it takes is arithmetic and not luck."""
    assume(size > (change / 2.0))

    watcher = watching(lib, level, spread, change, threshold)

    promised = lib.changepoint_delay_for(watcher, sp.to_float32(size))
    assert promised > 0.0

    at = sp.to_float32(level + (spread * size))

    took = 0
    said = sptk.CHANGEPOINT_NONE

    while (said == sptk.CHANGEPOINT_NONE) and (took < 10000):
        took += 1
        said = lib.changepoint_process_sample(watcher, at)

    assert said == sptk.CHANGEPOINT_ROSE

    # The promise is how many samples of gain are needed. The alarm comes on
    # the first whole sample at or past that, thus the count is the promise
    # rounded up.
    assert took == math.ceil(promised - 1e-6)
    assert lib.changepoint_began_ago(watcher) == took


@given(CHANGES, THRESHOLDS, SPREADS, LEVELS,
       st.sampled_from([1.5, 2.0, 4.0, 8.0]))
@RUNS
def test_a_step_downwards_is_told_from_one_upwards(lib, change, threshold,
                                                   spread, level, size):
    """A rise and a fall usually mean different things about the thing being
    watched, and they take the same time to find."""
    assume(size > (change / 2.0))

    watcher = watching(lib, level, spread, change, threshold)

    at = sp.to_float32(level - (spread * size))

    took = 0
    said = sptk.CHANGEPOINT_NONE

    while (said == sptk.CHANGEPOINT_NONE) and (took < 10000):
        took += 1
        said = lib.changepoint_process_sample(watcher, at)

    assert said == sptk.CHANGEPOINT_FELL
    assert took == math.ceil(lib.changepoint_delay_for(watcher,
                                                       sp.to_float32(size))
                             - 1e-6)


@given(CHANGES, THRESHOLDS, SPREADS, LEVELS)
@RUNS
def test_the_same_reading_in_other_units_behaves_the_same_way(lib, change,
                                                              threshold,
                                                              spread, level):
    """THE CLAIM THAT MAKES THE THRESHOLD MEAN ANYTHING. The sums are worked
    out in units of how far the reading wanders, thus a reading in millivolts
    and the same reading in volts must give the same alarms at the same
    samples. A threshold that had to be chosen afresh for each reading would
    carry none of the meaning the table in the header gives it.

    THE CLAIM IS ABOUT THE ARITHMETIC AND NOT ABOUT THE WIDTH. Every sample is
    formed as the level plus the spread multiplied by the noise, and the first
    thing the watcher does is take the level off again. Where the level is far
    larger than the spread, most of the digits of the sample are spent holding
    the level and the difference comes back rough: at 32 bits with a level of
    -1024 and a spread of 0.125, what is left is out by about two ten
    thousandths, which is enough to change which sample a sum crosses the
    threshold on. That is the same loss clean.c is built around, and it belongs
    to the caller and not to this module. The bound below keeps the two far
    enough apart for the width to carry both."""
    room = 1000.0 if not sptk.REAL_64 else 1.0e9
    assume(abs(level) <= (room * spread))

    plain = watching(lib, 0.0, 1.0, change, threshold)
    scaled = watching(lib, level, spread, change, threshold)

    values = noise(1500, seed=13)

    for value in values:
        one = lib.changepoint_process_sample(plain, value)
        other = lib.changepoint_process_sample(
            scaled, sp.to_float32(level + (spread * value)))

        # The alarms must agree exactly, because that is what a caller acts on.
        assert one == other

        # The sums are given more room, and the room is not slack. Working out
        # how far a sample of 1024.6 stands from a level of 1024 throws away
        # most of the digits of a float of 32 bits before the answer is even
        # divided by the spread. What is left is a reading whose sums walk a
        # little differently from the same reading held about nothing.
        assert abs(lib.changepoint_running_high(plain)
                   - lib.changepoint_running_high(scaled)) <= 0.02
        assert abs(lib.changepoint_running_low(plain)
                   - lib.changepoint_running_low(scaled)) <= 0.02


@given(CHANGES, SPREADS, LEVELS, st.sampled_from([1.0, 2.0, 4.0]))
@RUNS
def test_a_higher_threshold_is_never_quicker(lib, change, spread, level, size):
    """The trade, held to. Asking to be wrong less often cannot also be
    quicker, and a module where it was would be giving something away free."""
    assume(size > (change / 2.0))

    low = watching(lib, level, spread, change, 4.0)
    high = watching(lib, level, spread, change, 16.0)

    assert (lib.changepoint_delay_for(high, sp.to_float32(size))
            > lib.changepoint_delay_for(low, sp.to_float32(size)))


@given(CHANGES, THRESHOLDS)
@RUNS
def test_a_change_too_small_to_find_is_said_to_be_too_small(lib, change,
                                                            threshold):
    """A change of less than half the smallest change looked for gives the sum
    less at every sample than is taken off it, thus the sum drifts down to
    nothing and the alarm never comes. The module says 0 rather than a number
    of samples that would never arrive."""
    watcher = watching(lib, 0.0, 1.0, change, threshold)

    small = sp.to_float32(change / 4.0)

    assert lib.changepoint_delay_for(watcher, small) == 0.0

    at = small

    for _ in range(2000):
        assert (lib.changepoint_process_sample(watcher, at)
                == sptk.CHANGEPOINT_NONE)


@given(CHANGES, THRESHOLDS, st.sampled_from([1.0, 2.0, 4.0]))
@RUNS
def test_how_long_it_takes_does_not_depend_on_the_way_the_change_went(
        lib, change, threshold, size):
    """The two sums are mirrors of each other."""
    assume(size > (change / 2.0))

    watcher = watching(lib, 0.0, 1.0, change, threshold)

    assert (lib.changepoint_delay_for(watcher, sp.to_float32(size))
            == lib.changepoint_delay_for(watcher, sp.to_float32(-size)))


@given(CHANGES, THRESHOLDS, SPREADS, LEVELS)
@RUNS
def test_a_reset_watcher_answers_as_a_new_one_does(lib, change, threshold,
                                                   spread, level):
    """A sum left standing from a run before would carry the next alarm
    forward, and the caller would have no way to tell."""
    watcher = watching(lib, level, spread, change, threshold)
    fresh = watching(lib, level, spread, change, threshold)

    values = [sp.to_float32(level + (spread * value))
              for value in noise(300, seed=5)]

    for value in values:
        lib.changepoint_process_sample(watcher, value)

    lib.changepoint_reset(watcher)

    for value in values:
        assert (lib.changepoint_process_sample(watcher, value)
                == lib.changepoint_process_sample(fresh, value))
        assert (lib.changepoint_running_high(watcher)
                == lib.changepoint_running_high(fresh))

    # And what it was told about the reading is kept.
    assert watcher.designed


@given(CHANGES, THRESHOLDS, st.sampled_from([2.0, 4.0]))
@RUNS
def test_a_change_still_running_is_reported_again_after_the_same_wait(
        lib, change, threshold, size):
    """The sums start again after an alarm, so that a change that is still
    there gives the next alarm after the same delay rather than at every sample
    from then on. The count of alarms across a run is therefore the run divided
    by the delay."""
    assume(size > (change / 2.0))

    watcher = watching(lib, 0.0, 1.0, change, threshold)

    delay = math.ceil(lib.changepoint_delay_for(watcher, sp.to_float32(size))
                      - 1e-6)
    assume(delay >= 1)

    run = delay * 7
    at = sp.to_float32(size)

    alarms = 0

    for _ in range(run):
        if lib.changepoint_process_sample(watcher, at) != sptk.CHANGEPOINT_NONE:
            alarms += 1

    assert alarms == 7


@given(st.floats(min_value=-4.0, max_value=4.0, width=32))
def test_only_a_reading_that_wanders_can_be_watched(lib, spread):
    """A reading that never wanders shows any change in one sample and needs
    none of this.

    The bound is the smallest number the width holds and not nothing, because
    every sample is DIVIDED by the spread: a spread smaller than that turns an
    ordinary sample into a number the width cannot hold."""
    smallest = sys.float_info.min if sptk.REAL_64 else 1.1754943508222875e-38

    assert (lib.changepoint_is_valid_deviation(sp.to_float32(spread))
            == (spread > smallest))


@given(st.floats(min_value=-4.0, max_value=4.0, width=32))
def test_only_a_change_and_a_threshold_above_nothing_are_taken(lib, value):
    """A change of nothing is found everywhere; a threshold of nothing is
    crossed by the first sample."""
    expected = value > 0.0

    assert lib.changepoint_is_valid_change(sp.to_float32(value)) == expected
    assert lib.changepoint_is_valid_threshold(sp.to_float32(value)) == expected


@given(sp.elements(1000.0))
def test_a_watcher_that_was_never_designed_says_nothing(lib, value):
    """Rather than answering about a reading nobody described."""
    watcher = lib.changepoint_make()

    for _ in range(50):
        assert (lib.changepoint_process_sample(watcher, value)
                == sptk.CHANGEPOINT_NONE)
