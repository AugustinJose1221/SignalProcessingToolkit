"""Rules that a filter that finds its own coefficients must keep.

An adaptive filter is judged by one thing: GIVEN A REFERENCE AND THAT REFERENCE
SENT THROUGH SOME PATH, DOES IT FIND THE PATH. Everything a caller uses it for --
cancelling an interference, following an echo, matching a channel -- is that
question in another form.

The tests below are built on it, and on the two rules that make the three
learning laws different from one another.
"""

import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402

RUNS = settings(max_examples=30, deadline=None)

RULES = st.sampled_from(ffitt.ADAPTIVE_RULES)

# Rates a float of 32 bits holds exactly.
RATES = st.sampled_from([0.001953125, 0.0078125, 0.03125, 0.125])


def noise(count, seed=1):
    state = seed
    out = []

    for _ in range(count):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        out.append(sp.to_float32((((state >> 16) % 20000) / 10000.0) - 1.0))

    return out


def path(reference, taps):
    out = []

    for index in range(len(reference)):
        total = 0.0

        for tap, weight in enumerate(taps):
            if index >= tap:
                total += weight * reference[index - tap]

        out.append(sp.to_float32(total))

    return out


def designed(lib, length, rule, rate):
    filt = lib.adaptive_alloc(length)

    assert lib.adaptive_design(filt, rule, sp.to_float32(rate))

    return filt


@given(RULES)
@RUNS
def test_it_finds_the_path_it_was_shown(lib, rule):
    """THE QUESTION THE MODULE EXISTS TO ANSWER. Given a reference and that
    reference sent through a path, a filter at least as long as the path must
    end up HOLDING the path. Not merely making the error small: the
    coefficients themselves must be the path, because that is what a caller
    reading them off relies on."""
    taps = [0.6, -0.3, 0.2]

    reference = noise(60000)
    wanted = path(reference, taps)

    # A rate each rule is stable and reasonably quick at.
    rate = {ffitt.ADAPTIVE_LMS: 0.05,
            ffitt.ADAPTIVE_NORMALISED: 0.5,
            ffitt.ADAPTIVE_SIGN: 0.01}[rule]

    filt = designed(lib, len(taps), rule, rate)

    try:
        assert lib.adaptive_process_block(filt, ffitt.float_array(reference),
                                          ffitt.float_array(wanted), None,
                                          None, len(reference))

        for tap, weight in enumerate(taps):
            assert abs(lib.adaptive_get_coefficient(filt, tap)
                       - weight) <= 0.05
    finally:
        lib.adaptive_free(filt)


@given(st.sampled_from([ffitt.ADAPTIVE_LMS, ffitt.ADAPTIVE_NORMALISED]),
       RATES, st.integers(min_value=1, max_value=8))
@RUNS
def test_what_is_left_after_the_step_is_no_larger_than_before_it(lib, rule,
                                                                 rate,
                                                                 length):
    """A PRIORI AND A POSTERIORI. The step is taken to make the error smaller,
    thus a step that made it larger has overshot.

    HELD OF THE TWO RULES THAT PROMISE IT AND NOT OF THE THIRD. The plain and
    the normalised rules move by an amount PROPORTIONAL TO THE ERROR, thus as
    the error falls so does the step and the two can never cross. The sign rule
    moves by a fixed amount whatever the error is, thus once the error is
    smaller than one step it steps past it every time. That is not a fault in
    it and the test below says what it does instead.

    Held at every rate the module takes and not only at a safe one."""
    reference = noise(2000)
    wanted = path(reference, [0.6, -0.3])

    filt = designed(lib, length, rule, rate)

    try:
        for index in range(len(reference)):
            before = lib.adaptive_error(filt, reference[index], wanted[index])

            # What the filter would say for the same sample now that it has
            # taken its step. Reading it again would take another step, thus
            # the coefficients are read and the sum formed here.
            after = wanted[index]

            for tap in range(length):
                if index >= tap:
                    after -= (lib.adaptive_get_coefficient(filt, tap)
                              * reference[index - tap])

            assert abs(after) <= abs(before) + 1e-3
    finally:
        lib.adaptive_free(filt)


