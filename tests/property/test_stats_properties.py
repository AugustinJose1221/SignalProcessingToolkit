"""Properties of the measures of a list of samples.

Each measure is compared against the same measure written plainly in Python.
The module works these out with a select that reorders the list, which is fast
and easy to get subtly wrong; the plain Python sorts the whole list, which is
slow and obviously right. Comparing the two over many lists is the way to see
that the fast one is also right.
"""

import ctypes
import os
import statistics
import sys

from hypothesis import given
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

samples = st.lists(sp.elements(), min_size=1, max_size=40)


def as_float32(values):
    return [sp.to_float32(value) for value in values]


@given(data=samples)
def test_the_mean_is_the_sum_divided_by_the_count(lib, data):
    array = sptk.float_array(data)
    expected = statistics.fmean(as_float32(data))

    result = lib.stats_mean(array, len(data))

    assert abs(result - expected) <= (1e-4 * max(1.0, abs(expected)))


@given(data=samples)
def test_the_median_matches_a_plain_sort(lib, data):
    array = sptk.float_array(data)
    expected = statistics.median(sorted(as_float32(data)))

    result = lib.stats_median(array, len(data))

    assert abs(result - expected) <= 1e-4


@given(data=samples)
def test_the_median_leaves_the_same_samples_behind(lib, data):
    """The select reorders the list. It must not lose or change a sample."""
    array = sptk.float_array(data)

    lib.stats_median(array, len(data))

    after = sorted(round(array[index], 4) for index in range(len(data)))
    before = sorted(round(value, 4) for value in as_float32(data))
    assert after == before


@given(data=samples, part=st.floats(min_value=0.0, max_value=1.0, width=32))
def test_the_percentile_matches_a_plain_sort(lib, data, part):
    array = sptk.float_array(data)

    result = lib.stats_percentile(array, len(data), part)

    # The same rule, written the slow and obvious way.
    ordered = sorted(as_float32(data))
    place = part * (len(ordered) - 1)
    below = int(place)
    between = place - below
    if below + 1 < len(ordered):
        expected = ordered[below] + (between * (ordered[below + 1] - ordered[below]))
    else:
        expected = ordered[-1]

    assert abs(result - expected) <= 1e-3 * max(1.0, abs(expected))


@given(data=samples)
def test_the_percentile_never_leaves_the_range_of_the_samples(lib, data):
    for part in (0.0, 0.1, 0.5, 0.9, 1.0):
        array = sptk.float_array(data)
        result = lib.stats_percentile(array, len(data), part)
        assert min(as_float32(data)) - 1e-3 <= result <= max(as_float32(data)) + 1e-3


@given(data=samples)
def test_the_percentile_rises_with_the_part(lib, data):
    previous = None
    for part in (0.0, 0.25, 0.5, 0.75, 1.0):
        array = sptk.float_array(data)
        result = lib.stats_percentile(array, len(data), part)
        if previous is not None:
            assert result >= previous - 1e-3
        previous = result


@given(data=samples)
def test_the_median_absolute_deviation_matches_a_plain_sort(lib, data):
    array = sptk.float_array(data)
    work = sptk.float_array([0.0] * len(data))
    values = as_float32(data)
    middle = statistics.median(sorted(values))
    expected = statistics.median(sorted(abs(value - middle) for value in values))

    result = lib.stats_mad(array, len(data), work)

    assert abs(result - expected) <= 1e-3 * max(1.0, abs(expected))


@given(data=samples)
def test_the_median_absolute_deviation_leaves_the_data_as_it_was(lib, data):
    array = sptk.float_array(data)
    work = sptk.float_array([0.0] * len(data))

    lib.stats_mad(array, len(data), work)

    for index, value in enumerate(as_float32(data)):
        assert array[index] == value


@given(data=samples)
def test_the_deviation_is_the_root_of_the_variance(lib, data):
    array = sptk.float_array(data)

    variance = lib.stats_variance(array, len(data))
    deviation = lib.stats_deviation(array, len(data))

    assert variance >= -1e-6
    assert abs((deviation * deviation) - variance) <= 1e-3 * max(1.0, variance)


@given(data=samples)
def test_the_smallest_is_not_above_the_median_and_the_median_not_above_the_largest(lib, data):
    array = sptk.float_array(data)
    smallest = lib.stats_min(array, len(data))
    largest = lib.stats_max(array, len(data))
    middle = lib.stats_median(array, len(data))

    assert smallest <= middle + 1e-4
    assert middle <= largest + 1e-4


@given(data=samples, shift=sp.elements())
def test_moving_every_sample_moves_the_median_by_the_same_amount(lib, data, shift):
    """The median follows the samples; the deviation does not change at all."""
    array = sptk.float_array(data)
    moved = sptk.float_array([sp.to_float32(value + shift) for value in data])

    before = lib.stats_median(array, len(data))
    after = lib.stats_median(moved, len(data))

    assert abs((after - before) - shift) <= 1e-2 * max(1.0, abs(shift))
