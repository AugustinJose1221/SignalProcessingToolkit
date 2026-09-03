"""Rules that a transform answering at every sample must keep.

The sliding transform is not an approximation of anything either. With its
damping switched off it is THE DISCRETE FOURIER TRANSFORM AT ONE BIN, over the
last N samples, worked out by a recurrence instead of by a sum. That is the
property this file is built around.

The recurrence is the whole risk: it carries its total forward for ever, thus
an error that goes in never leaves. Every rule below is therefore checked after
a long run and not after one window, because a rule that holds for one window
says nothing about the thing that can go wrong here.
"""

import cmath
import math
import os
import sys

from hypothesis import given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402

RUNS = settings(max_examples=30, deadline=None)

TWO_PI = 2.0 * math.pi

# Window sizes the transform can also take, so that the two can be set beside
# each other bin for bin.
WINDOWS = st.sampled_from([16, 32, 64, 128])


def _slide(lib, size, count, damping=1.0):
    """A watcher with the damping switched off unless asked otherwise."""
    watcher = lib.slide_alloc(size, count)
    assert lib.slide_design(watcher, sp.to_float32(damping))
    return watcher


def _run(lib, watcher, block):
    lib.slide_process_block(watcher, ffitt.float_array(block), len(block))


def _by_transform(block, bin_index):
    """The bin of the transform of exactly this block, worked out in Python."""
    size = len(block)
    total = sum(block[n] * cmath.exp(-1j * TWO_PI * bin_index * n / size)
                for n in range(size))
    return abs(total)


@RUNS
@given(size=WINDOWS, data=st.data())
def test_undamped_it_is_the_transform_of_the_window_it_holds(lib, size, data):
    """THE RULE THE MODULE EXISTS TO KEEP.

    After any number of samples, the total must be the transform of the LAST
    size of them, and of nothing before. A recurrence that carried anything of
    the older samples would fail this, and so would one that had drifted.
    """
    bin_index = data.draw(st.integers(min_value=1, max_value=size // 2 - 1))
    block = data.draw(st.lists(
        st.floats(min_value=-4.0, max_value=4.0, allow_nan=False,
                  allow_infinity=False),
        min_size=size * 4, max_size=size * 4))

    watcher = _slide(lib, size, 1)
    assert lib.slide_watch(watcher, 0, bin_index)

    _run(lib, watcher, block)

    got = float(lib.slide_magnitude(watcher, 0))
    wanted = _by_transform(block[-size:], bin_index)

    # The window's own size sets the scale that an error is measured against.
    scale = max(1.0, sum(abs(value) for value in block[-size:]))

    lib.slide_free(watcher)

    assert abs(got - wanted) <= 0.02 * scale


@RUNS
@given(size=WINDOWS, data=st.data())
def test_it_forgets_everything_older_than_its_window(lib, size, data):
    """Two runs that end with the same window must end with the same answer.

    What came before the window must leave no trace at all. This is the rule
    that a total which quietly kept a share of the older samples would break,
    and it is not visible in a single window.
    """
    bin_index = data.draw(st.integers(min_value=1, max_value=size // 2 - 1))
    window = data.draw(st.lists(
        st.floats(min_value=-4.0, max_value=4.0, allow_nan=False,
                  allow_infinity=False),
        min_size=size, max_size=size))
    before_one = data.draw(st.lists(
        st.floats(min_value=-4.0, max_value=4.0, allow_nan=False,
                  allow_infinity=False),
        min_size=size * 2, max_size=size * 2))
    before_two = data.draw(st.lists(
        st.floats(min_value=-4.0, max_value=4.0, allow_nan=False,
                  allow_infinity=False),
        min_size=size * 2, max_size=size * 2))

    first = _slide(lib, size, 1)
    second = _slide(lib, size, 1)
    lib.slide_watch(first, 0, bin_index)
    lib.slide_watch(second, 0, bin_index)

    _run(lib, first, before_one + window)
    _run(lib, second, before_two + window)

    one = float(lib.slide_magnitude(first, 0))
    two = float(lib.slide_magnitude(second, 0))
    scale = max(1.0, sum(abs(value) for value in window))

    lib.slide_free(first)
    lib.slide_free(second)

    assert abs(one - two) <= 0.02 * scale


@RUNS
@given(size=WINDOWS, level=st.floats(min_value=-50.0, max_value=50.0))
def test_a_steady_level_lands_in_bin_zero_and_nowhere_else(lib, size, level):
    """A signal that does not move holds one frequency: none at all.

    Bin 0 must carry the whole of it and every other bin must carry nothing.
    This catches a turning factor built at the wrong angle, which the rule
    above would not: a wrong angle still gives a total of the right size.
    """

    watcher = _slide(lib, size, 2)
    lib.slide_watch(watcher, 0, 0)
    lib.slide_watch(watcher, 1, size // 4)

    _run(lib, watcher, [level] * (size * 3))

    at_nothing = float(lib.slide_magnitude(watcher, 0))
    elsewhere = float(lib.slide_magnitude(watcher, 1))

    lib.slide_free(watcher)

    assert abs(at_nothing - abs(level) * size) <= 0.02 * (abs(level) * size + 1.0)
    assert elsewhere <= 0.02 * (abs(level) * size + 1.0)


@RUNS
@given(size=WINDOWS, damping=st.sampled_from([0.99, 0.999, 0.9999]),
       data=st.data())
def test_the_damping_only_ever_makes_the_answer_smaller(lib, size, damping, data):
    """The damping is a shrinking and never a growing.

    A watcher with damping must read at or below one without it, on the same
    samples. A damping that made an answer larger would not be a damping, and
    the sign of the arithmetic is easy to get the wrong way round.
    """
    bin_index = data.draw(st.integers(min_value=1, max_value=size // 2 - 1))
    block = data.draw(st.lists(
        st.floats(min_value=-4.0, max_value=4.0, allow_nan=False,
                  allow_infinity=False),
        min_size=size * 3, max_size=size * 3))

    plain = _slide(lib, size, 1, 1.0)
    damped = _slide(lib, size, 1, damping)
    lib.slide_watch(plain, 0, bin_index)
    lib.slide_watch(damped, 0, bin_index)

    _run(lib, plain, block)
    _run(lib, damped, block)

    undamped_answer = float(lib.slide_magnitude(plain, 0))
    damped_answer = float(lib.slide_magnitude(damped, 0))
    scale = max(1.0, sum(abs(value) for value in block[-size:]))

    lib.slide_free(plain)
    lib.slide_free(damped)

    assert damped_answer <= undamped_answer + (0.02 * scale)


@RUNS
@given(size=WINDOWS, damping=st.floats(min_value=-2.0, max_value=3.0))
def test_it_refuses_a_damping_outside_its_range_and_keeps_the_one_it_had(lib, 
        size, damping):
    """A refused design must change nothing.

    A module that refused and half applied the change would leave a watcher
    that neither the caller nor the module could describe.
    """

    watcher = _slide(lib, size, 1)
    lib.slide_watch(watcher, 0, 1)

    was = float(watcher.damping)
    allowed = lib.slide_is_valid_damping(damping)
    answered = lib.slide_design(watcher, damping)

    now = float(watcher.damping)

    lib.slide_free(watcher)

    assert answered == allowed

    if not allowed:
        assert now == was
