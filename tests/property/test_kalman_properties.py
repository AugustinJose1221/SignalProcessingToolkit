"""Properties of the Kalman filter."""

import ctypes
import math
import os
import sys

from hypothesis import given
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ffitt  # noqa: E402
import strategies as sp  # noqa: E402

REFERENCE = ctypes.byref

# The smallest value must have an exact form as a float of 32 bits, thus it is
# a power of two and not 0.01.
positive = st.floats(min_value=0.015625, max_value=100.0, width=32)
level = st.floats(min_value=-100.0, max_value=100.0, width=32)


def make_scalar_filter(lib, state, covariance, process_noise, measurement_noise):
    """Give a filter for a constant value with one state element."""
    kalman = lib.kalman_alloc(1, 1, 1)

    for setter, value in (
            ("kalman_set_state_matrix", state),
            ("kalman_set_state_transition_matrix", 1.0),
            ("kalman_set_control_matrix", 0.0),
            ("kalman_set_input_matrix", 0.0),
            ("kalman_set_covariance_matrix", covariance),
            ("kalman_set_process_noise_covariance_matrix", process_noise),
            ("kalman_set_measurement_covariance_matrix", measurement_noise),
            ("kalman_set_observation_matrix", 1.0)):
        matrix = ffitt.make_matrix(lib, [[value]])
        getattr(lib, setter)(REFERENCE(kalman), REFERENCE(matrix))
        lib.matrix_free(REFERENCE(matrix))

    return kalman


def element(lib, matrix, i=0, j=0):
    return lib.matrix_get_element(REFERENCE(matrix), i, j)


@given(state=level, covariance=positive, process=st.floats(min_value=0.0,
                                                           max_value=10.0, width=32),
       noise=positive, measurement=level)
def test_the_gain_of_a_scalar_filter_stays_between_zero_and_one(lib, state,
                                                                covariance, process,
                                                                noise, measurement):
    # For a scalar filter the gain is P/(P+R). Both values are never less than
    # zero, thus the gain must stay between zero and one.
    kalman = make_scalar_filter(lib, state, covariance, process, noise)
    y = ffitt.make_matrix(lib, [[measurement]])

    assert lib.kalman_step(REFERENCE(kalman), None, REFERENCE(y))
    gain = element(lib, kalman.k)

    assert 0.0 <= gain <= 1.0

    lib.matrix_free(REFERENCE(y))
    lib.kalman_free(REFERENCE(kalman))


@given(state=level, covariance=positive, noise=positive, measurement=level)
def test_the_new_state_stays_between_the_old_state_and_the_measurement(
        lib, state, covariance, noise, measurement):
    # The filter mixes the estimate and the measurement. With no process noise
    # the result cannot go outside the two values.
    kalman = make_scalar_filter(lib, state, covariance, 0.0, noise)
    y = ffitt.make_matrix(lib, [[measurement]])

    assert lib.kalman_step(REFERENCE(kalman), None, REFERENCE(y))
    result = element(lib, kalman.x)

    low = min(state, measurement)
    high = max(state, measurement)
    span = high - low

    assert result >= low - 1e-3 - (1e-3 * span)
    assert result <= high + 1e-3 + (1e-3 * span)

    lib.matrix_free(REFERENCE(y))
    lib.kalman_free(REFERENCE(kalman))


@given(state=level, covariance=positive, noise=positive, measurement=level,
       count=st.integers(min_value=1, max_value=8))
def test_the_covariance_never_grows_when_there_is_no_process_noise(
        lib, state, covariance, noise, measurement, count):
    kalman = make_scalar_filter(lib, state, covariance, 0.0, noise)
    y = ffitt.make_matrix(lib, [[measurement]])

    previous = element(lib, kalman.p)
    for _ in range(count):
        assert lib.kalman_step(REFERENCE(kalman), None, REFERENCE(y))
        current = element(lib, kalman.p)
        assert current <= previous + 1e-4 + (1e-4 * abs(previous))
        assert current >= -1e-6
        previous = current

    lib.matrix_free(REFERENCE(y))
    lib.kalman_free(REFERENCE(kalman))


@given(state=level, covariance=positive, process=positive, noise=positive,
       measurement=level)
