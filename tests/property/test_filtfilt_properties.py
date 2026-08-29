"""Properties of filtering both ways.

Running a filter twice, once in each direction, is a thing that is easy to
write and easy to believe in without checking. The module claims two things for
it, and both are worth a signal on a bench:

    THE SHAPE IS KEPT. Every frequency is delayed by the same amount, which is
    none. Thus a signal that is symmetric about a point in time comes back
    symmetric about the SAME point. That is what a measurement needs and what a
    single pass cannot give.

    THE GAIN IS SQUARED. The filter is applied twice, thus a cutoff at 0.707 in
    one pass is 0.5 in two, and the band is narrower than the one designed.

Both are held here against a measurement of the module running, and not merely
against what the module says about itself. The second one matters most: a gain
function that agreed with itself but not with the filtering would mislead every
caller who designed against it.
"""

import ctypes
import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

cutoffs = st.sampled_from([0.02, 0.05, 0.1, 0.15, 0.25])
section_counts = st.integers(min_value=1, max_value=3)


def low_pass(lib, sections, cutoff):
    """Give a low pass with feedback, or None if it cannot be designed."""
    handle = lib.iir_alloc(sections)
    if not lib.iir_design_low_pass_with(ctypes.byref(handle),
                                        sp.to_float32(cutoff),
                                        sptk.IIR_BUTTERWORTH,
                                        sp.to_float32(1.0),
                                        sp.to_float32(40.0)):
        lib.iir_free(ctypes.byref(handle))
        return None
    return handle


def run_iir(lib, handle, values):
    source = sptk.float_array(values)
    room = sptk.real_buffer(len(values))
    ran = lib.filtfilt_iir(ctypes.byref(handle), source, room, len(values))
    return list(room), ran


def rms(values):
    return math.sqrt(sum(value * value for value in values) / len(values))


def tone(size, frequency, phase=0.0):
    return [sp.to_float32(math.sin(2.0 * math.pi * frequency * index + phase))
            for index in range(size)]


@given(sections=section_counts, cutoff=cutoffs,
       frequency=st.sampled_from([0.01, 0.03, 0.08, 0.12, 0.2, 0.3, 0.45]))
def test_what_the_gain_function_promises_is_what_the_filtering_does(
        lib, sections, cutoff, frequency):
    """The one test that keeps the designing honest.

    filtfilt_iir_gain exists so that a caller need not guess what the band
    really is. A tone is put through the filtering itself and its size measured
    before and after. The two must agree, or the function is telling callers
    about a filter that is not the one they are running.
    """
    handle = low_pass(lib, sections, cutoff)
    assume(handle is not None)
    try:
        promised = lib.filtfilt_iir_gain(ctypes.byref(handle),
                                         sp.to_float32(frequency))
        # A gain far down in the stop band is a difference of very small
        # numbers, which a 32 bit build cannot measure.
        assume(promised > 1e-3)

        size = 4096
        values = tone(size, frequency)
        answer, ran = run_iir(lib, handle, values)
        assert ran

        # The ends of the signal are where the carried part meets the real one.
        # The middle is where the steady answer lives.
        trim = size // 4
        measured = rms(answer[trim:size - trim]) / rms(values[trim:size - trim])
    finally:
        lib.iir_free(ctypes.byref(handle))

    assert abs(measured - promised) <= (0.05 * promised + 0.01)


@given(sections=section_counts, cutoff=cutoffs,
       frequency=st.sampled_from([0.01, 0.05, 0.1, 0.2, 0.4]))
def test_the_gain_of_two_passes_is_the_gain_of_one_pass_squared(lib, sections,
                                                                cutoff,
                                                                frequency):
    handle = low_pass(lib, sections, cutoff)
    assume(handle is not None)
    try:
        once = lib.iir_get_gain(ctypes.byref(handle), sp.to_float32(frequency))
        twice = lib.filtfilt_iir_gain(ctypes.byref(handle),
                                      sp.to_float32(frequency))
    finally:
        lib.iir_free(ctypes.byref(handle))

    assert twice == sp.to_float32(once * once)


@given(sections=section_counts, cutoff=cutoffs)
def test_the_cutoff_that_was_designed_is_no_longer_the_cutoff(lib, sections,
                                                              cutoff):
    """The price the header names, measured rather than asserted.

    A Butterworth filter passes 0.707 at its cutoff. Run both ways it passes
    half. A caller who wanted the band to end at the number given has to design
    for a wider one, and this is the arithmetic that says by how much.
    """
    handle = low_pass(lib, sections, cutoff)
    assume(handle is not None)
    try:
        once = lib.iir_get_gain(ctypes.byref(handle), sp.to_float32(cutoff))
        twice = lib.filtfilt_iir_gain(ctypes.byref(handle),
                                      sp.to_float32(cutoff))
    finally:
        lib.iir_free(ctypes.byref(handle))

    assert abs(once - (1.0 / math.sqrt(2.0))) <= 0.02
    assert abs(twice - 0.5) <= 0.02


