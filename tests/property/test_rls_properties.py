"""Rules that recursive least squares must keep.

The module carries the correlation matrix turned round, and the whole of its
behaviour rests on that matrix staying a real one. These tests drive it with
generated signals and hold what must be true whatever the signal was.
"""

import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

RUNS = settings(max_examples=30, deadline=None)

SAMPLES = 400

LENGTHS = st.integers(min_value=1, max_value=8)

# The floor is 0.9 and the ceiling 1.0. These are the values in between that a
# float of 32 bits holds exactly, so that the strategy is the same at either
# width.
FORGETTING = st.sampled_from([0.90625, 0.9375, 0.96875, 0.984375, 0.99609375,
                              1.0])


def noise(count, seed=1):
    """A repeatable rough signal, made here rather than in the library so that
    what the filter is fed does not depend on the library being right."""
    state = seed
    out = []

    for _ in range(count):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        out.append(sp.to_float32((((state >> 16) % 2000) / 1000.0) - 1.0))

    return out


def path(reference, taps):
    """Send the reference through a known path, which is what the filter is
    then asked to find."""
    out = []

    for index in range(len(reference)):
        total = 0.0
        for tap, weight in enumerate(taps):
            if index >= tap:
                total += weight * reference[index - tap]
        out.append(sp.to_float32(total))

    return out


def run(lib, length, forgetting, doubt, reference, wanted):
    filt = lib.rls_alloc(length)
    assert lib.rls_design(filt, forgetting, doubt)

    errors = []
    for index in range(len(reference)):
        errors.append(lib.rls_error(filt, reference[index], wanted[index]))

    return filt, errors


@given(LENGTHS, FORGETTING,
       st.floats(min_value=1.0, max_value=1024.0, width=32))
@RUNS
def test_the_matrix_stays_a_real_one_however_long_it_runs(lib, length,
                                                          forgetting, doubt):
    """THE RULE THE WHOLE MODULE RESTS ON. The matrix is held turned round and
    is written back at every sample. If it stops being a real matrix the filter
    gives numbers that are not numbers, and it must say so rather than carry
    on."""
    reference = noise(SAMPLES)
    wanted = path(reference, [0.6, -0.3, 0.2])

    filt, errors = run(lib, length, forgetting, doubt, reference, wanted)

    try:
        assert lib.rls_is_healthy(filt)

        for value in errors:
            assert math.isfinite(value)

        for index in range(length):
            assert math.isfinite(lib.rls_get_coefficient(filt, index))
    finally:
        lib.rls_free(filt)


@given(FORGETTING)
@RUNS
def test_a_filter_long_enough_finds_the_path_it_was_shown(lib, forgetting):
    """THE REASON THE MODULE EXISTS. Given a reference and that reference sent
    through a path, a filter at least as long as the path must end up holding
    the path."""
    taps = [0.6, -0.3, 0.2, 0.1]
    reference = noise(SAMPLES * 3)
    wanted = path(reference, taps)

    filt, errors = run(lib, len(taps), forgetting, 100.0, reference, wanted)

    try:
        assume(lib.rls_is_healthy(filt))

        for tap, weight in enumerate(taps):
            assert abs(lib.rls_get_coefficient(filt, tap) - weight) <= 0.02
    finally:
        lib.rls_free(filt)


@given(LENGTHS, FORGETTING)
@RUNS
def test_what_is_left_after_learning_is_smaller_than_before(lib, length,
                                                            forgetting):
    """A posteriori and a priori: the error worked out after the step must be
    no larger than the one before it, because the step was taken to make it
    smaller. A step that made it larger would be moving the wrong way."""
    reference = noise(SAMPLES)
    wanted = path(reference, [0.6, -0.3, 0.2])

    filt = lib.rls_alloc(length)

    try:
        assert lib.rls_design(filt, forgetting, 100.0)

        for index in range(SAMPLES):
            before = lib.rls_error(filt, reference[index], wanted[index])

            if not lib.rls_is_healthy(filt):
                break

            # What the filter would give for the same sample now that it has
            # taken its step. Reading it again would take another step, thus
            # the coefficients are read and the sum formed here.
            after = wanted[index]
            for tap in range(length):
                if index >= tap:
                    after -= (lib.rls_get_coefficient(filt, tap)
                              * reference[index - tap])

            assert abs(after) <= abs(before) + 1e-3
    finally:
        lib.rls_free(filt)


