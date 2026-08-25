"""Rules that the rules of peak finding must themselves keep.

peakdetect_find applies four rules together: a height, a prominence, a width
and a distance. Four rules that interact is a space no set of examples covers.
These tests hold what must be true of any combination of them.
"""

import ctypes
import os
import sys

from hypothesis import assume, given
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

SIGNALS = st.lists(sp.elements(20.0), min_size=3, max_size=60)
ROOM = 64


def rules(lib, height=None, prominence=0.0, width=0.0, distance=0):
    """Give a set of rules with only the named ones switched on."""
    options = lib.peakdetect_no_rules()
    if height is not None:
        options.minimum_height = height
    options.minimum_prominence = prominence
    options.minimum_width = width
    options.minimum_distance = distance
    return options


def found(lib, signal, options):
    """Give the indices of the peaks that pass the rules."""
    indices = (ctypes.c_uint32 * ROOM)()
    count = lib.peakdetect_find(sptk.float_array(signal), len(signal),
                                ctypes.byref(options), indices, ROOM)
    return [indices[index] for index in range(count)]


@given(SIGNALS)
def test_every_peak_that_is_found_stands_above_its_neighbours(lib, signal):
    """Whatever the rules, a peak is a place the signal comes to a top.

    A flat top is reported at its middle, thus the sample beside the answer may
    equal it; nothing beside it may be larger.
    """
    peaks = found(lib, signal, rules(lib))

    for peak in peaks:
        assert 0 < peak < len(signal) - 1
        assert signal[peak] >= signal[peak - 1]
        assert signal[peak] >= signal[peak + 1]


@given(SIGNALS)
def test_the_peaks_come_back_in_the_order_they_stand_in_the_signal(lib,
                                                                   signal):
    """A caller reads them as they come, thus they must be in order."""
    peaks = found(lib, signal, rules(lib))

    for first, second in zip(peaks, peaks[1:]):
        assert first < second


@given(SIGNALS)
def test_no_peak_is_reported_twice(lib, signal):
    peaks = found(lib, signal, rules(lib))
    assert len(peaks) == len(set(peaks))


@given(SIGNALS, st.floats(min_value=0.0, max_value=8.0, width=32))
def test_a_stricter_rule_never_finds_more_peaks(lib, signal, step):
    """THE RULE THAT TIES THE FOUR TOGETHER.

    Every one of the four rules only ever removes peaks. Asking for more of
    anything must give the same peaks or fewer, never more and never different
    ones. A rule that added a peak would not be a rule at all.
    """
    for name in ("prominence", "width", "distance"):
        if name == "distance":
            loose = rules(lib, distance=1)
            tight = rules(lib, distance=1 + int(step))
        elif name == "prominence":
            loose = rules(lib, prominence=0.0)
            tight = rules(lib, prominence=step)
        else:
            loose = rules(lib, width=0.0)
            tight = rules(lib, width=step)

        many = found(lib, signal, loose)
        few = found(lib, signal, tight)

        assert len(few) <= len(many), name
        assert set(few) <= set(many), name


@given(SIGNALS, st.integers(min_value=1, max_value=12))
def test_no_two_peaks_stand_closer_than_the_distance_asked_for(lib, signal,
                                                               distance):
    """The rule that separates the beats of a heart from the wobbles on them."""
    peaks = found(lib, signal, rules(lib, distance=distance))

    for first, second in zip(peaks, peaks[1:]):
        assert (second - first) >= distance


@given(SIGNALS, sp.elements(20.0))
def test_no_peak_below_the_height_asked_for_is_kept(lib, signal, height):
    peaks = found(lib, signal, rules(lib, height=height))

    for peak in peaks:
        assert signal[peak] >= height


@given(SIGNALS, st.floats(min_value=0.0, max_value=10.0, width=32))
def test_no_peak_below_the_prominence_asked_for_is_kept(lib, signal,
                                                        prominence):
    """What the rule promises must agree with what the measure reports."""
    peaks = found(lib, signal, rules(lib, prominence=prominence))

    for peak in peaks:
        measured = lib.peakdetect_prominence(sptk.float_array(signal),
                                             len(signal), peak)
        assert measured >= prominence - 1e-3


@given(SIGNALS)
def test_a_prominence_is_never_below_nothing(lib, signal):
    """A peak cannot stand below the ground it rises from."""
    peaks = found(lib, signal, rules(lib))

    for peak in peaks:
        assert lib.peakdetect_prominence(sptk.float_array(signal),
                                         len(signal), peak) >= -1e-4


@given(SIGNALS)
def test_a_prominence_never_reaches_above_the_whole_range_of_the_signal(lib,
                                                                        signal):
    """A peak cannot stand out by more than the signal ever moves."""
    peaks = found(lib, signal, rules(lib))
    span = max(signal) - min(signal)

    for peak in peaks:
        measured = lib.peakdetect_prominence(sptk.float_array(signal),
                                             len(signal), peak)
        assert measured <= span + 1e-3


@given(SIGNALS, st.floats(min_value=0.125, max_value=0.875, width=32))
def test_a_width_is_never_below_nothing_and_never_wider_than_the_signal(lib,
                                                                        signal,
                                                                        part):
    peaks = found(lib, signal, rules(lib))

    for peak in peaks:
        width = lib.peakdetect_width(sptk.float_array(signal), len(signal),
                                     peak, part)
        assert -1e-4 <= width <= float(len(signal))


@given(SIGNALS)
def test_the_rules_with_nothing_switched_on_keep_every_peak(lib, signal):
    """peakdetect_no_rules must let everything through.

    A caller sets only the rules it wants, thus what it does not set must not
    quietly remove anything.
    """
    peaks = found(lib, signal, rules(lib))

    plain = []
    for index in range(1, len(signal) - 1):
        if signal[index] > signal[index - 1] and signal[index] > signal[index + 1]:
            plain.append(index)

    # Every plain peak must be there. The module also reports the middle of a
    # flat top, which the plain walk above does not find, thus it may hold
    # more.
    assert set(plain) <= set(peaks)


@given(SIGNALS)
def test_a_signal_that_only_rises_or_only_falls_has_no_peak(lib, signal):
    """A top needs something on both sides of it."""
    rising = sorted(signal)
    falling = sorted(signal, reverse=True)
    assume(rising[0] < rising[-1])

    assert found(lib, rising, rules(lib)) == []
    assert found(lib, falling, rules(lib)) == []


@given(st.lists(sp.elements(20.0), min_size=0, max_size=2))
def test_a_signal_too_short_to_hold_a_peak_gives_none(lib, signal):
    """Three samples are the fewest that can hold a top."""
    assert found(lib, signal, rules(lib)) == []
