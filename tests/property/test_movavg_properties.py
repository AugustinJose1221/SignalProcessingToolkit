"""Rules that a moving mean must keep.

A moving mean is the simplest filter there is and it has one exact definition:
the answer at each sample IS the mean of the last so many samples. Everything
here is held against that definition rather than against what the module
happens to compute.

The rule worth knowing beyond it is the one nobody expects: a moving mean of n
samples REMOVES ANY WAVE WHOSE PERIOD DIVIDES n EXACTLY, completely and not
nearly. That is why a mean of 50 samples at 1000 a second is the answer to mains
hum, and it is the reason to choose one length over another.
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

SIZES = st.integers(min_value=2, max_value=32)


def through(lib, size, values):
    movavg = lib.movavg_alloc(size)

    try:
        out = sptk.real_buffer(len(values))

        lib.movavg_process_block(movavg, sptk.float_array(values), out,
                                 len(values))

        return [out[index] for index in range(len(values))]
    finally:
        lib.movavg_free(movavg)


def noise(count, seed=1):
    state = seed
    out = []

    for _ in range(count):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        out.append(sp.to_float32((((state >> 16) % 20000) / 10000.0) - 1.0))

    return out


@given(SIZES, st.integers(1, 64))
@RUNS
def test_the_answer_is_the_mean_of_the_last_samples(lib, size, seed):
    """THE DEFINITION, held sample by sample against the mean worked out here.

    Once the window is full the answer is the mean of the last `size` samples
    and nothing else. The module keeps a running total rather than adding them
    up afresh, thus a fault in what it adds or takes away would show here and
    nowhere else."""
    values = noise(size * 6, seed)
    got = through(lib, size, values)

    for index in range(size - 1, len(values)):
        wanted = sum(values[index - size + 1:index + 1]) / size

        assert abs(got[index] - wanted) <= 1e-4 * (1.0 + abs(wanted))


@given(SIZES, st.integers(1, 64))
@RUNS
def test_while_the_window_fills_the_mean_is_of_what_has_arrived(lib, size,
                                                                seed):
    """WHAT THE MODULE PROMISES ABOUT ITS FIRST FEW ANSWERS. A mean taken over
    the whole window before the window is full would start low and climb, and a
    caller measuring from the first sample would read a rise that is not there.
    The mean is taken over what has arrived instead."""
    values = noise(size * 3, seed)
    got = through(lib, size, values)

    for index in range(min(size - 1, len(values))):
        wanted = sum(values[:index + 1]) / (index + 1)

        assert abs(got[index] - wanted) <= 1e-4 * (1.0 + abs(wanted))


@given(st.integers(min_value=2, max_value=24),
       st.integers(min_value=1, max_value=4),
       st.floats(min_value=0.25, max_value=4.0, width=32))
@RUNS
def test_a_wave_whose_period_divides_the_window_is_removed_completely(
        lib, size, turns, height):
    """THE RULE THAT DECIDES WHAT LENGTH TO CHOOSE, and the one nobody expects.

    A mean of n samples adds up one whole turn of any wave whose period divides
    n, and a whole turn of a wave adds up to nothing. Such a wave is therefore
    removed COMPLETELY and not merely quietened.

    That is why a mean of 50 samples at 1000 a second is the answer to mains hum
    at 20 hertz and its harmonics, and why a mean of 49 is not."""
    # A wave that fits `turns` whole turns into the window.
    period = size / turns
    assume(period >= 2.0)

    count = size * 8
    values = [sp.to_float32(height * math.sin(TWO_PI * index / period))
              for index in range(count)]

    got = through(lib, size, values)

    # Once the window is full, nothing of the wave is left.
    for index in range(size - 1, count):
        assert abs(got[index]) <= 1e-3 * height


@given(SIZES, sp.elements(8.0))
@RUNS
def test_a_signal_that_does_not_change_comes_through_unchanged(lib, size,
                                                               level):
    """A mean of a level is that level, at every sample and from the first one.
    A filter that changed it would be adding a level of its own to every
    reading it was ever given."""
    got = through(lib, size, [level] * (size * 3))

    for value in got:
        assert abs(value - level) <= 1e-4 * (1.0 + abs(level))


@given(SIZES, st.integers(1, 64), st.floats(min_value=0.25, max_value=8.0,
                                            width=32))
@RUNS
def test_it_is_linear_in_the_signal(lib, size, seed, louder):
    """A mean is a sum divided by a count, thus twice the signal gives twice
    the answer and two signals added give the two answers added."""
    values = noise(size * 4, seed)
    louder_values = [sp.to_float32(value * louder) for value in values]

    plain = through(lib, size, values)
    scaled = through(lib, size, louder_values)

    for one, other in zip(plain, scaled):
        assert abs(other - (one * louder)) <= 1e-3 * (1.0 + abs(other))


@given(SIZES, st.integers(1, 64))
@RUNS
def test_the_root_mean_square_is_the_root_of_the_mean_of_the_squares(lib,
                                                                     size,
                                                                     seed):
    """A second running total, and it must be the thing its name says. The
    module keeps it beside the first rather than working it out afresh, thus
    the same fault could live in it alone."""
    values = noise(size * 4, seed)

    movavg = lib.movavg_alloc(size)

    try:
        for index, value in enumerate(values):
            lib.movavg_process_sample(movavg, value)

            first = max(0, index - size + 1)
            held = values[first:index + 1]

            wanted = math.sqrt(sum(v * v for v in held) / len(held))

            assert abs(lib.movavg_get_rms(movavg)
                       - wanted) <= 1e-3 * (1.0 + wanted)
            assert lib.movavg_get_rms(movavg) >= 0.0
    finally:
        lib.movavg_free(movavg)


@given(SIZES, st.integers(1, 64))
@RUNS
def test_the_running_total_does_not_drift_over_a_long_run(lib, size, seed):
    """A RUNNING TOTAL IS ADDED TO AND TAKEN FROM FOR EVER, thus the rounding
    of every sample it ever saw is still in it. Left alone it drifts away from
    the true mean, and the module builds the totals again from time to time for
    that reason.

    Held over a run far longer than that rebuilding, against the mean worked out
    afresh at the end."""
    count = size * 400
    values = noise(count, seed)

    movavg = lib.movavg_alloc(size)

    try:
        for value in values:
            lib.movavg_process_sample(movavg, value)

        wanted = sum(values[-size:]) / size

        assert abs(lib.movavg_get_mean(movavg)
                   - wanted) <= 1e-4 * (1.0 + abs(wanted))
    finally:
        lib.movavg_free(movavg)


@given(SIZES, st.integers(1, 64))
@RUNS
def test_a_reset_filter_answers_as_a_new_one_does(lib, size, seed):
    """The window and both totals are the whole of its memory."""
    values = noise(size * 3, seed)

    movavg = lib.movavg_alloc(size)

    try:
        for value in values:
            lib.movavg_process_sample(movavg, value)

        lib.movavg_reset(movavg)

        after = [lib.movavg_process_sample(movavg, value) for value in values]
    finally:
        lib.movavg_free(movavg)

    assert after == through(lib, size, values)
