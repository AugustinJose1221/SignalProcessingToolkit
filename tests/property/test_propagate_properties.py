"""Rules that carrying a state forward in time must keep.

The module offers three methods of rising cost and rising accuracy. What must
hold is not that any of them is right, because none of them is: it is that each
one is wrong in the way its order says it should be.
"""

import ctypes
import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402

RUNS = settings(max_examples=40, deadline=None)

METHODS = st.sampled_from([ffitt.PROPAGATE_EULER, ffitt.PROPAGATE_MIDPOINT,
                           ffitt.PROPAGATE_RUNGE])

# What each method promises: halving the step divides the error by this much.
# Euler is first order, midpoint second, Runge fourth.
ORDER = {
    ffitt.PROPAGATE_EULER: 1,
    ffitt.PROPAGATE_MIDPOINT: 2,
    ffitt.PROPAGATE_RUNGE: 4,
}


def decay_rate(strength):
    """A state that falls away at a known pace. Its answer is known exactly,
    which is what lets the error be measured rather than guessed."""

    def rate(time, state, given_input, out, count):
        for index in range(count):
            out[index] = -strength * state[index]

    return ffitt.RATE_FUNCTION(rate)


def turning_rate():
    """A pair that turns about the origin. Its length never changes, which is
    a rule that does not depend on knowing the answer."""

    def rate(time, state, given_input, out, count):
        out[0] = state[1]
        out[1] = -state[0]

    return ffitt.RATE_FUNCTION(rate)


def constant_rate(speed):
    """A state moving at a fixed pace. Every method must follow this one
    exactly, because there is nothing curved to cut a corner off."""

    def rate(time, state, given_input, out, count):
        for index in range(count):
            out[index] = speed

    return ffitt.RATE_FUNCTION(rate)


@given(METHODS, sp.elements(4.0),
       st.floats(min_value=0.25, max_value=4.0, width=32))
def test_a_state_moving_at_a_fixed_pace_is_followed_exactly(lib, method,
                                                            start, speed):
    """Every method takes the rate at one place or another and adds it up. With
    a rate that does not change there is nothing to be wrong about, thus a
    method that misses this one has its arithmetic wrong and not its order."""
    state = ffitt.float_array([start])
    step = sp.to_float32(0.125)
    steps = 20

    for index in range(steps):
        assert lib.propagate_state(method, constant_rate(speed),
                                   sp.to_float32(index * step), step, state,
                                   None, 1)

    expected = start + (speed * step * steps)

    assert abs(state[0] - expected) <= 1e-3 * (1.0 + abs(expected))


@given(METHODS, sp.elements(4.0), sp.elements(4.0))
@RUNS
def test_a_turning_pair_keeps_its_length(lib, method, first, second):
    """A rule that holds without knowing the answer. The pair turns about the
    origin, thus how far it stands from the origin cannot change. Each method
    lets it drift a little, and how much is what parts them."""
    assume(math.hypot(first, second) > 0.5)

    state = ffitt.float_array([first, second])
    began = math.hypot(first, second)

    step = sp.to_float32(0.03125)
    rate = turning_rate()

    for index in range(200):
        assert lib.propagate_state(method, rate, sp.to_float32(index * step),
                                   step, state, None, 2)

    now = math.hypot(state[0], state[1])

    # Euler always steps outwards along the line it is on, thus it spirals out
    # and is given room to. The other two hold far closer.
    room = 0.30 if method == ffitt.PROPAGATE_EULER else 0.02

    assert abs(now - began) <= room * began


@given(METHODS, st.floats(min_value=0.5, max_value=2.0, width=32))
@RUNS
def test_halving_the_step_makes_the_error_smaller_by_the_order(lib, method,
                                                               strength):
    """THE RULE THAT SAYS WHAT A METHOD IS FOR. A method of order n promises
    that halving the step divides the error by two to the n. Nothing else
    tells a costly method from a cheap one, and a method whose error fell no
    faster than Euler's would be four calls of the rate for nothing."""
    across = sp.to_float32(1.0)
    truth = math.exp(-strength * 1.0)

    rate = decay_rate(strength)

    def error(steps):
        state = ffitt.float_array([1.0])
        assert lib.propagate_state_over(method, rate, sp.to_float32(0.0),
                                        across, steps, state, None, 1)
        return abs(state[0] - truth)

    coarse = error(16)
    fine = error(32)

    # Below this the answer is as good as the width allows and the ratio
    # measures rounding rather than the method.
    assume(fine > 1e-6)

    promised = 2.0 ** ORDER[method]

    # Held loosely on purpose: the promise is about the step becoming small,
    # and at a step this size the next term still shows. What is being examined
    # is that a method of a higher order really does fall faster.
    assert coarse / fine >= promised * 0.7


@given(sp.elements(2.0), st.floats(min_value=0.5, max_value=2.0, width=32))
@RUNS
def test_a_costlier_method_is_not_worse_at_the_same_step(lib, start,
                                                         strength):
    """The reason to pay for Runge over Euler at the same step. If a method
    that asks for four rates were no better than one that asks for a single
    rate, there would be no reason to offer it."""
    assume(abs(start) > 0.25)

    across = sp.to_float32(1.0)
    steps = 8
    truth = start * math.exp(-strength * 1.0)
    rate = decay_rate(strength)

    def error(method):
        state = ffitt.float_array([start])
        assert lib.propagate_state_over(method, rate, sp.to_float32(0.0),
                                        across, steps, state, None, 1)
        return abs(state[0] - truth)

    euler = error(ffitt.PROPAGATE_EULER)
    midpoint = error(ffitt.PROPAGATE_MIDPOINT)
    runge = error(ffitt.PROPAGATE_RUNGE)

    room = 1e-6 * (1.0 + abs(truth))

    assert midpoint <= euler + room
    assert runge <= midpoint + room


