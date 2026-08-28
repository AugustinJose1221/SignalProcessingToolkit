"""Rules that a loop following a tone must keep.

A loop can be wrong in ways a transform cannot: it must be started near the
answer, it can settle onto something that is not there, and it takes time to
arrive. Most of what follows holds the loop to the promises it makes about
those three.
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
TONE = 64.0

# Values a float of 32 bits holds exactly.
BANDWIDTHS = st.sampled_from([0.001953125, 0.00390625, 0.0078125, 0.015625])
DAMPINGS = st.sampled_from([0.5, 0.707, 1.0, 2.0])


def noise(state):
    state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
    return state, ((((state >> 16) % 20000) / 10000.0) - 1.0)


def designed(lib, bandwidth, damping=0.707, frequency=TONE):
    loop = lib.pll_make()

    assert lib.pll_design(loop, sp.to_float32(frequency), sp.to_float32(RATE),
                          sp.to_float32(bandwidth), sp.to_float32(damping))

    return loop


def follow(lib, loop, frequency, count, level=1.0, noise_level=0.0, seed=7):
    """Run a tone through and give the mean frequency over the second half."""
    state = seed
    phase = 0.0
    total = 0.0
    used = 0

    for index in range(count):
        phase += frequency / RATE
        value = level * math.sin(TWO_PI * phase)

        if noise_level > 0.0:
            state, drawn = noise(state)
            value += noise_level * drawn

        lib.pll_process_sample(loop, sp.to_float32(value))

        if index >= (count // 2):
            total += lib.pll_get_frequency(loop, sp.to_float32(RATE))
            used += 1

    return total / used


@given(BANDWIDTHS, DAMPINGS, st.sampled_from([-3.0, -1.0, 0.0, 1.0, 3.0]))
@RUNS
def test_the_loop_walks_to_where_the_tone_really_is(lib, bandwidth, damping,
                                                    offset):
    """THE REASON THE MODULE EXISTS. The loop is told to look near one
    frequency and given another, and it must end up on the one it was given
    rather than the one it was told."""
    # The offset is kept inside the pull range, which is about the bandwidth.
    assume(abs(offset) < (bandwidth * RATE))

    loop = designed(lib, bandwidth, damping)
    wanted = TONE + offset

    found = follow(lib, loop, wanted, 60000)

    assert abs(found - wanted) < 0.3
    assert lib.pll_lock_quality(loop) > 0.8


@given(BANDWIDTHS, DAMPINGS)
@RUNS
def test_a_loop_given_nothing_says_it_found_nothing(lib, bandwidth, damping):
    """THE NUMBER THAT MUST BE READ. A loop given noise and no tone settles
    somewhere and reports a frequency exactly as confidently as it reports a
    real one. Nothing but the lock quality tells the two apart."""
    loop = designed(lib, bandwidth, damping)

    state = 5

    for _ in range(60000):
        state, drawn = noise(state)
        lib.pll_process_sample(loop, sp.to_float32(drawn))

    assert lib.pll_lock_quality(loop) < 0.5

    # It still reports a frequency, which is the trap.
    assert math.isfinite(lib.pll_get_frequency(loop, sp.to_float32(RATE)))


@given(BANDWIDTHS, DAMPINGS,
       st.sampled_from([0.015625, 0.25, 1.0, 8.0, 64.0]))
@RUNS
def test_how_loud_the_tone_is_does_not_change_what_the_loop_finds(
        lib, bandwidth, damping, level):
    """THE CLAIM THE LOUDNESS MEASURE MAKES. Without it the gain of the loop
    would be the gain asked for MULTIPLIED BY the loudness of whatever arrived:
    a quiet tone would never arrive and a loud one would be unstable, and the
    bandwidth would be a number about the signal rather than about the loop."""
    loop = designed(lib, bandwidth, damping)
    wanted = TONE + 1.0

    assume(abs(1.0) < (bandwidth * RATE))

    found = follow(lib, loop, wanted, 60000, level=level)

    assert abs(found - wanted) < 0.3
    assert lib.pll_lock_quality(loop) > 0.8


@given(BANDWIDTHS, DAMPINGS)
@RUNS
def test_the_phase_never_leaves_its_turn(lib, bandwidth, damping):
    """The phase is carried and folded, thus it never grows and never loses its
    digits however long the run. A phase let grow would step unevenly after a
    while and the loop would drift for no reason at all."""
    loop = designed(lib, bandwidth, damping)

    state = 3
    phase = 0.0

    for index in range(20000):
        phase += TONE / RATE
        state, drawn = noise(state)

        lib.pll_process_sample(loop, sp.to_float32(
            math.sin(TWO_PI * phase) + drawn))

        found = lib.pll_get_phase(loop)

        assert math.isfinite(found)
        assert 0.0 <= found < 1.0


@given(BANDWIDTHS, DAMPINGS)
@RUNS
def test_the_lock_quality_never_leaves_the_range_of_one(lib, bandwidth,
                                                        damping):
    """It is a share, thus it stands between nothing and one whatever it is
    given. A caller deciding whether to believe a frequency judges it against a
    fixed number."""
    loop = designed(lib, bandwidth, damping)

    state = 9
    phase = 0.0

    for index in range(20000):
        phase += TONE / RATE
        state, drawn = noise(state)

        value = math.sin(TWO_PI * phase) if (index % 3) else (5.0 * drawn)

        lib.pll_process_sample(loop, sp.to_float32(value))

        found = lib.pll_lock_quality(loop)

        assert math.isfinite(found)
        assert 0.0 <= found <= 1.0


@given(DAMPINGS)
@RUNS
def test_a_wider_loop_wanders_more(lib, damping):
    """THE WHOLE OF THE TRADE, held in the numbers. A wide loop follows a
    change quickly and lets noise into the answer; there is no setting that
    does both."""
    wander = []

    for bandwidth in (0.001953125, 0.0078125, 0.015625):
        loop = designed(lib, bandwidth, damping)

        state = 11
        phase = 0.0
        total = 0.0
        squared = 0.0
        used = 0

        for index in range(60000):
            phase += TONE / RATE
            state, drawn = noise(state)

            lib.pll_process_sample(loop, sp.to_float32(
                math.sin(TWO_PI * phase) + drawn))

            if index >= 30000:
                found = lib.pll_get_frequency(loop, sp.to_float32(RATE))
                total += found
                squared += found * found
                used += 1

        mean = total / used
        wander.append(math.sqrt(max(0.0, (squared / used) - (mean * mean))))

    for index in range(1, len(wander)):
        assert wander[index] > wander[index - 1]


@given(BANDWIDTHS, DAMPINGS)
@RUNS
def test_a_reset_loop_answers_as_a_new_one_does(lib, bandwidth, damping):
    """A frequency left over from a run before would send the next run off
    towards it, and a caller measuring from the start would never know."""
    loop = designed(lib, bandwidth, damping)
    fresh = designed(lib, bandwidth, damping)

    follow(lib, loop, TONE + 2.0, 20000)
    lib.pll_reset(loop)

    one = follow(lib, loop, TONE, 20000)
    other = follow(lib, fresh, TONE, 20000)

    assert abs(one - other) <= 1e-3 * (1.0 + abs(other))

    # And what it was designed with is kept.
    assert abs(loop.bandwidth - bandwidth) <= 1e-6


@given(st.floats(min_value=-0.5, max_value=0.5, width=32))
def test_only_a_bandwidth_that_leaves_the_loop_slower_than_its_ripple(
        lib, bandwidth):
    """The detector gives the error wanted and a ripple at twice the tone on
    top of it, and the loop leans on being too slow to follow that ripple."""
    assert (lib.pll_is_valid_bandwidth(sp.to_float32(bandwidth))
            == (0.0 < bandwidth <= 0.05))


@given(st.floats(min_value=-1.0, max_value=8.0, width=32))
def test_only_a_damping_that_neither_rings_nor_crawls(lib, damping):
    assert (lib.pll_is_valid_damping(sp.to_float32(damping))
            == (0.1 <= damping <= 4.0))


@given(BANDWIDTHS, DAMPINGS)
@RUNS
def test_the_loop_says_how_far_it_reaches_and_how_long_it_takes(lib,
                                                                bandwidth,
                                                                damping):
    """Both are rough and the header says so, but they must at least follow the
    bandwidth: a wider loop reaches further and arrives sooner."""
    loop = designed(lib, bandwidth, damping)

    assert abs(lib.pll_pull_range(loop) - bandwidth) <= 1e-6
    assert lib.pll_settle_samples(loop) == int(2.0 / bandwidth)


@given(BANDWIDTHS)
@RUNS
def test_the_tone_it_gives_back_is_the_tone_that_was_there(lib, bandwidth):
    """What comes out holds the frequency and the phase of what arrived and
    none of its noise, which is what recovering a carrier means."""
    loop = designed(lib, bandwidth)

    state = 3
    phase = 0.0
    together = 0.0
    mine = 0.0
    theirs = 0.0

    for index in range(60000):
        phase += TONE / RATE
        clean = math.sin(TWO_PI * phase)

        state, drawn = noise(state)

        got = lib.pll_process_sample(loop, sp.to_float32(clean + drawn))

        if index >= 30000:
            together += got * clean
            mine += got * got
            theirs += clean * clean

    alike = together / math.sqrt(mine * theirs)

    assert alike > 0.9