def test_the_covariance_of_a_filter_with_two_states_stays_symmetric(
        lib, state, covariance, process, noise, measurement):
    # The covariance matrix of a Kalman filter is always symmetric.
    kalman = lib.kalman_alloc(1, 2, 1)

    settings = (
        ("kalman_set_state_matrix", [[state], [0.0]]),
        ("kalman_set_state_transition_matrix", [[1.0, 1.0], [0.0, 1.0]]),
        ("kalman_set_control_matrix", [[0.0], [0.0]]),
        ("kalman_set_input_matrix", [[0.0]]),
        ("kalman_set_covariance_matrix", [[covariance, 0.0], [0.0, covariance]]),
        ("kalman_set_process_noise_covariance_matrix",
         [[process, 0.0], [0.0, process]]),
        ("kalman_set_measurement_covariance_matrix", [[noise]]),
        ("kalman_set_observation_matrix", [[1.0, 0.0]]),
    )
    for setter, rows in settings:
        matrix = ffitt.make_matrix(lib, rows)
        getattr(lib, setter)(REFERENCE(kalman), REFERENCE(matrix))
        lib.matrix_free(REFERENCE(matrix))

    y = ffitt.make_matrix(lib, [[measurement]])

    for _ in range(3):
        assert lib.kalman_step(REFERENCE(kalman), None, REFERENCE(y))

        upper = element(lib, kalman.p, 0, 1)
        lower = element(lib, kalman.p, 1, 0)
        size = max(abs(upper), abs(lower))
        assert abs(upper - lower) <= 1e-3 + (1e-3 * size)

        # The values on the diagonal of a covariance matrix are never less
        # than zero.
        assert element(lib, kalman.p, 0, 0) >= -1e-3
        assert element(lib, kalman.p, 1, 1) >= -1e-3

    lib.matrix_free(REFERENCE(y))
    lib.kalman_free(REFERENCE(kalman))


@given(state=level, covariance=positive, measurement=level)
def test_a_measurement_with_no_noise_gives_the_measurement_as_the_state(
        lib, state, covariance, measurement):
    # With R = 0 the measurement has no doubt. Thus the gain is one, and the
    # state must take the value of the measurement.
    kalman = make_scalar_filter(lib, state, covariance, 0.0, 0.0)
    y = ffitt.make_matrix(lib, [[measurement]])

    assert lib.kalman_step(REFERENCE(kalman), None, REFERENCE(y))

    assert sp.close(element(lib, kalman.x), measurement,
                    relative=1e-3, absolute=1e-3)
    assert sp.close(element(lib, kalman.k), 1.0, relative=1e-3, absolute=1e-3)

    lib.matrix_free(REFERENCE(y))
    lib.kalman_free(REFERENCE(kalman))


@given(state=level, noise=positive, measurement=level)
def test_an_estimate_with_no_doubt_does_not_change(lib, state, noise, measurement):
    # With P = 0 and Q = 0 the estimate has no doubt. Thus the gain is zero,
    # and the measurement does not change the state.
    kalman = make_scalar_filter(lib, state, 0.0, 0.0, noise)
    y = ffitt.make_matrix(lib, [[measurement]])

    assert lib.kalman_step(REFERENCE(kalman), None, REFERENCE(y))

    assert sp.close(element(lib, kalman.x), state, relative=1e-3, absolute=1e-3)
    assert sp.close(element(lib, kalman.k), 0.0, relative=1e-3, absolute=1e-3)

    lib.matrix_free(REFERENCE(y))
    lib.kalman_free(REFERENCE(kalman))


@given(covariance=positive, noise=positive, measurement=level,
       count=st.integers(min_value=5, max_value=30))
def test_equal_measurements_move_the_state_to_that_value(lib, covariance, noise,
                                                         measurement, count):
    # The filter reads the same value again and again. With no process noise
    # the state must come nearer to that value at every step.
    kalman = make_scalar_filter(lib, 0.0, covariance, 0.0, noise)
    y = ffitt.make_matrix(lib, [[measurement]])

    previous_distance = abs(measurement - element(lib, kalman.x))
    for _ in range(count):
        assert lib.kalman_step(REFERENCE(kalman), None, REFERENCE(y))
        distance = abs(measurement - element(lib, kalman.x))
        assert distance <= previous_distance + 1e-3 + (1e-3 * abs(measurement))
        previous_distance = distance

    lib.matrix_free(REFERENCE(y))
    lib.kalman_free(REFERENCE(kalman))


@given(state=level, covariance=positive, process=positive, noise=positive,
       measurement=level)
def test_the_filter_never_gives_a_value_that_is_not_a_number(lib, state, covariance,
                                                             process, noise,
                                                             measurement):
    kalman = make_scalar_filter(lib, state, covariance, process, noise)
    y = ffitt.make_matrix(lib, [[measurement]])

    for _ in range(5):
        assert lib.kalman_step(REFERENCE(kalman), None, REFERENCE(y))
        for value in (element(lib, kalman.x), element(lib, kalman.p),
                      element(lib, kalman.k)):
            assert not math.isnan(value)
            assert not math.isinf(value)

    lib.matrix_free(REFERENCE(y))
    lib.kalman_free(REFERENCE(kalman))