@given(RATES, st.floats(min_value=0.125, max_value=8.0, width=32))
@RUNS
def test_the_normalised_rule_does_not_care_how_loud_the_reference_is(
        lib, rate, louder):
    """WHAT THE NORMALISED RULE IS FOR, AND THE ONLY THING THAT PARTS IT FROM
    THE PLAIN ONE.

    The plain rule moves by the rate multiplied by the reference, thus a
    reference ten times as loud takes steps a hundred times as large and the
    filter runs away. The normalised rule divides by the energy of what is in
    the filter, thus the step is the same size whatever the reference is scaled
    to and the rate means what it says.

    Held by giving two filters the same problem at two loudnesses: the
    coefficients they end on must be the same, because a path is a path however
    loudly it is driven."""
    taps = [0.5, -0.25]

    reference = noise(20000)
    wanted = path(reference, taps)

    louder_reference = [sp.to_float32(value * louder) for value in reference]
    louder_wanted = [sp.to_float32(value * louder) for value in wanted]

    quiet = designed(lib, len(taps), ffitt.ADAPTIVE_NORMALISED, rate)
    loud = designed(lib, len(taps), ffitt.ADAPTIVE_NORMALISED, rate)

    try:
        for filt, ref, want in ((quiet, reference, wanted),
                                (loud, louder_reference, louder_wanted)):
            assert lib.adaptive_process_block(filt, ffitt.float_array(ref),
                                              ffitt.float_array(want), None,
                                              None, len(ref))

        for tap in range(len(taps)):
            assert abs(lib.adaptive_get_coefficient(quiet, tap)
                       - lib.adaptive_get_coefficient(loud, tap)) <= 0.02
    finally:
        lib.adaptive_free(quiet)
        lib.adaptive_free(loud)


@given(RULES, RATES, st.sampled_from([0.03125, 0.125, 0.5]))
@RUNS
def test_a_leak_pulls_the_coefficients_back_towards_nothing(lib, rule, rate,
                                                            leak):
    """WHAT THE LEAK IS FOR. A filter driven by a reference that holds only a
    few frequencies has directions nothing ever pushes on, and a coefficient
    left in such a direction wanders off on rounding alone. The leak pulls every
    coefficient a little way back at each step.

    Held where nothing at all is arriving: with no reference to learn from, a
    filter that has learned something must forget it."""
    taps = [0.6, -0.3]
    reference = noise(4000)
    wanted = path(reference, taps)

    filt = designed(lib, len(taps), rule, rate)

    try:
        assert lib.adaptive_set_leak(filt, sp.to_float32(leak))

        assert lib.adaptive_process_block(filt, ffitt.float_array(reference),
                                          ffitt.float_array(wanted), None,
                                          None, len(reference))

        started = [abs(lib.adaptive_get_coefficient(filt, tap))
                   for tap in range(len(taps))]

        assume(max(started) > 0.01)

        # Nothing arriving at all, for a long time.
        quiet = ffitt.float_array([0.0] * 20000)

        assert lib.adaptive_process_block(filt, quiet, quiet, None, None,
                                          20000)

        for tap in range(len(taps)):
            assert abs(lib.adaptive_get_coefficient(filt, tap)) < started[tap]
    finally:
        lib.adaptive_free(filt)


@given(RULES, RATES, st.integers(min_value=1, max_value=8))
@RUNS
def test_a_filter_with_no_leak_stands_still_when_nothing_arrives(lib, rule,
                                                                 rate,
                                                                 length):
    """The other side of the same rule. With no reference there is nothing to
    learn from, thus a filter that moved would be learning from its own
    rounding rather than from a signal."""
    filt = designed(lib, length, rule, rate)

    try:
        reference = noise(2000)
        wanted = path(reference, [0.6, -0.3])

        assert lib.adaptive_process_block(filt, ffitt.float_array(reference),
                                          ffitt.float_array(wanted), None,
                                          None, len(reference))

        # THE HISTORY MUST BE LET DRAIN FIRST. The filter holds the last few
        # samples of the reference, thus for that many samples after the
        # reference stops it is still making an output from what it holds, and
        # still learning from the error of it. It is standing still only once
        # the history is empty. Read before that, a filter with no leak at all
        # looks as though it were leaking.
        drain = ffitt.float_array([0.0] * (length + 2))

        assert lib.adaptive_process_block(filt, drain, drain, None, None,
                                          length + 2)

        held = [lib.adaptive_get_coefficient(filt, tap)
                for tap in range(length)]

        quiet = ffitt.float_array([0.0] * 5000)

        assert lib.adaptive_process_block(filt, quiet, quiet, None, None, 5000)

        for tap in range(length):
            assert abs(lib.adaptive_get_coefficient(filt, tap)
                       - held[tap]) <= 1e-5
    finally:
        lib.adaptive_free(filt)


