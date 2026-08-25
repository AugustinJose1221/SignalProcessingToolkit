"""Rules that a filter design must keep, for every specification.

The unit tests examine one specification apiece. These examine the whole space
of them, and the strongest test here is the one that generates a specification,
asks the module how many sections it needs, builds that filter and MEASURES
whether it really does what was asked.
"""

import ctypes
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

DESIGN = settings(max_examples=40)

RIPPLES = st.sampled_from([0.0625, 0.25, 0.5, 1.0, 2.0, 3.0])
ATTENUATIONS = st.sampled_from([20.0, 30.0, 40.0, 60.0, 70.0])
SHAPES = st.sampled_from(sptk.IIR_SHAPES)


def gains_across(lib, filter_handle, low, high, count=200):
    """Give the gain at places evenly spread across a band."""
    step = (high - low) / float(count)
    return [lib.iir_get_gain(ctypes.byref(filter_handle),
                             sp.to_float32(low + (step * index)))
            for index in range(count + 1)]


@st.composite
def specifications(draw):
    """Give a pass edge, a stop edge and the two ripples."""
    pass_edge = draw(st.sampled_from([0.05, 0.1, 0.15, 0.2, 0.25]))
    apart = draw(st.sampled_from([1.5, 2.0, 3.0]))
    stop_edge = pass_edge * apart
    assume(stop_edge < 0.45)
    return (pass_edge, stop_edge, draw(RIPPLES), draw(ATTENUATIONS))


@given(specifications(), SHAPES)
@DESIGN
def test_the_sections_it_asks_for_really_meet_the_specification(lib, spec,
                                                                shape):
    """THE STRONGEST TEST HERE.

    A number of sections that does not meet the specification is worse than no
    number at all, because a caller trusts it and never looks again. This
    generates a specification, asks how many sections it needs, builds exactly
    that filter and measures the band that is stopped.
    """
    pass_edge, stop_edge, ripple, attenuation = spec

    sections = lib.iir_sections_for(shape, sp.to_float32(pass_edge),
                                    sp.to_float32(stop_edge),
                                    sp.to_float32(ripple),
                                    sp.to_float32(attenuation))
    assume(sections > 0)
    # A very deep stop band at a very small gap asks for more sections than a
    # filter of biquads can hold steady, and the module is not claiming that.
    assume(sections <= 12)

    handle = lib.iir_alloc(sections)

    # For Chebyshev II the cutoff counts at the far edge; for the others at the
    # near one.
    cutoff = stop_edge if shape == sptk.IIR_CHEBYSHEV_II else pass_edge

    assume(lib.iir_design_low_pass_with(ctypes.byref(handle),
                                        sp.to_float32(cutoff), shape,
                                        sp.to_float32(ripple),
                                        sp.to_float32(attenuation)))

    wanted = 10.0 ** (-attenuation / 20.0)
    worst = max(gains_across(lib, handle, stop_edge, 0.5))

    lib.iir_free(ctypes.byref(handle))

    # HOW MUCH ROOM, AND WHY IT IS NOT THE SAME AT BOTH WIDTHS.
    #
    # An elliptic filter holds its band that is stopped down with notches, and
    # a notch must be placed exactly to reach all the way down. At 32 bits the
    # coefficients cannot always place them exactly and the floor sits up to
    # about 3 dB high, which the header of the module records. Every other
    # shape, and every shape at 64 bits, delivers what was asked.
    if (shape == sptk.IIR_ELLIPTIC) and not sptk.REAL_64:
        room = 10.0 ** (3.5 / 20.0)
    else:
        room = 1.05

    assert worst <= wanted * room


@given(specifications())
@DESIGN
def test_a_sharper_shape_never_needs_more_sections(lib, spec):
    """The shapes stand in an order, and the order must not turn over.

    A Chebyshev can do whatever a Butterworth does with the same order, and an
    elliptic whatever a Chebyshev does. Thus each in turn needs no more
    sections than the one before it.
    """
    pass_edge, stop_edge, ripple, attenuation = spec

    counts = [lib.iir_sections_for(shape, sp.to_float32(pass_edge),
                                   sp.to_float32(stop_edge),
                                   sp.to_float32(ripple),
                                   sp.to_float32(attenuation))
              for shape in sptk.IIR_SHAPES_BY_SHARPNESS]

    assume(all(count > 0 for count in counts))

    for gentler, sharper in zip(counts, counts[1:]):
        assert sharper <= gentler


