"""Properties of the filter that replaces only the samples that are wrong.

The module makes two claims that are worth more than its code, and both are
tested here as claims and not as code paths.

THE FIRST is that a clean signal comes back EXACTLY as it went in. That is the
whole difference between this filter and a median filter, and it is the reason
to pay for the extra work. A test that only checked that spikes are removed
would pass just as well for a median filter, which is not the same module.

THE SECOND is that the size of a spike cannot help it escape. The threshold is
built on the median absolute deviation exactly so that one bad sample cannot
raise the bar that is meant to catch it. A detector built on a standard
deviation misses the worst faults most surely of all, and the test below would
fail for one.
"""

import ctypes
import os
import statistics
import sys

from hypothesis import assume, given
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402

SCALE = 1.4826
THRESHOLD = 3.0


def hampel_model(values, window, threshold=THRESHOLD):
    """What the filter should give for a whole block, worked out plainly.

    Every multiplication is brought to the width the library holds, because the
    filter decides by comparing two products and a decision that is made in a
    wider arithmetic is a different decision.
    """
    half = window // 2
    size = len(values)
    fed = list(values) + [values[-1]] * half
    output = list(values)

    for index in range(size):
        end = index + half
        if end < (window - 1):
            continue
        held = fed[end - window + 1:end + 1]
        centre = sorted(held)[half]
        judged = held[half]

        distances = sorted(sp.to_float32(abs(value - centre))
                           for value in held)
        spread = sp.to_float32(distances[half] * SCALE)
        distance = sp.to_float32(abs(judged - centre))

        if spread > 0.0:
            wrong = distance > sp.to_float32(threshold * spread)
        else:
            wrong = distance > 0.0

        output[index] = centre if wrong else judged

    # The two ends never had neighbours on both sides.
    for index in range(min(half, size)):
        output[index] = values[index]
        output[size - 1 - index] = values[size - 1 - index]

    return output


def run_block(lib, hampel, values):
    """Give the output of the filter for a whole block."""
    source = ffitt.float_array(values)
    room = ffitt.real_buffer(len(values))
    replaced = lib.hampel_process_block(ctypes.byref(hampel), source, room,
                                        len(values))
    return list(room), replaced


windows = st.sampled_from([3, 5, 7, 9, 11])
# Whole numbers, so that no comparison in the filter turns on a digit that the
# width cannot hold.
counts = st.integers(min_value=-60, max_value=60).map(float)


@given(values=st.lists(counts, min_size=1, max_size=50), window=windows)
def test_every_answer_agrees_with_the_rule_worked_out_plainly(lib, values,
                                                              window):
    values = [sp.to_float32(value) for value in values]
    hampel = lib.hampel_alloc(window)
    try:
        answer, _ = run_block(lib, hampel, values)
        assert answer == hampel_model(values, window)
    finally:
        lib.hampel_free(ctypes.byref(hampel))


@given(start=st.integers(min_value=-20, max_value=20).map(float),
       slope=st.integers(min_value=-4, max_value=4).map(float),
       size=st.integers(min_value=1, max_value=40),
       window=windows)
def test_a_straight_line_comes_back_bit_for_bit(lib, start, slope, size,
                                                window):
    """The claim of the module, at its sharpest.

    Nothing about a straight line is wrong, thus nothing about it may be
    changed. Not rounded, not flattened, not shifted: the same values.
    """
    values = [sp.to_float32(start + slope * index) for index in range(size)]
    hampel = lib.hampel_alloc(window)
    try:
        answer, replaced = run_block(lib, hampel, values)
        assert answer == values
        assert replaced == 0
    finally:
        lib.hampel_free(ctypes.byref(hampel))


@given(start=st.integers(min_value=-20, max_value=20).map(float),
       slope=st.integers(min_value=1, max_value=4).map(float),
       size=st.integers(min_value=16, max_value=40),
       window=st.sampled_from([5, 7, 9, 11]))
def test_a_median_filter_moves_a_clean_ramp_and_this_one_leaves_it_alone(
        lib, start, slope, size, window):
    """The two filters set beside each other on a signal with nothing wrong.

    A median filter answers with a sample of its window whether or not anything
    was wrong, thus on a rising line it answers late and moves nearly every
    sample. This filter asks first, finds nothing wrong, and answers with the
    sample that arrived.

    Both are given the same signal. One changes almost all of it; the other
    changes none of it.
    """
    values = [sp.to_float32(start + slope * index) for index in range(size)]

    hampel = lib.hampel_alloc(window)
    medfilt = lib.medfilt_alloc(window)
    try:
        cleaned, replaced = run_block(lib, hampel, values)
        smoothed = [lib.medfilt_process_sample(ctypes.byref(medfilt), value)
                    for value in values]
    finally:
        lib.hampel_free(ctypes.byref(hampel))
        lib.medfilt_free(ctypes.byref(medfilt))

    moved_by_the_median = sum(1 for index in range(size)
                              if smoothed[index] != values[index])

    assert cleaned == values
    assert replaced == 0
    assert moved_by_the_median >= (size - 1)