@given(RULES, RATES, st.integers(min_value=1, max_value=8))
@RUNS
def test_the_error_and_the_output_are_the_two_halves_of_one_sample(lib, rule,
                                                                   rate,
                                                                   length):
    """The output is the interference as the filter learned it and the error is
    what is left. Together they must be the sample that was wanted, or one of
    the two is describing a different filter."""
    reference = noise(1000)
    wanted = path(reference, [0.6, -0.3])

    one = designed(lib, length, rule, rate)
    other = designed(lib, length, rule, rate)

    try:
        for index in range(len(reference)):
            left = lib.adaptive_error(one, reference[index], wanted[index])
            made = lib.adaptive_process_sample(other, reference[index],
                                               wanted[index])

            scale = 1.0 + abs(wanted[index])

            assert abs((made + left) - wanted[index]) <= 1e-3 * scale
    finally:
        lib.adaptive_free(one)
        lib.adaptive_free(other)


@given(RULES, RATES, st.integers(min_value=1, max_value=8))
@RUNS
def test_a_reset_filter_answers_as_a_new_one_does(lib, rule, rate, length):
    """The coefficients and the history are the whole of its memory."""
    reference = noise(1000)
    wanted = path(reference, [0.5, 0.25])

    used = designed(lib, length, rule, rate)
    fresh = designed(lib, length, rule, rate)

    try:
        assert lib.adaptive_process_block(used, ffitt.float_array(reference),
                                          ffitt.float_array(wanted), None,
                                          None, len(reference))

        lib.adaptive_reset(used)

        for filt in (used, fresh):
            assert lib.adaptive_process_block(filt,
                                              ffitt.float_array(reference),
                                              ffitt.float_array(wanted), None,
                                              None, len(reference))

        for tap in range(length):
            assert (lib.adaptive_get_coefficient(used, tap)
                    == lib.adaptive_get_coefficient(fresh, tap))
    finally:
        lib.adaptive_free(used)
        lib.adaptive_free(fresh)


@given(st.sampled_from([0.001953125, 0.0078125, 0.03125]))
@RUNS
def test_the_sign_rule_hunts_by_an_amount_that_follows_its_rate(lib, rate):
    """WHAT THE SIGN RULE DOES INSTEAD OF SETTLING, and the thing to know
    before choosing it.

    It moves by a fixed amount in the direction the sign of the error points,
    thus it never stops moving: once it is near the answer it steps past it,
    turns round, and steps past it again. What is left is not an error that
    falls away but one that HUNTS, by an amount that follows the rate directly.

    Measured on a path of two taps over sixty thousand samples, the coefficient
    wandered by 0.0066, 0.026 and 0.106 at rates of 0.002, 0.008 and 0.031: four
    times the rate, four times the wander. The plain and normalised rules
    wandered by 0.00005 at the middle rate, five hundred times less.

    That is the trade the sign rule offers: it needs no multiplication to take
    its step, and it never arrives."""
    taps = [0.6, -0.3]

    reference = noise(60000)
    wanted = path(reference, taps)

    def wander_of(rule, at):
        filt = designed(lib, len(taps), rule, at)

        try:
            assert lib.adaptive_process_block(filt,
                                              ffitt.float_array(reference),
                                              ffitt.float_array(wanted), None,
                                              None, len(reference))

            seen = []

            for index in range(50000, len(reference)):
                lib.adaptive_error(filt, reference[index], wanted[index])
                seen.append(lib.adaptive_get_coefficient(filt, 0))

            return max(seen) - min(seen)
        finally:
            lib.adaptive_free(filt)

    hunting = wander_of(ffitt.ADAPTIVE_SIGN, rate)
    settled = wander_of(ffitt.ADAPTIVE_NORMALISED, rate)

    # It really is still moving, and by far more than a rule that settles.
    assert hunting > 0.001
    assert hunting > (10.0 * settled)

    # And the amount follows the rate: four times the rate, about four times
    # the wander.
    faster = wander_of(ffitt.ADAPTIVE_SIGN, rate * 4.0)

    assert 2.0 < (faster / hunting) < 8.0


@given(st.integers(min_value=-4, max_value=8))
def test_only_the_three_rules_are_taken(lib, rule):
    assert lib.adaptive_is_valid_rule(rule) == (0 <= rule <= 2)


@given(RULES, st.floats(min_value=-2.0, max_value=4.0, width=32))
def test_only_a_leak_between_nothing_and_one_is_taken(lib, rule, leak):
    """A leak of one would throw the whole coefficient away at every step, and
    one above it would turn its sign."""
    filt = designed(lib, 4, rule, 0.01)

    try:
        assert lib.adaptive_set_leak(filt, sp.to_float32(leak)) == (
            0.0 <= leak <= 1.0)
    finally:
        lib.adaptive_free(filt)
