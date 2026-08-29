"""Properties of the filter of Kalman that carries points instead of slopes.

THE TWO TESTS THAT CARRY THE FILE.

THE FIRST is the same one that holds the extended filter together. This filter
takes a model as a pair of functions, carries a set of chosen points through
them, and reads the middle and the spread of where those points arrive. If the
model is STRAIGHT, that must give exactly what the plain filter of Kalman gives
from its matrices. The unscented transform is EXACT for a straight function,
which is not an approximation and not nearly: it is the reason the method was
invented. Thus the two filters must agree.

THE SECOND is the transform itself, tested apart from any filtering. The set of
points the filter places must have the state as its weighted middle and the
covariance as its weighted spread. That is what the points ARE FOR and what
they mean. A set placed with the wrong factor, or spread along the wrong
directions, still looks like a reasonable set of points and carries the wrong
answer through every step that follows.
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

REFERENCE = ctypes.byref

STEP = 0.5
STATE_ROWS = [[1.0, STEP], [0.0, 1.0]]
MEASUREMENT_ROWS = [[1.0, 0.0]]

positive = st.floats(min_value=0.0625, max_value=16.0, width=32)
level = st.floats(min_value=-20.0, max_value=20.0, width=32)


def elements(lib, matrix):
    return sptk.matrix_rows(lib, matrix)


def straight_model(lib):
    @sptk.STATE_FUNCTION
    def next_state(state, given_input, result):
        where = lib.matrix_get_element(state, 0, 0)
        speed = lib.matrix_get_element(state, 1, 0)
        lib.matrix_add_element(result, 0, 0,
                               sp.to_float32(where + STEP * speed))
        lib.matrix_add_element(result, 1, 0, speed)

    @sptk.MEASUREMENT_FUNCTION
    def measurement(state, result):
        lib.matrix_add_element(result, 0, 0,
                               lib.matrix_get_element(state, 0, 0))

    return next_state, measurement


def set_matrix(lib, setter, handle, rows):
    matrix = sptk.make_matrix(lib, rows)
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


def build_points(lib, state, covariance, process, noise, model):
    ukf = lib.ukf_alloc(1, 2, 1)
    lib.ukf_set_state_function(REFERENCE(ukf), model[0])
    lib.ukf_set_measurement_function(REFERENCE(ukf), model[1])
    set_matrix(lib, "ukf_set_state_matrix", ukf, [[state], [0.0]])
    set_matrix(lib, "ukf_set_covariance_matrix", ukf,
               [[covariance, 0.0], [0.0, covariance]])
    set_matrix(lib, "ukf_set_process_noise_covariance_matrix", ukf,
               [[process, 0.0], [0.0, process]])
    set_matrix(lib, "ukf_set_measurement_covariance_matrix", ukf, [[noise]])
    set_matrix(lib, "ukf_set_input_matrix", ukf, [[0.0]])
    return ukf


def agree(first, second, room):
    """True when two matrices agree, measured against the largest value in
    either. The error of a transform belongs to the whole matrix and not to one
    element of it.
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
    """The transform is EXACT for a straight function, thus so is the filter.

    The same model is given to both, one as matrices and one as functions, and
    the same measurements go through both. What comes out must be the same.
    """
    model = straight_model(lib)
    plain = build_plain(lib, state, covariance, process, noise)
    points = build_points(lib, state, covariance, process, noise, model)
    try:
        for reading in readings:
            y = sptk.make_matrix(lib, [[sp.to_float32(reading)]])
            first = lib.kalman_step(REFERENCE(plain), None, REFERENCE(y))
            second = lib.ukf_step(REFERENCE(points), None, REFERENCE(y))
            lib.matrix_free(REFERENCE(y))
            assume(first and second)

            # The room allowed is for the arithmetic only. The points are
            # placed with a root of the covariance, carried through, and read
            # back, thus the answer passes through more steps than the plain
            # filter and keeps fewer digits. It is the same answer.
            assert agree(elements(lib, plain.x), elements(lib, points.x), 5e-3)
            assert agree(elements(lib, plain.p), elements(lib, points.p), 5e-2)
    finally:
        lib.kalman_free(REFERENCE(plain))
        lib.ukf_free(REFERENCE(points))


@given(state=level, speed=level, first=positive, second=positive,
       together=st.floats(min_value=-1.0, max_value=1.0, width=32))
