"""Rules that every window must keep, at every size.

The module makes three claims in its header that a handful of examples cannot
carry: that every window is symmetric, that a Tukey window at 0 and at 1 is
two other windows exactly, and that the three gains agree with one another.
These tests make those claims for every size.
"""

import os
import sys

from hypothesis import given
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

SIZES = st.integers(min_value=1, max_value=64)
KINDS = st.sampled_from(sptk.WINDOWS_WITHOUT_A_PARAMETER)


def built(lib, size, kind, parameter=0.0):
    """Give the values of a window as a list."""
    window = sptk.real_buffer(size)
    lib.window_build_with(window, size, kind, parameter)
    return list(window)


@given(SIZES, KINDS)
def test_every_window_is_symmetric(lib, size, kind):
    """The first value and the last must be the same, and so on inwards.

    A window that is not symmetric puts a phase into the answer of the
    transform that is not in the signal.
    """
    values = built(lib, size, kind)

    for index in range(size // 2):
        assert sp.close(values[index], values[size - 1 - index],
                        relative=1e-5, absolute=1e-6)


@given(SIZES, KINDS)
def test_no_window_reaches_outside_nothing_and_one(lib, size, kind):
    """A window weighs samples. It cannot weigh one more than wholly."""
    for value in built(lib, size, kind):
        assert -1e-6 <= value <= 1.0 + 1e-6


@given(st.integers(min_value=1, max_value=32), KINDS)
def test_a_window_of_an_odd_size_stands_at_one_in_the_middle(lib, half, kind):
    """Every window here is normalised, and an odd size has a middle sample.

    AN EVEN SIZE DOES NOT REACH 1, and that is right rather than a fault. A
    symmetric window of an even size has no sample at its middle: the peak
    falls between the two innermost samples, thus a hann window of 4 reaches
    0.75 and never 1. The larger the window, the closer the innermost sample
    comes to the peak.
    """
    size = (2 * half) + 1
    values = built(lib, size, kind)

    assert sp.close(values[size // 2], 1.0, relative=1e-4, absolute=1e-4)
    assert max(values) <= 1.0 + 1e-6


@given(KINDS)
def test_a_tapered_window_of_two_values_is_nothing_at_all(lib, kind):
    """THE DEGENERATE SIZE, PINNED SO THAT IT CANNOT SURPRISE ANYONE.

    A symmetric window of two values is its two ends, and the ends are where a
    taper is nothing. The values are right; two ends really are all there is.
    But the header tells a caller to DIVIDE by the coherent gain, and here that
    is a division by nothing.

    This test says what the module does today. It is here so that a change to
    it is a decision and not an accident.
    """
    values = built(lib, 2, kind)

    if kind == sptk.WINDOW_RECTANGULAR:
        assert values == [1.0, 1.0]
    else:
        # Every tapered window falls to nothing or very near it.
        assert max(abs(value) for value in values) < 0.1


@given(KINDS)
def test_a_window_of_one_value_is_that_one_value(lib, kind):
    """There is no shape to make, thus nothing is taken away."""
    assert sp.close(built(lib, 1, kind)[0], 1.0)


@given(SIZES, KINDS)
def test_one_value_of_a_window_agrees_with_the_whole_of_it(lib, size, kind):
    """window_value exists for a caller with no room to hold the window.

    It must give exactly what window_build would have written there.
    """
    values = built(lib, size, kind)

    for index in range(size):
        assert sp.close(lib.window_value(index, size, kind, 0.0),
                        values[index], relative=1e-5, absolute=1e-6)


@given(st.integers(min_value=2, max_value=64))
def test_a_tukey_window_at_the_two_ends_is_two_other_windows(lib, size):
    """The header says so, and here it is held.

    At a parameter of 0 nothing falls, thus the window is rectangular. At 1
    everything falls, thus it is a Hann window.
    """
    flat = built(lib, size, sptk.WINDOW_RECTANGULAR)
    hann = built(lib, size, sptk.WINDOW_HANN)

    at_zero = built(lib, size, sptk.WINDOW_TUKEY, 0.0)
    at_one = built(lib, size, sptk.WINDOW_TUKEY, 1.0)

    for index in range(size):
        assert sp.close(at_zero[index], flat[index], relative=1e-4,
                        absolute=1e-5)
        assert sp.close(at_one[index], hann[index], relative=1e-4,
                        absolute=1e-5)


@given(st.integers(min_value=2, max_value=64))
def test_a_kaiser_window_at_a_beta_of_nothing_is_rectangular(lib, size):
    """A beta of 0 asks for no shape at all, thus nothing is taken away."""
    flat = built(lib, size, sptk.WINDOW_RECTANGULAR)
    kaiser = built(lib, size, sptk.WINDOW_KAISER, 0.0)

    for index in range(size):
        assert sp.close(kaiser[index], flat[index], relative=1e-4,
                        absolute=1e-5)


@given(st.integers(min_value=3, max_value=64), KINDS)
def test_the_three_gains_agree_with_one_another(lib, size, kind):
    """The noise bandwidth is the two gains, and must not be a third answer.

    A window has one shape, thus the number of bins of noise that one bin holds
    is fixed by the coherent gain and the noise gain together. Working it out
    apart from them and getting a different answer would mean one of the three
    is wrong.

    From a size of 3, because at 2 a tapered window has no gain to divide by.
    """
    window = sptk.real_buffer(size)
    lib.window_build(window, size, kind)

    coherent = lib.window_coherent_gain(window, size)
    noise = lib.window_noise_gain(window, size)
    bandwidth = lib.window_noise_bandwidth(window, size)

    assert coherent > 0.0

    assert sp.close(bandwidth, (noise / coherent) ** 2, relative=1e-3,
                    absolute=1e-3)


@given(st.integers(min_value=2, max_value=64))
def test_a_rectangular_window_gains_nothing_and_loses_nothing(lib, size):
    """It takes nothing away, thus all three gains are 1."""
    window = sptk.real_buffer(size)
    lib.window_build(window, size, sptk.WINDOW_RECTANGULAR)

    assert sp.close(lib.window_coherent_gain(window, size), 1.0)
    assert sp.close(lib.window_noise_gain(window, size), 1.0)
    assert sp.close(lib.window_noise_bandwidth(window, size), 1.0)


@given(SIZES, KINDS, st.lists(sp.elements(50.0), min_size=1, max_size=64))
def test_laying_a_window_on_a_block_multiplies_it_value_by_value(lib, size,
                                                                 kind, block):
    """window_apply must be a multiplication and nothing else."""
    size = min(size, len(block))
    window = sptk.real_buffer(size)
    lib.window_build(window, size, kind)

    output = sptk.real_buffer(size)
    lib.window_apply(window, sptk.float_array(block[:size]), output, size)

    for index in range(size):
        assert sp.close(output[index],
                        sp.to_float32(window[index] * block[index]),
                        relative=1e-4, absolute=1e-4)


@given(SIZES, KINDS, st.lists(sp.elements(50.0), min_size=1, max_size=64))
def test_a_window_can_be_laid_on_a_block_in_place(lib, size, kind, block):
    """The header says the input and the output may be the same list."""
    size = min(size, len(block))
    window = sptk.real_buffer(size)
    lib.window_build(window, size, kind)

    apart = sptk.real_buffer(size)
    lib.window_apply(window, sptk.float_array(block[:size]), apart, size)

    together = sptk.float_array(block[:size])
    lib.window_apply(window, together, together, size)

    for index in range(size):
        assert sp.close(apart[index], together[index])


@given(st.integers(min_value=-4, max_value=10))
def test_only_the_kinds_that_exist_are_taken(lib, kind):
    """A kind outside the list must be refused, whatever number it carries."""
    assert lib.window_is_valid_kind(kind) == (0 <= kind <= sptk.WINDOW_KAISER)


@given(st.sampled_from([sptk.WINDOW_RECTANGULAR, sptk.WINDOW_HANN,
                        sptk.WINDOW_HAMMING, sptk.WINDOW_BLACKMAN,
                        sptk.WINDOW_BLACKMAN_HARRIS, sptk.WINDOW_TUKEY,
                        sptk.WINDOW_KAISER]))
def test_the_windows_that_take_a_parameter_are_the_two_that_have_a_shape(lib,
                                                                        kind):
    """Only Tukey and Kaiser follow a parameter; the rest have a fixed shape."""
    expected = kind in (sptk.WINDOW_TUKEY, sptk.WINDOW_KAISER)
    assert lib.window_takes_a_parameter(kind) == expected