@given(sections=section_counts, cutoff=cutoffs,
       width=st.integers(min_value=2, max_value=40))
@settings(max_examples=60)
def test_a_symmetric_signal_comes_back_symmetric_about_the_same_place(
        lib, sections, cutoff, width):
    """Zero phase, stated as the thing a caller can see.

    A pulse that is the same on both sides of its middle is put in. If any
    frequency were delayed more than another, the answer would lean: it would
    rise faster than it fell, or the other way about. It must not lean.
    """
    size = 1024
    middle = size // 2
    values = [sp.to_float32(0.0) for _ in range(size)]
    for step in range(-width, width + 1):
        values[middle + step] = sp.to_float32(1.0 - abs(step) / (width + 1.0))

    handle = low_pass(lib, sections, cutoff)
    assume(handle is not None)
    try:
        answer, ran = run_iir(lib, handle, values)
        assert ran
    finally:
        lib.iir_free(ctypes.byref(handle))

    size_of = max(abs(value) for value in answer)
    assume(size_of > 1e-3)
    for step in range(1, middle - 8):
        assert abs(answer[middle + step]
                   - answer[middle - step]) <= (0.01 * size_of)


@given(sections=section_counts, cutoff=cutoffs,
       width=st.integers(min_value=2, max_value=30))
@settings(max_examples=60)
def test_one_pass_leans_where_two_passes_do_not(lib, sections, cutoff, width):
    """The same pulse through a single pass, so that the test above means
    something.

    A test that only showed the answer symmetric would pass for a module that
    did nothing at all. Here the same filter is run once over the same pulse,
    and it must lean: its answer arrives late and is not the same on both
    sides. That is the fault the module was written to remove.
    """
    size = 1024
    middle = size // 2
    values = [sp.to_float32(0.0) for _ in range(size)]
    for step in range(-width, width + 1):
        values[middle + step] = sp.to_float32(1.0 - abs(step) / (width + 1.0))

    handle = low_pass(lib, sections, cutoff)
    assume(handle is not None)
    try:
        source = sptk.float_array(values)
        once = sptk.real_buffer(size)
        lib.iir_reset(ctypes.byref(handle))
        for index in range(size):
            once[index] = lib.iir_process_sample(ctypes.byref(handle),
                                                 source[index])
        once = list(once)

        answer, ran = run_iir(lib, handle, values)
        assert ran
    finally:
        lib.iir_free(ctypes.byref(handle))

    size_of = max(abs(value) for value in answer)
    assume(size_of > 1e-2)

    leaning_once = max(abs(once[middle + step] - once[middle - step])
                       for step in range(1, middle - 8))
    leaning_twice = max(abs(answer[middle + step] - answer[middle - step])
                        for step in range(1, middle - 8))

    assert leaning_twice <= (0.01 * size_of)
    assert leaning_once > (10.0 * leaning_twice)


@given(sections=section_counts, cutoff=cutoffs,
       level=st.floats(min_value=-50.0, max_value=50.0))
def test_a_signal_that_does_not_move_comes_back_as_it_was(lib, sections,
                                                          cutoff, level):
    """A low pass passes everything at rest, and both ends must know it.

    This is the test that finds a settling that did not settle: a filter
    starting from nothing answers a constant with a long swing, and running
    both ways would put that swing at both ends of the answer.
    """
    level = sp.to_float32(level)
    assume(abs(level) > 0.0625)
    size = 512
    values = [level for _ in range(size)]

    handle = low_pass(lib, sections, cutoff)
    assume(handle is not None)
    try:
        answer, ran = run_iir(lib, handle, values)
        assert ran
    finally:
        lib.iir_free(ctypes.byref(handle))

    for value in answer:
        assert abs(value - level) <= (1e-3 * abs(level))


@given(sections=section_counts, cutoff=cutoffs,
       power=st.integers(min_value=-6, max_value=6))
def test_making_the_signal_larger_makes_the_answer_larger_by_as_much(
        lib, sections, cutoff, power):
    """A filter is a linear thing, and running it twice leaves it linear."""
    factor = float(2 ** power)
    size = 256
    values = tone(size, 0.07)
    scaled = [sp.to_float32(value * factor) for value in values]

    handle = low_pass(lib, sections, cutoff)
    assume(handle is not None)
    try:
        plain, ran = run_iir(lib, handle, values)
        assert ran
        large, ran = run_iir(lib, handle, scaled)
        assert ran
    finally:
        lib.iir_free(ctypes.byref(handle))

    # BIT FOR BIT, AND NOTHING LESS WILL DO.
    #
    # The factor is a power of two, thus multiplying by it is exact, and every
    # step of the filtering is linear. There is therefore no reason for a
    # single digit to differ, and any that did would mean the module was
    # deciding something by the SIZE of the signal rather than its shape.
    #
    # This test found exactly that. The settling used to compare how far the
    # answer had moved against its size plus one, and that added one asked less
    # of a small signal than of a large one. A low pass at a cutoff of 0.02
    # gave a shape 2 parts in 100 different for a signal a thousandth of the
    # size: the same measurement read in volts and in millivolts came back
    # different. The comparison is now against the signal alone.
    for index in range(size):
        assert large[index] == sp.to_float32(plain[index] * factor)


