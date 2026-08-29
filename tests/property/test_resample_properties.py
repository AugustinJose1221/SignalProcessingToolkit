"""Properties of changing the rate at which a signal is sampled.

The module exists because of one thing that goes wrong, and the tests are built
on that thing rather than on the shape of the interface.

Keeping every fourth sample is a whole rate change as far as the code goes. It
is also the fault the module was written to prevent: every frequency above half
the new rate comes back at a frequency it never had, sits on top of the signal,
and CANNOT BE TAKEN OUT AGAIN by any later step. The reading then looks
perfectly reasonable and is wrong.

Thus the central test here does not merely check that the module answers. It
puts the same tone through the module and through the plain thrown-away
decimation that a caller would otherwise write, and holds the module to
removing what the plain one lets through.
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

factors = st.sampled_from([2, 3, 4, 5, 8])


def tone(size, frequency, phase=0.0):
    return [sp.to_float32(math.sin(2.0 * math.pi * frequency * index + phase))
            for index in range(size)]


def rms(values):
    if not values:
        return 0.0
    return math.sqrt(sum(value * value for value in values) / len(values))


def decimate_block(lib, resample, values):
    source = sptk.float_array(values)
    room = sptk.real_buffer(len(values))
    written = lib.resample_decimate_block(ctypes.byref(resample), source, room,
                                          len(values))
    return list(room)[:written]


def interpolate_block(lib, resample, values, factor):
    source = sptk.float_array(values)
    room = sptk.real_buffer(len(values) * factor)
    written = lib.resample_interpolate_block(ctypes.byref(resample), source,
                                             room, len(values))
    return list(room)[:written]


@given(factor=factors,
       part=st.sampled_from([0.2, 0.4, 0.6, 0.8]))
@settings(max_examples=50)
def test_what_would_come_back_as_a_false_tone_is_stopped_before_it_can(
        lib, factor, part):
    """The fault the module prevents, set beside the module preventing it.

    A tone is placed ABOVE half the new rate, which is exactly where a signal
    must not be when samples are thrown away. It is decimated twice: once by
    keeping every factor-th sample, which is what a caller writes when the
    module is not there, and once by the module.

    The plain one gives back the tone at full size, standing at a frequency it
    never had. The module gives back next to nothing. That difference is the
    whole of what this module is.
    """
    # Half the NEW rate is 0.5/factor counted at the old rate. The tone is put
    # somewhere between there and half the old rate, which is the whole of the
    # band that must not survive being decimated.
    edge = 0.5 / factor
    frequency = edge + part * (0.5 - edge)
    assume(frequency < 0.5)

    size = 4096
    values = tone(size, frequency)

    length = lib.resample_advised_length(factor)
    resample = lib.resample_alloc_decimator(factor, length)
    try:
        cleaned = decimate_block(lib, resample, values)
    finally:
        lib.resample_free(ctypes.byref(resample))

    thrown_away = values[::factor]

    # Past the settling of the filter, so that what is measured is the answer
    # and not the filter starting up.
    skip = (length // factor) + 2
    assume(len(cleaned) > (2 * skip))

    stopped = rms(cleaned[skip:])
    let_through = rms(thrown_away[skip:])

    # The plain decimation keeps the tone whole: it is now a false one.
    assert let_through > 0.5
    # The module puts it away, and the header says by how much: the advised
    # length is chosen for a stop band about 60 dB down. That was measured
    # across every factor and every place in the band, and the worst of them
    # all came to 9.3e-4, which is 60.6 dB. The bound below is 54 dB, thus it
    # holds the module to the figure its own rule of thumb promises and leaves
    # only the room the last digits need.
    assert stopped < (0.002 * let_through)


@given(factor=factors,
       part=st.sampled_from([0.05, 0.15, 0.25, 0.35]))
@settings(max_examples=50)
def test_a_tone_that_belongs_in_the_new_band_comes_through_whole(lib, factor,
                                                                 part):
    """The other half of the same promise.

    Stopping everything would stop the aliases too. What is wanted is a filter
    that stops what must be stopped and passes the rest unchanged, and a tone
    well inside the new band must come back the size it went in.
    """
    frequency = part / factor

    size = 4096
    values = tone(size, frequency)

    length = lib.resample_advised_length(factor)
    resample = lib.resample_alloc_decimator(factor, length)
    try:
        answer = decimate_block(lib, resample, values)
    finally:
        lib.resample_free(ctypes.byref(resample))

    skip = (length // factor) + 2
    assume(len(answer) > (2 * skip))

    assert abs(rms(answer[skip:]) - rms(values)) <= 0.05


@given(factor=factors, size=st.integers(min_value=1, max_value=300))
def test_one_sample_comes_out_for_each_factor_that_goes_in(lib, factor, size):
    values = tone(size, 0.01)

    length = lib.resample_advised_length(factor)
    resample = lib.resample_alloc_decimator(factor, length)
    try:
        answer = decimate_block(lib, resample, values)
    finally:
        lib.resample_free(ctypes.byref(resample))

    assert len(answer) == (size // factor)


@given(factor=factors, size=st.integers(min_value=1, max_value=120))
def test_factor_samples_come_out_for_each_one_that_goes_in(lib, factor, size):
    values = tone(size, 0.01)

    length = lib.resample_advised_length(factor)
    resample = lib.resample_alloc_interpolator(factor, length)
    try:
        answer = interpolate_block(lib, resample, values, factor)
    finally:
        lib.resample_free(ctypes.byref(resample))

    assert len(answer) == (size * factor)


@given(factor=factors, level=st.floats(min_value=-40.0, max_value=40.0))
def test_a_signal_at_rest_keeps_its_level_both_ways(lib, factor, level):
    """Neither direction may change the size of a signal that does not move.

    For the decimator that means a filter whose coefficients add to one. For
    the interpolator it means something less obvious: the zeros put between the
    samples carry no energy, thus the coefficients are made factor times larger
    to make up for it. Get that wrong and every rate change quietly scales the
    measurement.
    """
    level = sp.to_float32(level)
    assume(abs(level) > 0.0625)

    size = 600
    values = [level for _ in range(size)]

    length = lib.resample_advised_length(factor)

    down = lib.resample_alloc_decimator(factor, length)
    up = lib.resample_alloc_interpolator(factor, length)
    try:
        fewer = decimate_block(lib, down, values)
        more = interpolate_block(lib, up, values, factor)
    finally:
        lib.resample_free(ctypes.byref(down))
        lib.resample_free(ctypes.byref(up))

    skip = length + factor
    assume(len(fewer) > (skip // factor + 4))

    for value in fewer[length // factor + 2:]:
        assert abs(value - level) <= (0.01 * abs(level))
    for value in more[skip:]:
        assert abs(value - level) <= (0.01 * abs(level))


@given(factor=factors, power=st.integers(min_value=-8, max_value=8))
def test_making_the_signal_larger_makes_the_answer_larger_by_as_much(
        lib, factor, power):
    """A resampler is a filter, and a filter is linear.

    The factor is a power of two, thus every step is exact and the two answers
    must agree to the last digit.
    """
    scale = float(2 ** power)
    size = 300
    values = tone(size, 0.03)
    scaled = [sp.to_float32(value * scale) for value in values]

    length = lib.resample_advised_length(factor)
    resample = lib.resample_alloc_decimator(factor, length)
    try:
        plain = decimate_block(lib, resample, values)
        lib.resample_reset(ctypes.byref(resample))
        large = decimate_block(lib, resample, scaled)
    finally:
        lib.resample_free(ctypes.byref(resample))

    assert len(plain) == len(large)
    for index in range(len(plain)):
        assert large[index] == sp.to_float32(plain[index] * scale)


@given(factor=factors, size=st.integers(min_value=1, max_value=200))
def test_feeding_a_block_is_the_same_as_feeding_the_samples_one_at_a_time(
        lib, factor, size):
    values = tone(size, 0.04)
    length = lib.resample_advised_length(factor)

    blockwise = lib.resample_alloc_decimator(factor, length)
    singly = lib.resample_alloc_decimator(factor, length)
    try:
        together = decimate_block(lib, blockwise, values)

        apart = []
        room = sptk.real_buffer(1)
        for value in values:
            if lib.resample_decimate(ctypes.byref(singly),
                                     sp.to_float32(value), room):
                apart.append(room[0])
    finally:
        lib.resample_free(ctypes.byref(blockwise))
        lib.resample_free(ctypes.byref(singly))

    assert together == apart


@given(factor=factors, size=st.integers(min_value=1, max_value=60))
def test_the_same_holds_for_the_interpolator(lib, factor, size):
    values = tone(size, 0.04)
    length = lib.resample_advised_length(factor)

    blockwise = lib.resample_alloc_interpolator(factor, length)
    singly = lib.resample_alloc_interpolator(factor, length)
    try:
        together = interpolate_block(lib, blockwise, values, factor)

        apart = []
        room = sptk.real_buffer(factor)
        for value in values:
            written = lib.resample_interpolate(ctypes.byref(singly),
                                               sp.to_float32(value), room)
            assert written == factor
            apart += list(room)[:written]
    finally:
        lib.resample_free(ctypes.byref(blockwise))
        lib.resample_free(ctypes.byref(singly))

    assert together == apart


@given(factor=factors)
def test_the_delay_is_half_the_filter_counted_at_the_rate_that_comes_out(
        lib, factor):
    length = lib.resample_advised_length(factor)
    resample = lib.resample_alloc_decimator(factor, length)
    try:
        assert (lib.resample_delay(ctypes.byref(resample))
                == (length // 2) // factor)
    finally:
        lib.resample_free(ctypes.byref(resample))


@given(factor=st.integers(min_value=0, max_value=64))
def test_a_factor_below_two_changes_nothing_and_is_refused(lib, factor):
    assert lib.resample_is_valid_factor(factor) == (factor >= 2)


@given(factor=st.integers(min_value=0, max_value=64))
def test_the_advised_length_is_always_odd_and_grows_with_the_factor(lib,
                                                                    factor):
    """The length must be odd or the filter has no middle coefficient, and
    without a middle it delays the frequencies by different amounts, which is
    the thing a resampler must never do.
    """
    length = lib.resample_advised_length(factor)

    if factor < 2:
        assert length == 0
        return

    assert length % 2 == 1
    assert length >= (33 * factor)
    assert length > lib.resample_advised_length(factor - 1)


@given(factor=factors, size=st.integers(min_value=1, max_value=200))
def test_a_reset_leaves_a_resampler_that_cannot_be_told_from_a_new_one(
        lib, factor, size):
    values = tone(size, 0.06)
    length = lib.resample_advised_length(factor)

    used = lib.resample_alloc_decimator(factor, length)
    fresh = lib.resample_alloc_decimator(factor, length)
    try:
        decimate_block(lib, used, values)
        lib.resample_reset(ctypes.byref(used))

        assert decimate_block(lib, used, values) == decimate_block(lib, fresh,
                                                                   values)
    finally:
        lib.resample_free(ctypes.byref(used))
        lib.resample_free(ctypes.byref(fresh))
