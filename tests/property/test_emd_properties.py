"""Properties of taking a signal apart into its modes.

The decomposition makes ONE promise that can be checked without knowing
anything about the signal, and the header states it plainly:

    the sum of all the functions and the residue gives the signal again.

That is the property this file is built on. It is worth more than any other
test of the module, because the method is a loop of splines and means with no
closed form to compare against: there is no second way to work out the right
answer. But whatever the loop does, whatever it takes out, everything it takes
out must still be there when the parts are added up. A step that loses a part
of the signal, or counts one twice, breaks it at once.

The rest of the file holds the module to what an intrinsic mode function IS,
and to the order the method takes them out in: the fastest first, and each one
slower than the one before.
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

MOST = 6
THRESHOLD = 10


class Decomposition:
    """Everything the decomposition needs, held together so that no list it
    was given is let go of while it is still using it.
    """

    def __init__(self, lib, values):
        self.lib = lib
        self.size = len(values)
        self.values = [sp.to_float32(value) for value in values]

        self.emd = lib.emd_alloc(self.size)
        self.x = ffitt.float_array([sp.to_float32(float(index))
                                   for index in range(self.size)])
        self.y = ffitt.float_array(self.values)
        self.residue = ffitt.real_buffer(self.size)
        self.working = ffitt.real_buffer(self.size)
        self.peak_index = ffitt.real_buffer(self.size)
        self.valley_index = ffitt.real_buffer(self.size)

        self.imf = (ffitt.Imf * MOST)()
        self.room = [ffitt.real_buffer(self.size) for _ in range(2 * MOST)]
        for index in range(MOST):
            self.imf[index].x = ctypes.cast(self.room[2 * index],
                                            ctypes.POINTER(ffitt.REAL_T))
            self.imf[index].y = ctypes.cast(self.room[2 * index + 1],
                                            ctypes.POINTER(ffitt.REAL_T))
            self.imf[index].size = self.size
            self.imf[index].dynamic_alloc = False

        lib.emd_initialize(REFERENCE(self.emd), MOST, self.imf, self.x,
                           self.y, self.residue, self.working,
                           self.peak_index, self.valley_index)

    def sift(self, threshold=THRESHOLD):
        return self.lib.emd_sift(REFERENCE(self.emd), threshold)

    def mode(self, index):
        return [self.imf[index].y[place] for place in range(self.size)]

    def rest(self):
        return list(self.residue)

    def close(self):
        self.lib.emd_free(self.emd)


def wandering(size, parts):
    """A signal built of a few waves of different speeds, which is the kind of
    signal the method is for.
    """
    values = []
    for index in range(size):
        total = 0.0
        for turns, height in parts:
            total += height * math.sin(2.0 * math.pi * turns * index / size)
        values.append(sp.to_float32(total))
    return values


def turning_points(values):
    """How many times the signal changes direction."""
    count = 0
    for index in range(1, len(values) - 1):
        before = values[index] - values[index - 1]
        after = values[index + 1] - values[index]
        if (before > 0.0 and after < 0.0) or (before < 0.0 and after > 0.0):
            count += 1
    return count


signals = st.lists(sp.elements(magnitude=10.0), min_size=8, max_size=64)


@given(values=signals)
@settings(max_examples=60, deadline=None)
def test_the_parts_added_together_give_the_signal_again(lib, values):
    """The promise of the module, and the only one that needs no other
    knowledge to check.

    Whatever the method takes out, and however many times it repeats its step,
    nothing may be lost and nothing may be counted twice.
    """
    work = Decomposition(lib, values)
    try:
        found = work.sift()
        assume(found > 0)

        total = list(work.rest())
        for index in range(found):
            part = work.mode(index)
            for place in range(work.size):
                total[place] += part[place]
    finally:
        work.close()

    size = 1.0 + max(abs(value) for value in work.values)
    for place in range(work.size):
        assert abs(total[place] - work.values[place]) <= (1e-2 * size)


@given(parts=st.lists(st.tuples(st.integers(min_value=1, max_value=20),
                                st.floats(min_value=0.25, max_value=4.0,
                                          width=32)),
                      min_size=1, max_size=3),
       size=st.sampled_from([64, 96, 128]))
@settings(max_examples=40, deadline=None)
def test_the_parts_of_a_signal_built_of_waves_add_back_to_it(lib, parts,
                                                              size):
    """The same promise on the signal the method was made for.

    A list of random values has no modes to find; a sum of waves has. The
    method has real work to do here, thus this is where a step that loses part
    of the signal shows itself.
    """
    values = wandering(size, parts)
    work = Decomposition(lib, values)
    try:
        found = work.sift()
        assume(found > 0)

        total = list(work.rest())
        for index in range(found):
            part = work.mode(index)
            for place in range(work.size):
                total[place] += part[place]
    finally:
        work.close()

    scale = 1.0 + max(abs(value) for value in work.values)
    for place in range(work.size):
        assert abs(total[place] - work.values[place]) <= (1e-2 * scale)


@given(parts=st.lists(st.tuples(st.integers(min_value=1, max_value=20),
                                st.floats(min_value=0.25, max_value=4.0,
                                          width=32)),
                      min_size=2, max_size=3),
       size=st.sampled_from([96, 128]))
@settings(max_examples=40, deadline=None)
def test_the_fastest_mode_comes_out_first(lib, parts, size):
    """The order the method works in, held as a rule.

    Each step takes out what is left that changes fastest. Thus a mode taken
    out later must turn direction NO MORE OFTEN than the one before it. A
    method that took them out in any other order would still add back to the
    signal, thus the test above would not find it.
    """
    values = wandering(size, parts)
    work = Decomposition(lib, values)
    try:
        found = work.sift()
        assume(found >= 2)

        # A MODE THAT HOLDS ALMOST NOTHING HOLDS NO FREQUENCY EITHER.
        #
        # The last thing the method takes out is often a millionth of the
        # signal, which is the rounding of the splines and not a mode. Counting
        # how often such a thing turns direction counts the rounding, and the
        # rounding turns at every sample. Only the modes that carry a real part
        # of the signal are ordered here.
        largest = max(abs(value) for value in work.values)
        counts = [turning_points(work.mode(index)) for index in range(found)
                  if max(abs(value)
                         for value in work.mode(index)) > (1e-3 * largest)]
    finally:
        work.close()

    assume(len(counts) >= 2)
    for index in range(1, len(counts)):
        assert counts[index] <= counts[index - 1]


@given(parts=st.lists(st.tuples(st.integers(min_value=2, max_value=16),
                                st.floats(min_value=0.5, max_value=4.0,
                                          width=32)),
                      min_size=1, max_size=3),
       size=st.sampled_from([64, 96, 128]))
@settings(max_examples=40, deadline=None)
def test_what_is_left_over_has_nothing_left_in_it(lib, parts, size):
    """The residue is what is left when there is no more mode to take out.

    That is a claim about the residue itself, and it is tested here by asking
    the method the same question twice. The signal is taken apart, and then
    what is left over is GIVEN BACK to a fresh decomposition. It must find
    nothing at all.

    This is the honest form of the rule. Counting how often the residue turns
    direction was tried first and it does not work: where a signal is taken
    apart completely, what is left is the rounding of the splines and rounding
    turns at nearly every sample. The count then measures the rounding. Asking
    the method whether it is finished measures what was meant.

    A decomposition that stopped because it had filled every place the caller
    offered is a different case, and it says nothing about the residue. It is
    left out.
    """
    values = wandering(size, parts)

    work = Decomposition(lib, values)
    try:
        found = work.sift()
        residue = work.rest()
    finally:
        work.close()

    # Only a decomposition that stopped of its own accord makes the claim.
    assume(found < MOST)

    again = Decomposition(lib, residue)
    try:
        assert again.sift() == 0
    finally:
        again.close()


@given(parts=st.lists(st.tuples(st.integers(min_value=1, max_value=16),
                                st.floats(min_value=0.5, max_value=4.0,
                                          width=32)),
                      min_size=1, max_size=3),
       size=st.sampled_from([64, 96]),
       power=st.integers(min_value=-4, max_value=4))
@settings(max_examples=40, deadline=None)
def test_making_the_signal_larger_makes_every_part_larger_by_as_much(
        lib, parts, size, power):
    """The method is built of splines and means, and both are linear.

    Thus the same signal twice the size must give the same modes twice the
    size, and the same number of them. A step that compared anything against a
    fixed amount would break this, and the module would then behave differently
    on the same measurement read in different units.
    """
    factor = float(2 ** power)
    values = wandering(size, parts)
    scaled = [sp.to_float32(value * factor) for value in values]

    plain = Decomposition(lib, values)
    large = Decomposition(lib, scaled)
    try:
        first = plain.sift()
        second = large.sift()
        assume(first > 0)
        assert first == second

        modes = [plain.mode(index) for index in range(first)]
        grown = [large.mode(index) for index in range(second)]
        rest = plain.rest()
        grown_rest = large.rest()
    finally:
        plain.close()
        large.close()

    scale = (1.0 + max(abs(value) for value in values)) * factor
    for index in range(first):
        for place in range(size):
            assert abs(grown[index][place]
                       - modes[index][place] * factor) <= (1e-3 * scale)
    for place in range(size):
        assert abs(grown_rest[place]
                   - rest[place] * factor) <= (1e-3 * scale)


@given(values=st.lists(sp.elements(magnitude=10.0), min_size=1,
                       max_size=2))
def test_a_signal_too_short_to_hold_a_peak_gives_nothing(lib, values):
    """Fewer than three samples hold no peak and no valley, thus there is
    nothing to take out and the module must say so rather than guess.
    """
    work = Decomposition(lib, values)
    try:
        status = ctypes.c_uint32(1)
        lib.emd_get_imf(REFERENCE(work.emd), 0, THRESHOLD, REFERENCE(status))
        assert status.value == 0
    finally:
        work.close()


@given(values=signals, most=st.integers(min_value=1, max_value=MOST))
@settings(max_examples=40, deadline=None)
def test_the_method_never_gives_more_functions_than_it_was_allowed(lib,
                                                                   values,
                                                                   most):
    """The caller says how many functions there is room for. Writing past that
    is writing into memory that belongs to somebody else.
    """
    work = Decomposition(lib, values)
    try:
        lib.emd_initialize(REFERENCE(work.emd), most, work.imf, work.x,
                           work.y, work.residue, work.working,
                           work.peak_index, work.valley_index)
        found = work.sift()
        assert found <= most
    finally:
        work.close()


@given(values=signals)
@settings(max_examples=40, deadline=None)
def test_every_function_holds_as_many_samples_as_the_signal(lib, values):
    work = Decomposition(lib, values)
    try:
        found = work.sift()
        for index in range(found):
            assert work.imf[index].size == work.size
            for place in range(work.size):
                assert work.imf[index].x[place] == sp.to_float32(float(place))
    finally:
        work.close()