def test_the_points_have_the_state_as_their_middle(lib, state, speed, first,
                                                   second, together):
    """The first half of what the points mean.

    Add up the points, each with its own weight, and the state must come back.
    A set placed even slightly off centre carries a bias into every step that
    follows, and nothing later can find it.
    """
    # A covariance that is a real spread: the corner term cannot be larger than
    # the two on the diagonal allow.
    corner = together * math.sqrt(first * second) * 0.9

    model = straight_model(lib)
    ukf = build_points(lib, 0.0, 1.0, 0.0, 1.0, model)
    try:
        set_matrix(lib, "ukf_set_state_matrix", ukf,
                   [[sp.to_float32(state)], [sp.to_float32(speed)]])
        set_matrix(lib, "ukf_set_covariance_matrix", ukf,
                   [[sp.to_float32(first), sp.to_float32(corner)],
                    [sp.to_float32(corner), sp.to_float32(second)]])

        room = lib.matrix_alloc(2, 5)
        assume(lib.ukf_place_points_into(REFERENCE(ukf), REFERENCE(room)))

        placed = elements(lib, room)
        weights = [element[0] for element in elements(lib,
                                                      ukf.scratch.weight_mean)]
        lib.matrix_free(REFERENCE(room))

        middle = [sum(weights[k] * placed[row][k] for k in range(5))
                  for row in range(2)]
    finally:
        lib.ukf_free(REFERENCE(ukf))

    wanted = [sp.to_float32(state), sp.to_float32(speed)]
    scale = 1.0 + abs(wanted[0]) + abs(wanted[1])
    for row in range(2):
        assert abs(middle[row] - wanted[row]) <= (1e-3 * scale)


@given(state=level, speed=level, first=positive, second=positive,
       together=st.floats(min_value=-1.0, max_value=1.0, width=32))
def test_the_points_have_the_covariance_as_their_spread(lib, state, speed,
                                                        first, second,
                                                        together):
    """The second half, and the harder one.

    Take how far each point stands from the state, multiply that by itself both
    ways round, add the results with the weights for a spread, and the
    covariance must come back. This is the whole claim of the transform, and it
    is what makes the filter work without any slope being worked out at all.
    """
    corner = together * math.sqrt(first * second) * 0.9

    model = straight_model(lib)
    ukf = build_points(lib, 0.0, 1.0, 0.0, 1.0, model)
    try:
        set_matrix(lib, "ukf_set_state_matrix", ukf,
                   [[sp.to_float32(state)], [sp.to_float32(speed)]])
        wanted = [[sp.to_float32(first), sp.to_float32(corner)],
                  [sp.to_float32(corner), sp.to_float32(second)]]
        set_matrix(lib, "ukf_set_covariance_matrix", ukf, wanted)

        room = lib.matrix_alloc(2, 5)
        assume(lib.ukf_place_points_into(REFERENCE(ukf), REFERENCE(room)))

        placed = elements(lib, room)
        weights = [element[0]
                   for element in elements(lib, ukf.scratch.weight_spread)]
        lib.matrix_free(REFERENCE(room))

        centre = [sp.to_float32(state), sp.to_float32(speed)]
        spread = [[0.0, 0.0], [0.0, 0.0]]
        for k in range(5):
            away = [placed[row][k] - centre[row] for row in range(2)]
            for i in range(2):
                for j in range(2):
                    spread[i][j] += weights[k] * away[i] * away[j]
    finally:
        lib.ukf_free(REFERENCE(ukf))

    assert agree(spread, wanted, 1e-2)


@given(state=level, covariance=positive)
def test_the_points_stand_one_at_the_middle_and_the_rest_in_pairs(lib, state,
                                                                  covariance):
    """A state of two elements gives five points: one at the middle and two
    pairs, each pair standing the same distance to either side of it. A pair
    that is not even is a root taken wrongly.
    """
    model = straight_model(lib)
    ukf = build_points(lib, state, covariance, 0.0, 1.0, model)
    try:
        room = lib.matrix_alloc(2, 5)
        assume(lib.ukf_place_points_into(REFERENCE(ukf), REFERENCE(room)))
        placed = elements(lib, room)
        lib.matrix_free(REFERENCE(room))
    finally:
        lib.ukf_free(REFERENCE(ukf))

    centre = [sp.to_float32(state), 0.0]
    scale = 1.0 + abs(centre[0]) + math.sqrt(covariance)

    for row in range(2):
        assert abs(placed[row][0] - centre[row]) <= (1e-4 * scale)

    for pair in range(2):
        for row in range(2):
            above = placed[row][1 + pair] - centre[row]
            below = placed[row][3 + pair] - centre[row]
            assert abs(above + below) <= (1e-3 * scale)