@given(RIPPLES, st.integers(min_value=1, max_value=5),
       st.sampled_from([0.05, 0.1, 0.2]))
@DESIGN
def test_a_chebyshev_ripples_by_exactly_what_was_asked(lib, ripple, sections,
                                                       cutoff):
    """The whole point of asking for a ripple is getting that ripple.

    And the top of the ripple must stand at 1 and not above it: a filter that
    reached above 1 would be amplifying the band it is meant to pass.
    """
    handle = lib.iir_alloc(sections)

    assume(lib.iir_design_low_pass_with(ctypes.byref(handle),
                                        sp.to_float32(cutoff),
                                        sptk.IIR_CHEBYSHEV_I,
                                        sp.to_float32(ripple),
                                        sp.to_float32(60.0)))

    gains = gains_across(lib, handle, 0.0, cutoff)
    lib.iir_free(ctypes.byref(handle))

    largest = max(gains)
    smallest = min(gains)
    assume(smallest > 0.0)

    wanted = 10.0 ** (ripple / 20.0)

    assert abs((largest / smallest) - wanted) <= 0.02 * wanted
    assert largest <= 1.01


@given(RIPPLES, ATTENUATIONS, st.integers(min_value=2, max_value=5))
@DESIGN
def test_an_elliptic_ripples_at_both_ends_by_what_was_asked(lib, ripple,
                                                            attenuation,
                                                            sections):
    """THE SHAPE THAT RIPPLES AT BOTH ENDS.

    Both ripples must be what was asked. That is what makes a filter elliptic
    and not merely sharp, and it is the thing that is easy to get nearly right.
    """
    cutoff = 0.1
    handle = lib.iir_alloc(sections)

    assume(lib.iir_design_low_pass_with(ctypes.byref(handle),
                                        sp.to_float32(cutoff),
                                        sptk.IIR_ELLIPTIC,
                                        sp.to_float32(ripple),
                                        sp.to_float32(attenuation)))

    passing = gains_across(lib, handle, 0.0, cutoff)

    # Where the band that is stopped begins is not known ahead of time, thus
    # it is found by walking out until the answer has settled.
    stop_from = None
    wanted_stop = 10.0 ** (-attenuation / 20.0)
    for step in range(1, 400):
        place = cutoff + (step * 0.001)
        if place >= 0.5:
            break
        if lib.iir_get_gain(ctypes.byref(handle),
                            sp.to_float32(place)) <= wanted_stop:
            stop_from = place
            break

    stopped = (gains_across(lib, handle, stop_from, 0.5)
               if stop_from is not None else None)

    lib.iir_free(ctypes.byref(handle))

    largest = max(passing)
    smallest = min(passing)
    assume(smallest > 0.0)

    wanted = 10.0 ** (ripple / 20.0)

    assert abs((largest / smallest) - wanted) <= 0.05 * wanted
    assert largest <= 1.01

    assume(stopped is not None)

    # The same allowance the other test explains.
    room = 10.0 ** (3.5 / 20.0) if not sptk.REAL_64 else 1.1

    assert max(stopped) <= wanted_stop * room


@given(st.sampled_from([0.05, 0.1, 0.2, 0.3]), SHAPES,
       st.integers(min_value=1, max_value=4))