@given(sections=section_counts, cutoff=cutoffs)
def test_filtering_a_list_into_itself_gives_the_same_as_into_another(
        lib, sections, cutoff):
    """The header allows the input and the output to be the same list.

    That works only because each sample is read before it is written over, in
    both directions. It is one line in the code and the whole of the promise.
    """
    size = 300
    values = tone(size, 0.09) 
    handle = low_pass(lib, sections, cutoff)
    assume(handle is not None)
    try:
        apart, ran = run_iir(lib, handle, values)
        assert ran

        together = sptk.float_array(values)
        assert lib.filtfilt_iir(ctypes.byref(handle), together, together, size)
    finally:
        lib.iir_free(ctypes.byref(handle))

    assert list(together) == apart


@given(sections=section_counts, cutoff=cutoffs,
       size=st.integers(min_value=0, max_value=10))
def test_a_signal_too_short_to_fill_the_filter_is_refused(lib, sections,
                                                          cutoff, size):
    handle = low_pass(lib, sections, cutoff)
    assume(handle is not None)
    try:
        source = sptk.float_array([sp.to_float32(1.0)] * max(size, 1))
        room = sptk.real_buffer(max(size, 1))
        ran = lib.filtfilt_iir(ctypes.byref(handle), source, room, size)
        assert ran == (size > (2 * sections))
    finally:
        lib.iir_free(ctypes.byref(handle))


@given(filter_size=st.integers(min_value=1, max_value=64),
       size=st.integers(min_value=0, max_value=400))
def test_the_carried_part_is_three_filters_long_or_all_there_is(lib,
                                                                filter_size,
                                                                size):
    """A signal cannot be carried further past its end than it is long."""
    padding = lib.filtfilt_padding(filter_size, size)

    if size == 0:
        assert padding == 0
    else:
        assert padding == min(3 * filter_size, size - 1)
        assert padding <= (size - 1)


@given(length=st.sampled_from([9, 15, 21, 31]),
       cutoff=st.sampled_from([0.05, 0.1, 0.2]),
       frequency=st.sampled_from([0.02, 0.06, 0.12, 0.3]))
def test_the_same_holds_for_a_filter_with_no_feedback(lib, length, cutoff,
                                                      frequency):
    """A filter with a middle already keeps the shape. It is run both ways for
    the sharper edges, and the gain is squared just the same.
    """
    handle = lib.fir_alloc(length)
    try:
        assume(lib.fir_design_low_pass_with(ctypes.byref(handle),
                                            sp.to_float32(cutoff),
                                            sptk.WINDOW_HAMMING,
                                            sp.to_float32(0.0)))
        once = lib.fir_get_gain(ctypes.byref(handle), sp.to_float32(frequency))
        promised = lib.filtfilt_fir_gain(ctypes.byref(handle),
                                         sp.to_float32(frequency))
        assert promised == sp.to_float32(once * once)
        assume(promised > 1e-3)

        size = 2048
        values = tone(size, frequency)
        source = sptk.float_array(values)
        room = sptk.real_buffer(size)
        assert lib.filtfilt_fir(ctypes.byref(handle), source, room, size)

        trim = size // 4
        answer = list(room)
        measured = rms(answer[trim:size - trim]) / rms(values[trim:size - trim])
    finally:
        lib.fir_free(ctypes.byref(handle))

    assert abs(measured - promised) <= (0.05 * promised + 0.01)


@given(length=st.sampled_from([9, 15, 21]),
       cutoff=st.sampled_from([0.05, 0.1, 0.2]),
       size=st.integers(min_value=0, max_value=25))
def test_a_signal_shorter_than_the_filter_with_no_feedback_is_refused(
        lib, length, cutoff, size):
    handle = lib.fir_alloc(length)
    try:
        assume(lib.fir_design_low_pass_with(ctypes.byref(handle),
                                            sp.to_float32(cutoff),
                                            sptk.WINDOW_HAMMING,
                                            sp.to_float32(0.0)))
        source = sptk.float_array([sp.to_float32(1.0)] * max(size, 1))
        room = sptk.real_buffer(max(size, 1))
        ran = lib.filtfilt_fir(ctypes.byref(handle), source, room, size)
        assert ran == (size > length)
    finally:
        lib.fir_free(ctypes.byref(handle))