@given(level=st.integers(min_value=-10, max_value=10).map(float),
       size=st.integers(min_value=13, max_value=40),
       place=st.integers(min_value=0, max_value=39),
       window=windows,
       power=st.integers(min_value=6, max_value=20))
def test_a_spike_cannot_grow_its_way_out_of_being_caught(lib, level, size,
                                                         place, window, power):
    """The reason the deviation is a median one, held as a law.

    The spike is made 64, then 128, and so on up to a million times the signal
    it sits on. A threshold built on a standard deviation would rise with the
    spike and let the largest ones through. This one must catch every single
    one, and the largest most easily of all.

    The neighbours on either side are moved by one, so that the window has a
    spread of its own and the test is not merely finding the one value in a row
    of equal ones.
    """
    half = window // 2
    place = half + (place % max(1, size - 2 * half))
    assume(place < (size - half))

    values = [sp.to_float32(level) for _ in range(size)]
    values[place - 1] = sp.to_float32(level + 1.0)
    values[place + 1] = sp.to_float32(level - 1.0)
    spike = sp.to_float32(level + float(2 ** power))
    values[place] = spike

    hampel = lib.hampel_alloc(window)
    try:
        answer, replaced = run_block(lib, hampel, values)
    finally:
        lib.hampel_free(ctypes.byref(hampel))

    assert answer[place] != spike
    assert replaced >= 1
    # What stands in its place is a value the window really held.
    assert answer[place] in values[place - half:place + half + 1]


@given(values=st.lists(counts, min_size=1, max_size=40),
       window=windows,
       power=st.integers(min_value=-4, max_value=4))
def test_doubling_the_signal_doubles_the_answer_and_changes_no_decision(
        lib, values, window, power):
    """A median and a median absolute deviation both scale with the signal.

    Thus so does the threshold, and thus the filter must make exactly the same
    decisions about a signal twice as large. A filter that compared against any
    fixed size would fail this, and would then behave differently on the same
    measurement read in millivolts and in volts.
    """
    factor = float(2 ** power)
    values = [sp.to_float32(value) for value in values]
    scaled = [sp.to_float32(value * factor) for value in values]

    hampel = lib.hampel_alloc(window)
    try:
        plain, plain_count = run_block(lib, hampel, values)
        lib.hampel_reset(ctypes.byref(hampel))
        large, large_count = run_block(lib, hampel, scaled)
    finally:
        lib.hampel_free(ctypes.byref(hampel))

    assert plain_count == large_count
    assert large == [sp.to_float32(value * factor) for value in plain]


@given(values=st.lists(counts, min_size=1, max_size=40), window=windows)
def test_turning_the_signal_upside_down_turns_the_answer_upside_down(
        lib, values, window):
    values = [sp.to_float32(value) for value in values]
    turned = [sp.to_float32(-value) for value in values]

    hampel = lib.hampel_alloc(window)
    try:
        plain, plain_count = run_block(lib, hampel, values)
        lib.hampel_reset(ctypes.byref(hampel))
        upside_down, turned_count = run_block(lib, hampel, turned)
    finally:
        lib.hampel_free(ctypes.byref(hampel))

    assert plain_count == turned_count
    assert upside_down == [sp.to_float32(-value) for value in plain]


@given(values=st.lists(counts, min_size=1, max_size=40),
       window=windows,
       lower=st.floats(min_value=0.0625, max_value=4.0),
       gap=st.floats(min_value=0.0625, max_value=8.0))
def test_a_higher_threshold_never_replaces_more_than_a_lower_one(lib, values,
                                                                 window,
                                                                 lower, gap):
    """The threshold means what it says: it is how far is too far.

    A number that is raised must let more through, never less. That ordering is
    what lets a caller turn one knob and know which way the answer moves.
    """
    values = [sp.to_float32(value) for value in values]

    hampel = lib.hampel_alloc(window)
    try:
        assert lib.hampel_set_threshold(ctypes.byref(hampel),
                                        sp.to_float32(lower))
        _, many = run_block(lib, hampel, values)

        lib.hampel_reset(ctypes.byref(hampel))
        assert lib.hampel_set_threshold(ctypes.byref(hampel),
                                        sp.to_float32(lower + gap))
        _, few = run_block(lib, hampel, values)
    finally:
        lib.hampel_free(ctypes.byref(hampel))

    assert few <= many