@given(METHODS, st.integers(min_value=1, max_value=32),
       st.floats(min_value=0.5, max_value=2.0, width=32))
@RUNS
def test_going_across_in_steps_is_the_same_as_stepping_across(lib, method,
                                                              steps, strength):
    """propagate_state_over is propagate_state called in a row. If the two
    parted, one of them would be moving the time along differently, and a rate
    that depends on time would then be read at the wrong place."""
    across = sp.to_float32(1.0)
    step = sp.to_float32(across / steps)
    rate = decay_rate(strength)

    over = ffitt.float_array([1.0])
    assert lib.propagate_state_over(method, rate, sp.to_float32(0.0), across,
                                    steps, over, None, 1)

    apart = ffitt.float_array([1.0])
    for index in range(steps):
        assert lib.propagate_state(method, rate, sp.to_float32(index * step),
                                   step, apart, None, 1)

    assert abs(over[0] - apart[0]) <= 1e-4 * (1.0 + abs(over[0]))


@given(METHODS, st.lists(sp.elements(4.0), min_size=1, max_size=8))
@RUNS
def test_every_part_of_the_state_is_carried_alike(lib, method, values):
    """Each part falls away at the same pace, thus each must end at the SAME
    FRACTION of where it began, whatever fraction the method arrived at. What
    is examined here is not how accurate the method is: it is that every part
    was carried, that none was left where it was, and that none was written
    over by another. Measuring against the true answer instead would mix that
    in with the error of the method and hide it.

    A part carried alone is compared against a whole state carried together,
    because a fault that writes one part over another only shows when there is
    more than one part to confuse."""
    count = len(values)
    rate = decay_rate(sp.to_float32(1.0))

    together = ffitt.float_array(values)
    assert lib.propagate_state_over(method, rate, sp.to_float32(0.0),
                                    sp.to_float32(1.0), 20, together, None,
                                    count)

    for index, began in enumerate(values):
        alone = ffitt.float_array([began])
        assert lib.propagate_state_over(method, rate, sp.to_float32(0.0),
                                        sp.to_float32(1.0), 20, alone, None, 1)

        assert abs(together[index] - alone[0]) <= 1e-4 * (1.0 + abs(began))


@given(METHODS)
def test_each_method_says_how_many_rates_it_asks_for(lib, method):
    """The count is what the caller pays. It must agree with what the method
    really does, because a caller with a costly rate chooses on this number."""
    asks = {ffitt.PROPAGATE_EULER: 1, ffitt.PROPAGATE_MIDPOINT: 2,
            ffitt.PROPAGATE_RUNGE: 4}

    assert lib.propagate_asks_for_each_step(method) == asks[method]


@given(st.integers(min_value=-4, max_value=8))
def test_only_the_three_methods_are_taken(lib, method):
    assert lib.propagate_is_valid_method(method) == (0 <= method <= 2)


@given(st.integers(min_value=0, max_value=40))
def test_only_a_state_the_working_room_can_hold_is_taken(lib, count):
    """The methods keep their working room on the stack, thus the bound is
    what that room holds and not a matter of taste."""
    assert lib.propagate_is_valid_count(count) == (1 <= count <= 16)


@given(METHODS, sp.elements(4.0))
def test_a_state_the_room_cannot_hold_is_refused(lib, method, start):
    """Refused rather than cut short, because carrying part of a state forward
    and leaving the rest where it was gives an answer that looks like one."""
    count = 17
    state = ffitt.float_array([start] * count)

    assert not lib.propagate_state(method, constant_rate(sp.to_float32(1.0)),
                                   sp.to_float32(0.0), sp.to_float32(0.1),
                                   state, None, count)

    for index in range(count):
        assert state[index] == start


@given(METHODS, st.floats(min_value=-4.0, max_value=0.0, width=32),
       sp.elements(4.0))
def test_a_step_of_no_time_or_of_time_running_backwards_is_refused(lib, method,
                                                                   step,
                                                                   start):
    """A step of nothing does no work and a step below nothing asks the model
    to run backwards, which a model of a real thing is not written for. Both
    are refused rather than answered, and the state is left where it was so
    that a caller who ignored the answer still has what it began with."""
    state = ffitt.float_array([start])

    assert not lib.propagate_state(method, constant_rate(sp.to_float32(2.0)),
                                   sp.to_float32(0.0), sp.to_float32(step),
                                   state, None, 1)

    assert state[0] == start


@given(METHODS, st.integers(min_value=-4, max_value=0), sp.elements(4.0))
def test_going_across_no_time_or_in_no_steps_is_refused(lib, method, across,
                                                        start):
    """The same rule where the caller gives a whole span and a count of steps.
    A count of nothing would divide by nothing."""
    state = ffitt.float_array([start])

    assert not lib.propagate_state_over(method,
                                        constant_rate(sp.to_float32(2.0)),
                                        sp.to_float32(0.0),
                                        sp.to_float32(across), 4, state, None,
                                        1)

    assert not lib.propagate_state_over(method,
                                        constant_rate(sp.to_float32(2.0)),
                                        sp.to_float32(0.0),
                                        sp.to_float32(1.0), 0, state, None, 1)

    assert state[0] == start