@given(state=level, covariance=positive,
       process=st.floats(min_value=0.0, max_value=4.0, width=32),
       noise=positive,
       readings=st.lists(level, min_size=1, max_size=10))
def test_the_covariance_stays_the_same_read_either_way_round(lib, state,
                                                             covariance,
                                                             process, noise,
                                                             readings):
    """A covariance is symmetric because of what it means. A filter that reads
    its spread from a set of points has more ways to lose that than one that
    reads it from a matrix, thus it is worth asking.
    """
    model = straight_model(lib)
    ukf = build_points(lib, state, covariance, process, noise, model)
    try:
        for reading in readings:
            y = sptk.make_matrix(lib, [[sp.to_float32(reading)]])
            ran = lib.ukf_step(REFERENCE(ukf), None, REFERENCE(y))
            lib.matrix_free(REFERENCE(y))
            assume(ran)

            values = elements(lib, ukf.p)
            size = abs(values[0][0]) + abs(values[1][1]) + 1.0
            assert abs(values[0][1] - values[1][0]) <= (1e-3 * size)
            assert values[0][0] >= -1e-4
            assert values[1][1] >= -1e-4
    finally:
        lib.ukf_free(REFERENCE(ukf))


@given(state=level, covariance=positive, reading=level)
def test_a_measurement_that_cannot_be_trusted_moves_nothing(lib, state,
                                                            covariance,
                                                            reading):
    model = straight_model(lib)
    ukf = build_points(lib, state, covariance, 0.0, 1.0e6, model)
    try:
        assume(lib.ukf_predict(REFERENCE(ukf)))
        before = elements(lib, ukf.x)

        set_matrix(lib, "ukf_set_measurement_matrix", ukf,
                   [[sp.to_float32(reading)]])
        assume(lib.ukf_update(REFERENCE(ukf)))
        after = elements(lib, ukf.x)
    finally:
        lib.ukf_free(REFERENCE(ukf))

    for index in range(2):
        assert abs(after[index][0] - before[index][0]) <= 1e-2


@given(state=level, reading=level)
def test_a_measurement_that_can_be_trusted_is_followed(lib, state, reading):
    model = straight_model(lib)
    ukf = build_points(lib, state, 1.0e4, 0.0, 1.0e-4, model)
    try:
        assume(lib.ukf_predict(REFERENCE(ukf)))
        set_matrix(lib, "ukf_set_measurement_matrix", ukf,
                   [[sp.to_float32(reading)]])
        assume(lib.ukf_update(REFERENCE(ukf)))
        after = elements(lib, ukf.x)
    finally:
        lib.ukf_free(REFERENCE(ukf))

    room = 1e-3 * (1.0 + abs(state) + abs(reading))
    assert abs(after[0][0] - sp.to_float32(reading)) <= room


@given(nx=st.integers(min_value=1, max_value=6),
       alpha=st.floats(min_value=-2.0, max_value=2.0, width=32),
       kappa=st.floats(min_value=-8.0, max_value=8.0, width=32))
def test_a_spreading_that_the_width_cannot_carry_is_refused(lib, nx, alpha,
                                                             kappa):
    """The spreading divides the weights, thus a spreading near zero makes them
    enormous and their sum loses all its meaning. The module must say so rather
    than answer with numbers that look like an answer.
    """
    alpha = sp.to_float32(alpha)
    kappa = sp.to_float32(kappa)
    allowed = lib.ukf_is_valid_spread(nx, alpha, kappa)

    ukf = lib.ukf_alloc(1, nx, 1)
    try:
        was = (ukf.alpha, ukf.beta, ukf.kappa)
        taken = lib.ukf_set_spread(REFERENCE(ukf), alpha, sp.to_float32(2.0),
                                   kappa)
        assert taken == allowed

        if taken:
            assert (ukf.alpha, ukf.kappa) == (alpha, kappa)
        else:
            # A refusal must change nothing at all. A filter left holding half
            # of a spreading it would not accept is worse than one that
            # refused, because it goes on running.
            assert (ukf.alpha, ukf.beta, ukf.kappa) == was
    finally:
        lib.ukf_free(REFERENCE(ukf))