@given(window=windows, bad=st.floats(min_value=-4.0, max_value=0.0))
def test_a_threshold_that_means_nothing_is_refused(lib, window, bad):
    hampel = lib.hampel_alloc(window)
    try:
        assert lib.hampel_set_threshold(ctypes.byref(hampel),
                                        sp.to_float32(bad)) is False
        assert hampel.threshold == sp.to_float32(THRESHOLD)
    finally:
        lib.hampel_free(ctypes.byref(hampel))


@given(values=st.lists(counts, min_size=1, max_size=40), window=windows)
def test_the_answer_for_a_sample_is_always_a_sample_of_its_own_window(lib,
                                                                     values,
                                                                     window):
    """Nothing new may appear in the output.

    The filter either keeps the sample or puts the middle of the window in its
    place, and the middle of an odd window is one of the samples. Thus no value
    can come out that did not go in near that place. A filter that averaged
    anything would break this at once.
    """
    half = window // 2
    values = [sp.to_float32(value) for value in values]

    hampel = lib.hampel_alloc(window)
    try:
        answer, _ = run_block(lib, hampel, values)
    finally:
        lib.hampel_free(ctypes.byref(hampel))

    for index, given_back in enumerate(answer):
        near = values[max(0, index - half):index + half + 1]
        assert given_back in near


@given(window=windows)
def test_the_answer_comes_half_a_window_late(lib, window):
    hampel = lib.hampel_alloc(window)
    try:
        assert lib.hampel_delay(ctypes.byref(hampel)) == window // 2
    finally:
        lib.hampel_free(ctypes.byref(hampel))


@given(window=st.integers(min_value=0, max_value=20))
def test_only_an_odd_window_of_three_or_more_can_be_used(lib, window):
    """A window of one has no neighbours; an even window has no middle."""
    assert lib.hampel_is_valid_window(window) == ((window >= 3)
                                                  and (window % 2 == 1))


@given(values=st.lists(counts, min_size=1, max_size=30), window=windows)
def test_the_count_of_what_was_replaced_climbs_and_a_reset_clears_it(lib,
                                                                     values,
                                                                     window):
    values = [sp.to_float32(value) for value in values]
    hampel = lib.hampel_alloc(window)
    try:
        _, first = run_block(lib, hampel, values)
        assert lib.hampel_replaced_count(ctypes.byref(hampel)) == first

        _, second = run_block(lib, hampel, values)
        assert (lib.hampel_replaced_count(ctypes.byref(hampel))
                == first + second)

        lib.hampel_reset(ctypes.byref(hampel))
        assert lib.hampel_replaced_count(ctypes.byref(hampel)) == 0
    finally:
        lib.hampel_free(ctypes.byref(hampel))


@given(level=st.integers(min_value=-10, max_value=10).map(float),
       size=st.integers(min_value=13, max_value=40),
       place=st.integers(min_value=0, max_value=39),
       window=st.sampled_from([3, 5, 7, 9]),
       power=st.integers(min_value=6, max_value=20))
def test_a_rule_built_on_a_standard_deviation_would_catch_nothing_at_all(
        lib, level, size, place, window, power):
    """Why the deviation is a median one, stated as arithmetic and not opinion.

    For a window of n samples, no sample can stand further from the mean than
    the square root of n-1 standard deviations. That is a limit of the
    arithmetic and not a matter of the signal: it holds for every window of
    every n samples that were ever measured.

    For n of 9 that limit is 2.83, and for n of 3 it is 1.41. All of them lie
    BELOW three. Thus a rule that replaces a sample standing three standard
    deviations from the mean of a window of NINE OR FEWER can never replace
    anything, whatever arrives, however badly the wiring fails. The windows
    tested here are exactly those, and a window of eleven is left out because
    it is the first one where such a rule could fire at all.

    The same spike is put to both rules here. The rule of the module catches
    it; the rule it was chosen over does not, and cannot.
    """
    half = window // 2
    place = half + (place % max(1, size - 2 * half))
    assume(place < (size - half))

    values = [sp.to_float32(level) for _ in range(size)]
    values[place - 1] = sp.to_float32(level + 1.0)
    values[place + 1] = sp.to_float32(level - 1.0)
    values[place] = sp.to_float32(level + float(2 ** power))

    held = values[place - half:place + half + 1]

    # What a rule built on a standard deviation would say about this window.
    mean = statistics.fmean(held)
    spread = statistics.pstdev(held)
    assume(spread > 0.0)
    stands_out = abs(held[half] - mean) / spread

    assert stands_out < 3.0
    assert stands_out <= ((window - 1) ** 0.5) + 1e-4

    # What the module says about the same window.
    hampel = lib.hampel_alloc(window)
    try:
        _, replaced = run_block(lib, hampel, values)
    finally:
        lib.hampel_free(ctypes.byref(hampel))

    assert replaced >= 1