@DESIGN
def test_a_low_pass_passes_the_bottom_and_stops_the_top(lib, cutoff, shape,
                                                        sections):
    """Whatever the shape, a low pass must do what its name says.

    THE TWO PLACES ARE READ AT THE ENDS AND NOT AROUND THE CUTOFF, because the
    cutoff means a different thing for each shape: for Chebyshev II it is where
    the band that is STOPPED begins, thus a filter of one section is already
    well down at a quarter of it. What every shape must agree on is that
    nothing is taken away at the bottom and something is at the top.
    """
    handle = lib.iir_alloc(sections)

    assume(lib.iir_design_low_pass_with(ctypes.byref(handle),
                                        sp.to_float32(cutoff), shape,
                                        sp.to_float32(1.0),
                                        sp.to_float32(40.0)))

    at_nothing = lib.iir_get_gain(ctypes.byref(handle), sp.to_float32(0.0))
    at_the_top = lib.iir_get_gain(ctypes.byref(handle), sp.to_float32(0.49))

    lib.iir_free(ctypes.byref(handle))

    # At nothing every shape passes: Chebyshev I and elliptic start at the
    # bottom of their ripple, which for 1 dB is 0.891.
    assert at_nothing > 0.85

    # And at the very top every shape has taken the signal away.
    assert at_the_top < at_nothing
    assert at_the_top < 0.5


@given(st.sampled_from(sptk.WINDOWS_WITHOUT_A_PARAMETER),
       st.sampled_from([0.005, 0.01, 0.02, 0.05]))
@DESIGN
def test_the_length_it_asks_for_really_turns_that_fast(lib, kind, width):
    """The same rule as for the sections, on the other kind of filter."""
    length = lib.fir_length_for(kind, sp.to_float32(width))
    assume(length > 0)
    assume(length <= 1200)

    # Always odd, because a high pass needs a middle coefficient.
    assert length % 2 == 1

    handle = lib.fir_alloc(length)
    assume(lib.fir_design_low_pass_with(ctypes.byref(handle),
                                        sp.to_float32(0.25), kind,
                                        sp.to_float32(0.0)))

    leaves = None
    reaches = None
    for step in range(0, 8001):
        place = step / 16000.0
        gain = lib.fir_get_gain(ctypes.byref(handle), sp.to_float32(place))
        if gain >= 0.9:
            leaves = place
        if (leaves is not None) and (gain < 0.1):
            reaches = place
            break

    lib.fir_free(ctypes.byref(handle))

    assume(reaches is not None)

    # A very short filter carries the constant less exactly than a long one.
    assert abs((reaches - leaves) - width) <= width * 0.15


@given(st.sampled_from([0.005, 0.01, 0.02]))
def test_a_gentler_window_needs_a_longer_filter(lib, width):
    """The trade, seen through the length it costs."""
    order = [sptk.WINDOW_RECTANGULAR, sptk.WINDOW_HAMMING, sptk.WINDOW_HANN,
             sptk.WINDOW_BLACKMAN, sptk.WINDOW_BLACKMAN_HARRIS]
    lengths = [lib.fir_length_for(kind, sp.to_float32(width))
               for kind in order]

    for shorter, longer in zip(lengths, lengths[1:]):
        assert longer >= shorter


@given(st.sampled_from(sptk.WINDOWS_WITHOUT_A_PARAMETER),
       st.integers(min_value=3, max_value=400))
def test_the_turn_of_a_window_is_a_number_divided_by_the_length(lib, kind,
                                                                length):
    """Doubling the length must halve the turn and change nothing else."""
    one = lib.fir_transition_width(kind, length)
    two = lib.fir_transition_width(kind, length * 2)

    assert one > 0.0
    assert abs(two - (one / 2.0)) <= 1e-6


@given(st.integers(min_value=1, max_value=4),
       st.sampled_from([0.05, 0.1, 0.2]))
@DESIGN
def test_a_filter_holds_the_signal_back_and_never_pushes_it_forward(lib,
                                                                    sections,
                                                                    cutoff):
    """A filter cannot answer before it has been asked.

    The group delay is how long it holds the frequencies about a place, and no
    filter that runs forwards in time can hold them back by less than nothing.
    """
    handle = lib.iir_alloc(sections)

    assume(lib.iir_design_low_pass_with(ctypes.byref(handle),
                                        sp.to_float32(cutoff),
                                        sptk.IIR_BUTTERWORTH,
                                        sp.to_float32(1.0),
                                        sp.to_float32(60.0)))

    for step in range(1, 40):
        place = step / 100.0
        delay = lib.iir_group_delay(ctypes.byref(handle),
                                    sp.to_float32(place))
        assert delay > -0.01

    lib.iir_free(ctypes.byref(handle))
