"""Rules that measuring a delay must keep.

The two ways are held to the same claim: given a reading and that reading
delayed by a known amount, give back that amount. What differs is how close
each gets and what each asks of the reading, and both of those are held here.
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

RUNS = settings(max_examples=30, deadline=None)

SIZE = 512
LARGEST_LAG = 48

LOWEST_BIN = 20
HIGHEST_BIN = 60

TWO_PI = 2.0 * math.pi

# Delays a float of 32 bits holds exactly, so that the strategy asks for the
# same numbers at either width.
DELAYS = st.sampled_from([-11.0, -6.5, -2.25, 0.0, 0.125, 3.0, 5.75, 9.0,
                          14.5])


def build_pair(behind, noise_level=0.0, seed=3):
    """A reading and that reading delayed, built from where the signal stands
    in time so that a delay of a part of a sample is a real delay.

    THE BAND IS FILLED BIN BY BIN. The way that reads the phase works from how
    far the phase turns from one bin to the NEXT, thus a reading of a few tones
    far apart leaves it nothing to work with. Every bin of the band is given a
    phase of its own, which is what an ordinary rush of noise looks like."""
    state = seed
    phases = {}

    for bin_index in range(LOWEST_BIN, HIGHEST_BIN + 1):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        phases[bin_index] = TWO_PI * (((state >> 16) % 20000) / 10000.0 - 1.0)

    first = []
    second = []

    for index in range(SIZE):
        at = float(index)
        back = at - behind

        one = 0.0
        other = 0.0

        for bin_index in range(LOWEST_BIN, HIGHEST_BIN + 1):
            turn = TWO_PI * bin_index / SIZE

            one += math.sin((turn * at) + phases[bin_index])
            other += math.sin((turn * back) + phases[bin_index])

        if noise_level > 0.0:
            state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
            one += noise_level * ((((state >> 16) % 20000) / 10000.0) - 1.0)
            state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
            other += noise_level * ((((state >> 16) % 20000) / 10000.0) - 1.0)

        first.append(sp.to_float32(one))
        second.append(sp.to_float32(other))

    return sptk.float_array(first), sptk.float_array(second)


def by_correlation(lib, first, second, strength=False):
    work = sptk.real_buffer(2 * LARGEST_LAG + 1)
    found = sptk.real_buffer(1)
    how_much = sptk.real_buffer(1) if strength else None

    assert lib.delay_by_correlation(first, second, SIZE, LARGEST_LAG, work,
                                    found, how_much)

    return (found[0], how_much[0]) if strength else found[0]


def by_phase(lib, first, second):
    fft = lib.fft_alloc(SIZE)
    one = (sptk.Cnum * SIZE)()
    other = (sptk.Cnum * SIZE)()
    found = sptk.real_buffer(1)

    try:
        assert lib.delay_by_phase(first, second, SIZE, fft, one, other, found)
        return found[0]
    finally:
        lib.fft_free(fft)


@given(DELAYS)
@RUNS
def test_the_phase_gives_back_the_delay_it_was_given(lib, behind):
    """THE REASON THE MODULE EXISTS. The reading is the other reading delayed
    by a known amount, and the answer must be that amount, whether or not it is
    a whole number of samples."""
    first, second = build_pair(behind)

    assert abs(by_phase(lib, first, second) - behind) <= 0.02


@given(DELAYS)
@RUNS
def test_the_correlation_gives_back_the_delay_within_a_fifth_of_a_sample(
        lib, behind):
    """The same claim of the cheaper way, held to what it can do. The curve
    fitted through three points is not the shape of the peak, thus this way
    leans towards the nearer neighbour. It is still far closer than the nearest
    whole sample, which is the whole reason to fit anything."""
    first, second = build_pair(behind)

    assert abs(by_correlation(lib, first, second) - behind) <= 0.2


@given(DELAYS)
@RUNS
def test_swapping_the_two_readings_turns_the_answer_round(lib, behind):
    """A delay is a statement about a pair in an order. Asked the other way
    round, the same pair must give the same number with its sign turned, or the
    two answers would be describing different things."""
    first, second = build_pair(behind)

    one_way = by_phase(lib, first, second)
    other_way = by_phase(lib, second, first)

    assert abs(one_way + other_way) <= 0.02

    one_way = by_correlation(lib, first, second)
    other_way = by_correlation(lib, second, first)

    assert abs(one_way + other_way) <= 0.05


@given(DELAYS)
@RUNS
def test_a_reading_against_itself_stands_behind_itself_by_nothing(lib, behind):
    """The one answer that cannot be argued with, and the one a sign error
    hides in: a reading and a copy of it are not delayed at all, whichever way
    round they are given."""
    first, _ = build_pair(behind)

    assert abs(by_phase(lib, first, first)) <= 0.001

    found, strength = by_correlation(lib, first, first, strength=True)

    assert abs(found) <= 0.001
    # And a reading agrees with itself perfectly.
    assert strength > 0.99


@given(DELAYS)
@RUNS
def test_the_phase_stands_closer_than_the_correlation(lib, behind):
    """The reason to pay for a transform. A way that asked for a transform and
    was no closer than one that did not would be there for nothing."""
    # Where the delay is a whole number of samples both land on it exactly and
    # there is nothing to tell apart.
    assume(abs(behind - round(behind)) > 0.1)

    first, second = build_pair(behind)

    assert (abs(by_phase(lib, first, second) - behind)
            < abs(by_correlation(lib, first, second) - behind))


@given(DELAYS, st.sampled_from([0.5, 1.0, 2.0]))
@RUNS
def test_the_delay_survives_noise_on_both_readings(lib, behind, level):
    """Noise on each reading separately, which is what two microphones give.
    It has nothing in common between the two, thus it cannot move where they
    agree; it can only make the agreement less certain.

    The room given grows with the noise, because it must. The noise here fills
    the whole band while the signal fills part of it, thus most bins hold noise
    alone and the phase step across those bins is whatever the noise made it.
    The way that reads the phase weighs every bin, and it is pulled by the ones
    that say nothing."""
    first, second = build_pair(behind, noise_level=level)

    room = 0.2 + (0.25 * level)

    assert abs(by_phase(lib, first, second) - behind) <= room
    assert abs(by_correlation(lib, first, second) - behind) <= room + 0.3


@given(DELAYS, st.floats(min_value=0.25, max_value=8.0, width=32))
@RUNS
def test_the_delay_does_not_depend_on_how_loud_either_reading_is(lib, behind,
                                                                 louder):
    """A delay is a statement about time. Turning one reading up says nothing
    about time, thus the answer must not move."""
    first, second = build_pair(behind)

    big = sptk.float_array([sp.to_float32(second[index] * louder)
                            for index in range(SIZE)])

    assert abs(by_phase(lib, first, second)
               - by_phase(lib, first, big)) <= 0.01
    assert abs(by_correlation(lib, first, second)
               - by_correlation(lib, first, big)) <= 0.01


@given(DELAYS)
@RUNS
def test_two_readings_with_nothing_in_common_say_so(lib, behind):
    """THE STRENGTH MUST BE READ, and it is only worth reading if it falls when
    it should. A pair with nothing in common still has a place where it agrees
    best, and the delay to that place is a number with nothing behind it."""
    first, _ = build_pair(behind, seed=3)
    _, unrelated = build_pair(behind, seed=8191)

    _, strength = by_correlation(lib, first, unrelated, strength=True)

    assert strength < 0.5


@st.composite
def a_list_with_a_peak_in_it(draw):
    """Build a list that HOLDS a peak rather than asking for lists until one
    of them happens to. Asking threw away fifty lists for every three it kept,
    which makes the generated values a poor spread of what they should be."""
    values = draw(st.lists(sp.elements(4.0), min_size=3, max_size=32))
    peak = draw(st.integers(min_value=1, max_value=len(values) - 2))

    above = draw(st.floats(min_value=0.0625, max_value=4.0, width=32))

    values[peak] = sp.to_float32(max(values[peak - 1], values[peak + 1])
                                 + above)

    return values, peak


@given(a_list_with_a_peak_in_it())
def test_a_refined_peak_never_leaves_the_step_it_was_found_in(lib, made):
    """The refinement says where the top of a curve through three points
    stands, and that top lies between the two neighbours. An answer outside
    that would put the peak past a point that is lower than the peak."""
    values, peak = made

    within = lib.delay_refine_peak(sptk.float_array(values), len(values), peak)

    assert -0.5 <= within <= 0.5


@given(st.lists(sp.elements(4.0), min_size=3, max_size=32))
def test_a_peak_with_even_neighbours_needs_no_refining(lib, values):
    """Where the two neighbours stand at the same height the curve through them
    is even about the middle, thus its top is the middle."""
    count = len(values)

    values[0] = sp.to_float32(values[2])
    values[1] = sp.to_float32(abs(values[0]) + 1.0)

    assert abs(lib.delay_refine_peak(sptk.float_array(values), count, 1)) <= 1e-5


@given(st.lists(sp.elements(4.0), min_size=3, max_size=32))
def test_there_is_nothing_to_refine_at_either_end(lib, values):
    """There are not three points at an end."""
    count = len(values)
    given_values = sptk.float_array(values)

    assert lib.delay_refine_peak(given_values, count, 0) == 0.0
    assert lib.delay_refine_peak(given_values, count, count - 1) == 0.0


@given(st.integers(min_value=-4, max_value=8))
def test_only_the_two_ways_are_taken(lib, way):
    assert lib.delay_is_valid_way(way) == (0 <= way <= 1)


@given(DELAYS)
@RUNS
def test_a_search_that_does_not_fit_inside_the_reading_is_refused(lib, behind):
    """A largest lag as long as the reading leaves no overlap to agree on, and
    a lag of nothing leaves no range to search."""
    first, second = build_pair(behind)

    work = sptk.real_buffer(2 * SIZE + 1)
    found = sptk.real_buffer(1)
    found[0] = sp.to_float32(77.0)

    assert not lib.delay_by_correlation(first, second, SIZE, SIZE, work, found,
                                        None)
    assert not lib.delay_by_correlation(first, second, SIZE, 0, work, found,
                                        None)
    assert found[0] == 77.0