@given(LENGTHS, FORGETTING)
@RUNS
def test_the_same_signal_twice_gives_the_same_answer(lib, length, forgetting):
    """Nothing in the filter may depend on anything but what it was given. A
    second run that differed would mean it kept something it should not."""
    reference = noise(SAMPLES)
    wanted = path(reference, [0.5, 0.25])

    first, one = run(lib, length, forgetting, 100.0, reference, wanted)
    second, other = run(lib, length, forgetting, 100.0, reference, wanted)

    try:
        assert one == other
    finally:
        lib.rls_free(first)
        lib.rls_free(second)


@given(LENGTHS, FORGETTING)
@RUNS
def test_a_reset_filter_answers_as_a_new_one_does(lib, length, forgetting):
    """A reset must put back the state a new filter has, and not merely most
    of it. The matrix is the part that is easy to leave behind."""
    reference = noise(SAMPLES)
    wanted = path(reference, [0.5, 0.25])

    filt = lib.rls_alloc(length)

    try:
        assert lib.rls_design(filt, forgetting, 100.0)

        for index in range(SAMPLES):
            lib.rls_error(filt, reference[index], wanted[index])

        lib.rls_reset(filt)

        after_reset = [lib.rls_error(filt, reference[index], wanted[index])
                       for index in range(SAMPLES)]

        fresh, expected = run(lib, length, forgetting, 100.0, reference,
                              wanted)
        lib.rls_free(fresh)

        assert after_reset == expected
    finally:
        lib.rls_free(filt)


@given(LENGTHS, FORGETTING, sp.elements(4.0))
@RUNS
def test_the_error_and_the_output_are_the_two_halves_of_one_sample(lib, length,
                                                                   forgetting,
                                                                   level):
    """rls_error gives what is left; rls_process_sample gives what the filter
    made. Together they must be the sample that was wanted, or one of the two
    is describing a different filter."""
    reference = noise(SAMPLES)
    wanted = path(reference, [0.6, -0.3])

    one = lib.rls_alloc(length)
    other = lib.rls_alloc(length)

    try:
        assert lib.rls_design(one, forgetting, 100.0)
        assert lib.rls_design(other, forgetting, 100.0)

        for index in range(SAMPLES):
            left = lib.rls_error(one, reference[index], wanted[index])
            made = lib.rls_process_sample(other, reference[index],
                                          wanted[index])

            assume(lib.rls_is_healthy(one))

            scale = 1.0 + abs(wanted[index])
            assert abs((made + left) - wanted[index]) <= 1e-3 * scale
    finally:
        lib.rls_free(one)
        lib.rls_free(other)


@given(st.floats(min_value=-2.0, max_value=4.0, width=32))
def test_only_forgetting_between_the_two_bounds_is_taken(lib, forgetting):
    """Below the floor the filter throws the past away faster than it gathers
    it and the matrix runs away. Above one it would weigh the past more than
    the present, which is not forgetting at all."""
    expected = 0.9 <= forgetting <= 1.0

    assert lib.rls_is_valid_forgetting(sp.to_float32(forgetting)) == expected


@given(LENGTHS, FORGETTING)
@RUNS
def test_a_reference_of_nothing_leaves_the_filter_where_it_was(lib, length,
                                                               forgetting):
    """With nothing coming in there is nothing to learn from, whatever is
    wanted. A filter that moved would be learning from the doubt in its own
    matrix rather than from a signal."""
    filt = lib.rls_alloc(length)

    try:
        assert lib.rls_design(filt, forgetting, 100.0)

        for _ in range(100):
            left = lib.rls_error(filt, sp.to_float32(0.0), sp.to_float32(1.0))
            assert abs(left - 1.0) <= 1e-5

        for index in range(length):
            assert abs(lib.rls_get_coefficient(filt, index)) <= 1e-6
    finally:
        lib.rls_free(filt)
