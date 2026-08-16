"""Properties of the binary search, the peak detection and the valley
detection."""

import ctypes
import os
import sys

from hypothesis import given
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

REFERENCE = ctypes.byref

signals = st.lists(sp.elements(), min_size=1, max_size=24)


@st.composite
def rising_data_and_value(draw):
    """Give a rising list of values and a value inside its range."""
    size = draw(st.integers(min_value=2, max_value=12))
    start = draw(st.floats(min_value=-50.0, max_value=50.0, width=32))
    steps = draw(st.lists(st.floats(min_value=0.25, max_value=8.0, width=32),
                          min_size=size - 1, max_size=size - 1))
    data = [start]
    for step in steps:
        data.append(sp.to_float32(data[-1] + step))

    position = draw(st.floats(min_value=0.0, max_value=1.0, width=32))
    value = sp.to_float32(data[0] + ((data[-1] - data[0]) * position))
    return data, value


@given(pair=rising_data_and_value())
def test_the_binary_search_gives_the_first_value_that_is_not_less(lib, pair):
    data, value = pair
    array = sptk.float_array(data)

    index = lib.binarysearch_get_index(array, value, len(data))

    assert index < len(data)
    assert data[index] >= value
    if index > 0:
        assert data[index - 1] < value


@given(pair=rising_data_and_value())
def test_the_binary_search_finds_a_value_that_is_in_the_list(lib, pair):
    data, _ = pair
    array = sptk.float_array(data)

    for position, value in enumerate(data):
        index = lib.binarysearch_get_index(array, value, len(data))
        # The values rise, thus each value comes one time only.
        assert index == position


@given(data=signals)
def test_every_peak_is_larger_than_the_value_on_each_side(lib, data):
    array = sptk.float_array(data)
    index_buffer = (ctypes.c_float * (len(data) + 2))()
    peak_buffer = (ctypes.c_float * (len(data) + 2))()

    count = lib.peakdetect_get_peaks(array, index_buffer, peak_buffer, len(data))

    for position in range(count):
        index = int(index_buffer[position])
        assert 0 < index < len(data) - 1
        assert data[index] > data[index - 1]
        assert data[index] > data[index + 1]
        assert peak_buffer[position] == data[index]


@given(data=signals)
def test_every_valley_is_smaller_than_the_value_on_each_side(lib, data):
    array = sptk.float_array(data)
    index_buffer = (ctypes.c_float * (len(data) + 2))()
    valley_buffer = (ctypes.c_float * (len(data) + 2))()

    count = lib.valleydetect_get_valley(array, index_buffer, valley_buffer, len(data))

    for position in range(count):
        index = int(index_buffer[position])
        assert 0 < index < len(data) - 1
        assert data[index] < data[index - 1]
        assert data[index] < data[index + 1]
        assert valley_buffer[position] == data[index]


@given(data=signals)
def test_the_peaks_come_in_the_order_of_the_signal(lib, data):
    array = sptk.float_array(data)
    index_buffer = (ctypes.c_float * (len(data) + 2))()
    peak_buffer = (ctypes.c_float * (len(data) + 2))()

    count = lib.peakdetect_get_peaks(array, index_buffer, peak_buffer, len(data))

    indices = [int(index_buffer[position]) for position in range(count)]
    assert indices == sorted(indices)
    assert len(set(indices)) == len(indices)


@given(data=signals)
def test_a_point_is_never_a_peak_and_a_valley_at_the_same_time(lib, data):
    array = sptk.float_array(data)
    peak_index = (ctypes.c_float * (len(data) + 2))()
    peak_value = (ctypes.c_float * (len(data) + 2))()
    valley_index = (ctypes.c_float * (len(data) + 2))()
    valley_value = (ctypes.c_float * (len(data) + 2))()

    peaks = lib.peakdetect_get_peaks(array, peak_index, peak_value, len(data))
    valleys = lib.valleydetect_get_valley(array, valley_index, valley_value, len(data))

    peak_set = {int(peak_index[position]) for position in range(peaks)}
    valley_set = {int(valley_index[position]) for position in range(valleys)}

    assert peak_set.isdisjoint(valley_set)


@given(data=st.lists(sp.elements(), min_size=1, max_size=2))
def test_a_short_signal_has_no_peak_and_no_valley(lib, data):
    # A peak needs a value on each side, thus a signal with one or two values
    # can hold no peak.
    array = sptk.float_array(data)
    index_buffer = (ctypes.c_float * 4)()
    value_buffer = (ctypes.c_float * 4)()

    assert lib.peakdetect_get_peaks(array, index_buffer, value_buffer,
                                    len(data)) == 0
    assert lib.valleydetect_get_valley(array, index_buffer, value_buffer,
                                       len(data)) == 0


@given(pair=rising_data_and_value(),
       distance=st.floats(min_value=1.0, max_value=100.0, width=32))
def test_the_binary_search_gives_an_index_inside_the_list_for_any_value(lib, pair,
                                                                        distance):
    # The caller reads the list at the index that the search gives. Thus the
    # index must stay inside the list, even for a value that lies outside the
    # range of the list.
    data, _ = pair
    array = sptk.float_array(data)

    for value in (data[0] - distance, data[-1] + distance, data[0], data[-1]):
        index = lib.binarysearch_get_index(array, sp.to_float32(value), len(data))
        assert 0 <= index < len(data)
