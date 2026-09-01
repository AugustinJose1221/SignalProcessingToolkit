"""Properties of the filter of Kalman that is extended to a curved model.

THE ONE TEST THAT MATTERS MOST, AND WHY.

The extended filter takes a model written as a pair of functions and works out
the slopes of those functions at the place where the state now stands. It then
does what the plain filter of Kalman does, using those slopes.

Thus, IF THE MODEL IS ALREADY STRAIGHT, the slopes it works out ARE the
matrices of the plain filter, and the two filters must give the same answer.
Not a similar answer: the same one. That single fact holds the whole module
together, because it ties a hundred lines of new arithmetic to a module that is
already tested, already used, and known to be right.

A fault anywhere in the extended filter, in the working out of the slopes, in
the order of the multiplying, or in which matrix is transposed, breaks it.

The rest of the file holds the filter to what a covariance IS. It must stay
symmetric, it must never fall below zero, and a measurement must never make it
larger. Those are not opinions about filtering; they are what the word means.
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

REFERENCE = ctypes.byref

# A model of a thing that moves at a steady speed. The state holds where it is
# and how fast it goes; only where it is can be measured.
#
# The step is a power of two, thus multiplying by it loses no digit and the two
# filters are compared on the arithmetic and not on the rounding.
STEP = 0.5
STATE_ROWS = [[1.0, STEP], [0.0, 1.0]]
MEASUREMENT_ROWS = [[1.0, 0.0]]

positive = st.floats(min_value=0.0625, max_value=16.0, width=32)
level = st.floats(min_value=-20.0, max_value=20.0, width=32)


def elements(lib, matrix):
    return ffitt.matrix_rows(lib, matrix)


def straight_model(lib):
    """Give the two functions of the model, which are straight lines.

    The pair is given back with them, because a pointer to a function that
    Python has let go of is a pointer into nothing.
    """
    @ffitt.STATE_FUNCTION
    def next_state(state, given_input, result):
        where = lib.matrix_get_element(state, 0, 0)
        speed = lib.matrix_get_element(state, 1, 0)
        lib.matrix_add_element(result, 0, 0,
                               sp.to_float32(where + STEP * speed))
        lib.matrix_add_element(result, 1, 0, speed)

    @ffitt.MEASUREMENT_FUNCTION
    def measurement(state, result):
        lib.matrix_add_element(result, 0, 0,
                               lib.matrix_get_element(state, 0, 0))

    return next_state, measurement


def set_matrix(lib, setter, handle, rows):
    matrix = ffitt.make_matrix(lib, rows)
    getattr(lib, setter)(REFERENCE(handle), REFERENCE(matrix))
    lib.matrix_free(REFERENCE(matrix))


def build_plain(lib, state, covariance, process, noise):
    kalman = lib.kalman_alloc(1, 2, 1)
    set_matrix(lib, "kalman_set_state_matrix", kalman, [[state], [0.0]])
    set_matrix(lib, "kalman_set_state_transition_matrix", kalman, STATE_ROWS)
    set_matrix(lib, "kalman_set_control_matrix", kalman, [[0.0], [0.0]])
    set_matrix(lib, "kalman_set_input_matrix", kalman, [[0.0]])
    set_matrix(lib, "kalman_set_covariance_matrix", kalman,
               [[covariance, 0.0], [0.0, covariance]])
    set_matrix(lib, "kalman_set_process_noise_covariance_matrix", kalman,
               [[process, 0.0], [0.0, process]])
    set_matrix(lib, "kalman_set_measurement_covariance_matrix", kalman,
               [[noise]])
    set_matrix(lib, "kalman_set_observation_matrix", kalman, MEASUREMENT_ROWS)
    return kalman


def build_extended(lib, state, covariance, process, noise, model):
    ekf = lib.ekf_alloc(1, 2, 1)
    lib.ekf_set_state_function(REFERENCE(ekf), model[0])
    lib.ekf_set_measurement_function(REFERENCE(ekf), model[1])
    set_matrix(lib, "ekf_set_state_matrix", ekf, [[state], [0.0]])
    set_matrix(lib, "ekf_set_covariance_matrix", ekf,
               [[covariance, 0.0], [0.0, covariance]])
    set_matrix(lib, "ekf_set_process_noise_covariance_matrix", ekf,
               [[process, 0.0], [0.0, process]])
    set_matrix(lib, "ekf_set_measurement_covariance_matrix", ekf, [[noise]])
    set_matrix(lib, "ekf_set_input_matrix", ekf, [[0.0]])
    return ekf


def near_rows(first, second, room):
    return all(abs(a - b) <= (room * (1.0 + abs(a) + abs(b)))
               for row_a, row_b in zip(first, second)
               for a, b in zip(row_a, row_b))


def agree(first, second, room):
    """True when two matrices agree, measured against the largest value in
    either of them.

    The error of a slope that was measured rather than read spreads across the
    whole matrix. It does not belong to one element, thus it must not be
    weighed against one element: an element that happens to sit near zero would
    otherwise be held to a standard that nothing else in the matrix is held to.
    """
    scale = 1.0 + max(max(abs(value) for value in row)
                      for row in (first + second))
    return all(abs(a - b) <= (room * scale)
               for row_a, row_b in zip(first, second)
               for a, b in zip(row_a, row_b))


@given(state=level, covariance=positive,
       process=st.floats(min_value=0.0, max_value=4.0, width=32),
       noise=positive,
       readings=st.lists(level, min_size=1, max_size=8))
@settings(max_examples=100)
def test_a_straight_model_gives_what_the_plain_filter_of_kalman_gives(
        lib, state, covariance, process, noise, readings):
    """The test the whole module rests on.

    The same straight model is given to both filters, one as matrices and one
    as functions, and the same measurements are put through both. Their states
    and their covariances must agree at every step.
    """
    model = straight_model(lib)
    plain = build_plain(lib, state, covariance, process, noise)
    extended = build_extended(lib, state, covariance, process, noise, model)
    try:
        for reading in readings:
            y = ffitt.make_matrix(lib, [[sp.to_float32(reading)]])
            first = lib.kalman_step(REFERENCE(plain), None, REFERENCE(y))
            second = lib.ekf_step(REFERENCE(extended), None, REFERENCE(y))
            lib.matrix_free(REFERENCE(y))

            assert first == second
            assume(first)

            # THE ROOM ALLOWED, AND WHERE THE NUMBER COMES FROM.
            #
            # The extended filter does not read the slopes from a matrix. It
            # measures them, by moving the state a little and seeing how far
            # the answer moves. For a straight model that measurement is exact
            # in exact arithmetic, but subtracting two nearly equal numbers
            # throws digits away, thus the two filters agree to the digits that
            # are left and not to the last one.
            #
            # THE FIRST BOUND HERE WAS WRONG, AND THE WAY IT WAS MEASURED IS
            # WHY. It was set at 5e-3 from a sweep of 2400 steps whose numbers
            # were all drawn from an even spread, and that sweep found 1.7e-3.
            # Running the whole suite 25 times over then broke it.
            #
            # A search for a falsifying case does not draw from an even spread.
            # It reaches for the END of every range, and for several ends AT
            # ONCE. The worst case is exactly such a corner: a filter that is
            # very unsure, a measurement it trusts almost completely, and a
            # state that must swing the whole width of the range to reach it.
            # Four numbers at their limits together, which an even spread
            # essentially never draws.
            #
            # Measured again over those corners:
            #
            #     32 bits   6.4e-3   at state -20, covariance 16,
            #                        no process noise, measurement noise
            #                        0.0625, reading +20
            #     64 bits   3.5e-9
            #
            # The bound now stands at three times the worst corner. A real
            # fault in the arithmetic would be orders of magnitude and not a
            # factor of three, thus the test still catches one.
            assert agree(elements(lib, plain.x), elements(lib, extended.x),
                         2e-2)
            assert agree(elements(lib, plain.p), elements(lib, extended.p),
                         2e-2)
    finally:
        lib.kalman_free(REFERENCE(plain))
        lib.ekf_free(REFERENCE(extended))


@given(state=level, covariance=positive,
       step=st.sampled_from([0.0625, 0.25, 1.0, 4.0]))
def test_the_slope_of_a_straight_model_is_the_model_itself(lib, state,
                                                           covariance, step):
    """A straight line has the same slope everywhere, thus the size of the
    little move must not matter at all. The filter must find the model back.
    """
    model = straight_model(lib)
    ekf = build_extended(lib, state, covariance, 0.0, 1.0, model)
    try:
        lib.ekf_set_derivative_step(REFERENCE(ekf), sp.to_float32(step))

        room = lib.matrix_alloc(2, 2)
        lib.ekf_state_jacobian_into(REFERENCE(ekf), REFERENCE(room))
        assert near_rows(elements(lib, room), STATE_ROWS, 1e-3)
        lib.matrix_free(REFERENCE(room))

        room = lib.matrix_alloc(1, 2)
        lib.ekf_measurement_jacobian_into(REFERENCE(ekf), REFERENCE(room))
        assert near_rows(elements(lib, room), MEASUREMENT_ROWS, 1e-3)
        lib.matrix_free(REFERENCE(room))
    finally:
        lib.ekf_free(REFERENCE(ekf))


@given(state=level, covariance=positive,
       process=st.floats(min_value=0.0, max_value=4.0, width=32),
       noise=positive,
       readings=st.lists(level, min_size=1, max_size=10))
def test_the_covariance_stays_the_same_read_either_way_round(lib, state,
                                                             covariance,
                                                             process, noise,
                                                             readings):
    """A covariance is symmetric because of what it means, not by convention.

    The value at row i and column j is how the doubt about element i and the
    doubt about element j go together, and that question has the same answer
    asked either way round. An implementation that lets the two corners drift
    apart has an arithmetic fault, and the drift grows step by step until the
    filter cannot be used at all.
    """
    model = straight_model(lib)
    ekf = build_extended(lib, state, covariance, process, noise, model)
    try:
        for reading in readings:
            y = ffitt.make_matrix(lib, [[sp.to_float32(reading)]])
            ran = lib.ekf_step(REFERENCE(ekf), None, REFERENCE(y))
            lib.matrix_free(REFERENCE(y))
            assume(ran)

            values = elements(lib, ekf.p)
            size = abs(values[0][0]) + abs(values[1][1]) + 1.0
            assert abs(values[0][1] - values[1][0]) <= (1e-3 * size)
    finally:
        lib.ekf_free(REFERENCE(ekf))


@given(state=level, covariance=positive,
       process=st.floats(min_value=0.0, max_value=4.0, width=32),
       noise=positive,
       readings=st.lists(level, min_size=1, max_size=10))
def test_the_doubt_never_falls_below_nothing(lib, state, covariance, process,
                                             noise, readings):
    """A covariance holds squares along its diagonal. A square is never below
    zero, and neither is the area the two elements cover between them.
    """
    model = straight_model(lib)
    ekf = build_extended(lib, state, covariance, process, noise, model)
    try:
        for reading in readings:
            y = ffitt.make_matrix(lib, [[sp.to_float32(reading)]])
            ran = lib.ekf_step(REFERENCE(ekf), None, REFERENCE(y))
            lib.matrix_free(REFERENCE(y))
            assume(ran)

            values = elements(lib, ekf.p)
            assert values[0][0] >= -1e-4
            assert values[1][1] >= -1e-4

            area = (values[0][0] * values[1][1]) - (values[0][1] * values[1][0])
            size = 1.0 + abs(values[0][0] * values[1][1])
            assert area >= (-1e-3 * size)
    finally:
        lib.ekf_free(REFERENCE(ekf))


@given(state=level, covariance=positive, noise=positive, reading=level)
def test_a_measurement_never_leaves_a_filter_less_sure_than_before(
        lib, state, covariance, noise, reading):
    """Reading something can only reduce the doubt, never add to it.

    With no process noise, the doubt after a measurement must be no larger than
    the doubt before it, on both elements of the state. A filter whose gain
    were worked out with a sign the wrong way round would grow instead, and
    would still look like a filter for a few steps.
    """
    model = straight_model(lib)
    ekf = build_extended(lib, state, covariance, 0.0, noise, model)
    try:
        lib.ekf_predict(REFERENCE(ekf))
        before = elements(lib, ekf.p)

        set_matrix(lib, "ekf_set_measurement_matrix", ekf,
                   [[sp.to_float32(reading)]])
        assume(lib.ekf_update(REFERENCE(ekf)))

        after = elements(lib, ekf.p)
    finally:
        lib.ekf_free(REFERENCE(ekf))

    for index in range(2):
        assert after[index][index] <= (before[index][index]
                                       + 1e-3 * (1.0 + before[index][index]))


@given(state=level, covariance=positive, reading=level)
def test_a_measurement_that_cannot_be_trusted_moves_nothing(lib, state,
                                                            covariance,
                                                            reading):
    """The measurement noise is what says how much a reading is worth.

    Made very large, the reading is worth nothing and the filter must keep what
    it had. This is the test that finds a gain worked out the wrong way up:
    such a filter would follow the worthless reading exactly.
    """
    model = straight_model(lib)
    ekf = build_extended(lib, state, covariance, 0.0, 1.0e6, model)
    try:
        lib.ekf_predict(REFERENCE(ekf))
        before = elements(lib, ekf.x)

        set_matrix(lib, "ekf_set_measurement_matrix", ekf,
                   [[sp.to_float32(reading)]])
        assume(lib.ekf_update(REFERENCE(ekf)))

        after = elements(lib, ekf.x)
    finally:
        lib.ekf_free(REFERENCE(ekf))

    for index in range(2):
        assert abs(after[index][0] - before[index][0]) <= 1e-2


@given(state=level, covariance=positive, reading=level)
def test_a_measurement_that_can_be_trusted_is_followed(lib, state, covariance,
                                                        reading):
    """The other end of the same rule. A reading with almost no noise, given to
    a filter that is very unsure, must be believed nearly whole.
    """
    model = straight_model(lib)
    ekf = build_extended(lib, state, 1.0e4, 0.0, 1.0e-4, model)
    set_matrix(lib, "ekf_set_state_matrix", ekf, [[sp.to_float32(state)],
                                                  [0.0]])
    try:
        lib.ekf_predict(REFERENCE(ekf))
        set_matrix(lib, "ekf_set_measurement_matrix", ekf,
                   [[sp.to_float32(reading)]])
        assume(lib.ekf_update(REFERENCE(ekf)))

        after = elements(lib, ekf.x)
    finally:
        lib.ekf_free(REFERENCE(ekf))

    # The room allowed grows with how far the filter had to move, because that
    # is the size of the subtraction the arithmetic has to make. A filter that
    # starts at 19 and is pulled to 0 must throw away the digits of a 19.
    room = 1e-3 * (1.0 + abs(state) + abs(reading))
    assert abs(after[0][0] - sp.to_float32(reading)) <= room
