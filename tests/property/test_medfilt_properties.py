"""Properties of the median filter.

The filter holds its window in order and moves one sample in and one sample out
for each step. That is fast and easy to get subtly wrong, above all where the
same value stands more than once. Each answer is compared here against a plain
sort of the same window, which is slow and obviously right.
"""

import ctypes
import os
import statistics
import sys

from hypothesis import given
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402


def brute_median(signal, now, size):
    """The median of the window that ends at now, worked out by a sort."""
    window = sorted(signal[max(0, now - size + 1):now + 1])
    middle = len(window) // 2
    if len(window) % 2 == 1:
        return window[middle]
    return (window[middle - 1] + window[middle]) / 2.0


@given(signal=st.lists(sp.elements(), min_size=1, max_size=60),
       size=st.integers(min_value=1, max_value=15))
def test_every_answer_matches_a_plain_sort(lib, signal, size):
    values = [sp.to_float32(value) for value in signal]
    medfilt = lib.medfilt_alloc(size)
    try:
        for index, value in enumerate(values):
            result = lib.medfilt_process_sample(ctypes.byref(medfilt), value)
            assert abs(result - brute_median(values, index, size)) <= 1e-3
    finally:
        lib.medfilt_free(ctypes.byref(medfilt))


@given(signal=st.lists(st.integers(min_value=0, max_value=3).map(float),
                       min_size=1, max_size=60),
       size=st.integers(min_value=1, max_value=15))
def test_it_holds_up_when_the_same_value_stands_many_times(lib, signal, size):
    """Few different values means many duplicates, which is the hard case."""
    values = [sp.to_float32(value) for value in signal]
    medfilt = lib.medfilt_alloc(size)
    try:
        for index, value in enumerate(values):
            result = lib.medfilt_process_sample(ctypes.byref(medfilt), value)
            assert abs(result - brute_median(values, index, size)) <= 1e-3
    finally:
        lib.medfilt_free(ctypes.byref(medfilt))


@given(signal=st.lists(sp.elements(), min_size=1, max_size=40),
       size=st.integers(min_value=1, max_value=12))
def test_the_answer_is_always_one_of_the_samples_of_an_odd_window(lib, signal, size):
    """A median of an odd window is a sample, never a value between two."""
    if size % 2 == 0:
        size += 1
    values = [sp.to_float32(value) for value in signal]
    medfilt = lib.medfilt_alloc(size)
    try:
        for index, value in enumerate(values):
            result = lib.medfilt_process_sample(ctypes.byref(medfilt), value)
            window = values[max(0, index - size + 1):index + 1]
            if len(window) % 2 == 1:
                assert any(abs(result - held) <= 1e-4 for held in window)
    finally:
        lib.medfilt_free(ctypes.byref(medfilt))


@given(signal=st.lists(sp.elements(), min_size=1, max_size=40),
       size=st.integers(min_value=1, max_value=12))
def test_the_answer_never_leaves_the_range_of_the_window(lib, signal, size):
    values = [sp.to_float32(value) for value in signal]
    medfilt = lib.medfilt_alloc(size)
    try:
        for index, value in enumerate(values):
            result = lib.medfilt_process_sample(ctypes.byref(medfilt), value)
            window = values[max(0, index - size + 1):index + 1]
            assert min(window) - 1e-3 <= result <= max(window) + 1e-3
    finally:
        lib.medfilt_free(ctypes.byref(medfilt))


@given(signal=st.lists(sp.elements(), min_size=1, max_size=40))
def test_a_window_of_one_passes_the_signal_through(lib, signal):
    values = [sp.to_float32(value) for value in signal]
    medfilt = lib.medfilt_alloc(1)
    try:
        for value in values:
            assert lib.medfilt_process_sample(ctypes.byref(medfilt), value) == value
    finally:
        lib.medfilt_free(ctypes.byref(medfilt))
