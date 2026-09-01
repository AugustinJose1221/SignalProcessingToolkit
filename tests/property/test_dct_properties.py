"""Rules that turning a signal into cosines must keep.

The transform is only worth having if it can be undone, and it is only worth
choosing over fft if it really does gather a smooth signal into fewer numbers.
Both are held here.
"""

import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402

RUNS = settings(max_examples=40, deadline=None)

# Any size at all and not only a power of two, which is what this offers over
# the transform.
SIZES = st.integers(min_value=1, max_value=64)


@st.composite
def signal(draw, size=None):
    if size is None:
        size = draw(SIZES)

    return draw(st.lists(sp.elements(8.0), min_size=size, max_size=size))


def forward(lib, values):
    out = ffitt.real_buffer(len(values))

    assert lib.dct_forward(ffitt.float_array(values), out, len(values))

    return [out[index] for index in range(len(values))]


def inverse(lib, values):
    out = ffitt.real_buffer(len(values))

    assert lib.dct_inverse(ffitt.float_array(values), out, len(values))

    return [out[index] for index in range(len(values))]


@given(signal())
@RUNS
def test_undoing_it_gives_back_what_went_in(lib, values):
    """THE RULE THAT MAKES IT A TRANSFORM. Anything that cannot be undone is a
    measurement and not a transform, and a compression built on it could not
    rebuild the signal."""
    back = inverse(lib, forward(lib, values))

    scale = 1.0 + max(abs(value) for value in values)

    for one, other in zip(values, back):
        assert abs(one - other) <= 1e-4 * scale


@given(signal())
@RUNS
def test_the_cosines_hold_as_much_as_the_signal(lib, values):
    """THE RULE THAT MAKES THE SHARES MEAN ANYTHING. The transform is written so
    that what the cosines add up to is what the signal adds up to. Without it,
    asking how much of a signal the first few numbers hold would be asking about
    a scale rather than about the signal."""
    cosines = forward(lib, values)

    in_signal = sum(value * value for value in values)
    in_cosines = sum(value * value for value in cosines)

    assert abs(in_signal - in_cosines) <= 1e-3 * (1.0 + in_signal)


@given(signal(), st.floats(min_value=0.25, max_value=8.0, width=32))
@RUNS
def test_the_answer_scales_with_the_signal(lib, values, louder):
    """Turning a signal up turns every cosine up by the same amount, thus how
    much of it the first few numbers hold does not move."""
    plain = forward(lib, values)
    scaled = forward(lib, [sp.to_float32(value * louder) for value in values])

    for one, other in zip(plain, scaled):
        room = 1e-3 * (1.0 + (abs(louder) * (1.0 + abs(one))))

        assert abs(other - (one * louder)) <= room


@given(SIZES, sp.elements(8.0))
@RUNS
def test_a_steady_signal_is_one_number(lib, size, level):
    """A signal that does not change is one cosine of no frequency and nothing
    else. Anything in the other numbers would mean the transform is finding
    changes that are not there."""
    cosines = forward(lib, [level] * size)

    assert abs(cosines[0] - (level * math.sqrt(size))) <= 1e-3 * (
        1.0 + abs(level) * math.sqrt(size))

    for index in range(1, size):
        assert abs(cosines[index]) <= 1e-3 * (1.0 + abs(level))


@given(st.integers(min_value=8, max_value=64))
@RUNS
def test_a_slow_signal_needs_fewer_numbers_than_noise(lib, size):
    """THE REASON THE MODULE EXISTS. A signal that changes slowly gathers into
    its first few numbers; a signal of noise has nothing to gather and needs
    nearly all of them. A transform that did not part the two would be no use
    for compression at all."""
    slow = []
    noise = []
    state = 1

    for index in range(size):
        part = index / size

        # A curve that does NOT come back to where it started, which is the
        # case the transform handles badly and this one handles well.
        slow.append(sp.to_float32(part + (0.3 * math.sin(2.0 * math.pi
                                                         * part))))

        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        noise.append(sp.to_float32((((state >> 16) % 2000) / 1000.0) - 1.0))

    for_slow = lib.dct_count_for_share(ffitt.float_array(forward(lib, slow)),
                                       size, sp.to_float32(0.99))
    for_noise = lib.dct_count_for_share(ffitt.float_array(forward(lib, noise)),
                                        size, sp.to_float32(0.99))

    assert 0 < for_slow <= 6
    assert for_noise > for_slow


@given(signal(), st.sampled_from([0.5, 0.9, 0.99, 1.0]))
@RUNS
def test_asking_for_a_larger_share_never_needs_fewer_numbers(lib, values,
                                                             share):
    """The numbers are counted from the largest end inwards, thus asking for
    more of the signal can only ever want more of them."""
    assume(sum(value * value for value in values) > 0.01)

    cosines = ffitt.float_array(forward(lib, values))
    size = len(values)

    fewer = lib.dct_count_for_share(cosines, size, sp.to_float32(share / 2.0))
    more = lib.dct_count_for_share(cosines, size, sp.to_float32(share))

    assert 0 < fewer <= more <= size


@given(signal(), st.sampled_from([0.25, 0.5, 0.9, 0.99, 1.0]))
@RUNS
def test_the_count_given_really_holds_the_share_asked_for(lib, values, share):
    """THE CONTRACT, held against the signal rather than against a guess. The
    count that comes back must be the smallest one whose numbers really carry
    the share, thus that many carry it and one fewer does not.

    NOT that the whole share needs every number: a signal that does not change
    puts everything into its first number, thus even the whole of it needs only
    one. Asking for the size back was asking for something untrue of half the
    signals there are."""
    cosines = forward(lib, values)
    size = len(values)

    whole = sum(value * value for value in cosines)
    assume(whole > 0.01)

    count = lib.dct_count_for_share(ffitt.float_array(cosines), size,
                                    sp.to_float32(share))

    assert 1 <= count <= size

    held = sum(cosines[index] ** 2 for index in range(count))

    # That many carry the share.
    assert held >= (share * whole) - (1e-4 * whole)

    # And one fewer does not, which is what makes it the SMALLEST count.
    if count > 1:
        fewer = sum(cosines[index] ** 2 for index in range(count - 1))

        assert fewer < (share * whole) + (1e-4 * whole)


@given(st.integers(min_value=0, max_value=2048))
def test_only_a_size_the_cost_is_worth_paying_at_is_taken(lib, size):
    """This works in time proportional to the square of the size, thus the
    bound is where that stops being worth paying."""
    assert lib.dct_is_valid_size(size) == (1 <= size <= 1024)


@given(signal(), st.sampled_from([-0.5, 0.0, 1.5]))
@RUNS
def test_a_share_that_is_not_a_share_is_refused(lib, values, share):
    """Nothing rather than a count of numbers for a share nobody can have."""
    cosines = ffitt.float_array(forward(lib, values))

    assert lib.dct_count_for_share(cosines, len(values),
                                   sp.to_float32(share)) == 0
