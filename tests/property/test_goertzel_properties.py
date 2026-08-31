"""Rules that reading one frequency must keep.

Goertzel is not an approximation of anything. It is the DISCRETE FOURIER
TRANSFORM AT ONE BIN, worked out by a recurrence instead of by a sum, and it
must give what the transform gives. That is the property this file is built
around, and every other rule here follows from it.

Nothing below tests the shape of the interface. What is held is what the
arithmetic is: a bin of the transform, its size, its phase, and how a signal
that is not at a bin behaves.
"""

import cmath
import math
import os
import sys

from hypothesis import assume, given, settings
from hypothesis import strategies as st

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402
import strategies as sp  # noqa: E402

RUNS = settings(max_examples=40, deadline=None)

TWO_PI = 2.0 * math.pi

# Block sizes the transform can also take, so that the two can be set beside
# each other.
BLOCKS = st.sampled_from([16, 32, 64, 128])

RATE = 1024.0


def read(lib, block, bin_index, values, rate=RATE):
    """Run a block through a detector tuned to the given bin."""
    frequency = (bin_index * rate) / block

    detector = lib.goertzel_init(sp.to_float32(frequency),
                                 sp.to_float32(rate), block)

    lib.goertzel_process_block(detector, sptk.float_array(values), len(values))

    return detector


def dft_bin(values, bin_index):
    """The one bin of the discrete transform, summed here rather than
    transformed, so that the two answers arrive by different roads."""
    total = complex(0.0, 0.0)
    count = len(values)

    for index, value in enumerate(values):
        angle = -TWO_PI * bin_index * index / count
        total += value * complex(math.cos(angle), math.sin(angle))

    return total


def noise(count, seed=1):
    state = seed
    out = []

    for _ in range(count):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        out.append(sp.to_float32((((state >> 16) % 20000) / 10000.0) - 1.0))

    return out


@given(BLOCKS, st.integers(min_value=1, max_value=40), st.integers(1, 64))
@RUNS
def test_it_gives_the_size_of_the_transform_at_that_bin(lib, block, bin_index,
                                                        seed):
    """THE RULE THE WHOLE MODULE IS. Goertzel is the transform at one bin
    worked out by a recurrence rather than by a sum. If the two ever parted, one
    of them would not be the transform.

    The sum here is written out in Python, thus the two answers arrive by
    different roads and a fault in one cannot hide in the other."""
    assume(bin_index < block // 2)

    values = noise(block, seed)

    detector = read(lib, block, bin_index, values)
    wanted = abs(dft_bin(values, bin_index))

    scale = 1.0 + sum(abs(value) for value in values)

    assert abs(lib.goertzel_magnitude(detector) - wanted) <= 1e-3 * scale


@given(BLOCKS, st.integers(min_value=1, max_value=40), st.integers(1, 64))
@RUNS
def test_the_size_squared_is_the_size_multiplied_by_itself(lib, block,
                                                           bin_index, seed):
    """The squared form exists so that a caller comparing two answers need not
    take a root at all. It must be the root's own square, or the two say
    different things about the same block."""
    assume(bin_index < block // 2)

    detector = read(lib, block, bin_index, noise(block, seed))

    size = lib.goertzel_magnitude(detector)
    squared = lib.goertzel_magnitude_squared(detector)

    assert abs(squared - (size * size)) <= 1e-3 * (1.0 + squared)
    assert squared >= 0.0


@given(BLOCKS, st.integers(min_value=1, max_value=40), st.integers(1, 64))
@RUNS
def test_it_gives_the_phase_of_the_transform_with_the_offset_it_carries(
        lib, block, bin_index, seed):
    """THE PHASE IS THE TRANSFORM'S PHASE, MEASURED FROM THE OTHER END OF THE
    BLOCK.

    The recurrence carries two numbers and the answer is read off them when the
    block ends, thus its origin stands at the LAST sample where the transform's
    stands at the first. That puts a fixed turn between the two:

        goertzel phase = transform phase + 2 pi k (N - 1) / N

    Measured, the two agree to five decimal places at every block and bin tried.
    It is not a fault and it cannot be removed without making the recurrence
    keep a third number, but a caller setting this beside an fft phase and
    expecting them to match will be wrong by that turn every time."""
    assume(bin_index < block // 2)

    values = noise(block, seed)

    detector = read(lib, block, bin_index, values)
    wanted = dft_bin(values, bin_index)

    # A phase means nothing where there is nothing at that bin to have one.
    assume(abs(wanted) > 0.5)

    offset = TWO_PI * bin_index * (block - 1) / block
    expected = cmath.phase(wanted) + offset

    got = lib.goertzel_phase(detector)

    # Angles wrap, thus the two are compared as points on a circle rather than
    # as numbers.
    apart = abs(cmath.exp(1j * got) - cmath.exp(1j * expected))

    assert apart < 1e-2


@given(BLOCKS, st.integers(min_value=1, max_value=40),
       st.integers(min_value=1, max_value=6), st.integers(1, 64))
@RUNS
def test_moving_the_signal_along_turns_the_phase_by_what_it_should(
        lib, block, bin_index, shift, seed):
    """THE SHIFT RULE, WHICH IS TRUE OF THE TRANSFORM AND OWES NOTHING TO ANY
    CONVENTION. Moving a signal along by one sample turns the phase at bin k by
    exactly one k-th of a turn of the block, and leaves the size alone. Whatever
    origin the phase is measured from cancels in the difference, thus this holds
    the phase to the transform without leaning on the offset above at all."""
    assume(bin_index < block // 2)
    assume(shift < block // 4)

    values = noise(block, seed)

    # Moved along inside the block, which is what the transform of a block
    # treats as moving: what falls off the end comes back at the beginning.
    moved = values[-shift:] + values[:-shift]

    still = read(lib, block, bin_index, values)
    along = read(lib, block, bin_index, moved)

    # The size does not change at all.
    assume(lib.goertzel_magnitude(still) > 0.5)
    assert abs(lib.goertzel_magnitude(along)
               - lib.goertzel_magnitude(still)) <= 1e-3 * (
                   1.0 + lib.goertzel_magnitude(still))

    # And the phase turns by exactly the shift multiplied by the bin.
    turned = TWO_PI * bin_index * shift / block

    one = cmath.exp(1j * lib.goertzel_phase(still))
    other = cmath.exp(1j * (lib.goertzel_phase(along) + turned))

    assert abs(one - other) < 2e-2


@given(BLOCKS, st.integers(min_value=1, max_value=40),
       st.floats(min_value=0.25, max_value=4.0, width=32),
       st.floats(min_value=0.0, max_value=1.0, width=32))
@RUNS
def test_a_tone_sitting_on_the_bin_gives_half_the_block_times_its_height(
        lib, block, bin_index, height, turn):
    """THE ANSWER FOR THE CASE THE MODULE IS REACHED FOR. A tone that sits
    exactly on the bin gives a size of half the block multiplied by the height
    of the tone, whatever phase the tone is at. That is what the transform of a
    tone comes to, and it is what lets a caller turn the answer back into a
    height."""
    assume(0 < bin_index < block // 2)

    values = [sp.to_float32(height * math.sin((TWO_PI * bin_index * index
                                               / block) + (TWO_PI * turn)))
              for index in range(block)]

    detector = read(lib, block, bin_index, values)

    wanted = (height * block) / 2.0

    assert abs(lib.goertzel_magnitude(detector) - wanted) <= 1e-2 * wanted


@given(BLOCKS, st.integers(min_value=2, max_value=30))
@RUNS
def test_a_tone_at_one_bin_is_not_seen_at_another(lib, block, bin_index):
    """A tone that sits exactly on a bin is invisible at every other bin,
    because the turns of the two fit a whole number of times into the block and
    cancel. That is why a transform can tell one frequency from another at
    all."""
    assume(bin_index + 1 < block // 2)

    values = [sp.to_float32(math.sin(TWO_PI * bin_index * index / block))
              for index in range(block)]

    at_the_tone = lib.goertzel_magnitude(read(lib, block, bin_index, values))
    beside_it = lib.goertzel_magnitude(read(lib, block, bin_index + 1, values))

    assert beside_it < (0.01 * at_the_tone)


@given(BLOCKS, st.integers(min_value=1, max_value=40),
       st.floats(min_value=0.25, max_value=8.0, width=32), st.integers(1, 64))
@RUNS
def test_turning_the_signal_up_turns_the_answer_up_by_as_much(lib, block,
                                                              bin_index,
                                                              louder, seed):
    """The transform is linear in the signal, thus so is this. A caller
    measuring a level relies on it."""
    assume(bin_index < block // 2)

    values = noise(block, seed)
    louder_values = [sp.to_float32(value * louder) for value in values]

    plain = lib.goertzel_magnitude(read(lib, block, bin_index, values))
    scaled = lib.goertzel_magnitude(read(lib, block, bin_index,
                                         louder_values))

    assert abs(scaled - (plain * louder)) <= 1e-3 * (1.0 + scaled)


@given(BLOCKS, st.integers(min_value=1, max_value=40), st.integers(1, 64),
       st.integers(1, 64))
@RUNS
def test_the_answer_for_two_signals_added_is_the_two_answers_added(
        lib, block, bin_index, first_seed, second_seed):
    """LINEARITY, WHICH IS WHAT MAKES A TRANSFORM A TRANSFORM. It is held on
    the complex answer and not on the size, because two sizes do not add: two
    tones of the same frequency and opposite phase have sizes that add to
    something and a sum that is nothing."""
    assume(bin_index < block // 2)
    assume(first_seed != second_seed)

    first = noise(block, first_seed)
    second = noise(block, second_seed)
    both = [sp.to_float32(a + b) for a, b in zip(first, second)]

    def complex_answer(values):
        detector = read(lib, block, bin_index, values)
        size = lib.goertzel_magnitude(detector)
        angle = lib.goertzel_phase(detector)

        return size * complex(math.cos(angle), math.sin(angle))

    together = complex_answer(both)
    apart = complex_answer(first) + complex_answer(second)

    scale = 1.0 + abs(apart)

    assert abs(together - apart) <= 1e-2 * scale


@given(BLOCKS, st.integers(min_value=1, max_value=40), st.integers(1, 64))
@RUNS
def test_a_block_is_the_samples_one_at_a_time(lib, block, bin_index, seed):
    """The recurrence is carried from one sample to the next, thus the two forms
    could easily part company."""
    assume(bin_index < block // 2)

    values = noise(block, seed)
    frequency = (bin_index * RATE) / block

    together = read(lib, block, bin_index, values)

    apart = lib.goertzel_init(sp.to_float32(frequency), sp.to_float32(RATE),
                              block)

    for value in values:
        lib.goertzel_process_sample(apart, value)

    assert (lib.goertzel_magnitude(together)
            == lib.goertzel_magnitude(apart))


@given(BLOCKS, st.integers(min_value=1, max_value=40), st.integers(1, 64))
@RUNS
def test_a_detector_says_when_it_has_read_a_whole_block(lib, block, bin_index,
                                                        seed):
    """The answer means nothing until the block is full: the recurrence has to
    run to the end of it before the two numbers it carries stand for the
    transform."""
    assume(bin_index < block // 2)

    values = noise(block, seed)
    frequency = (bin_index * RATE) / block

    detector = lib.goertzel_init(sp.to_float32(frequency),
                                 sp.to_float32(RATE), block)

    for index, value in enumerate(values):
        assert lib.goertzel_is_block_complete(detector) == (index >= block)
        lib.goertzel_process_sample(detector, value)

    assert lib.goertzel_is_block_complete(detector)


@given(BLOCKS, st.integers(min_value=1, max_value=40), st.integers(1, 64))
@RUNS
def test_a_reset_detector_reads_as_a_new_one_does(lib, block, bin_index, seed):
    """The two numbers the recurrence carries are the whole of its memory, thus
    a reset that left either behind would colour the next block."""
    assume(bin_index < block // 2)

    values = noise(block, seed)
    frequency = (bin_index * RATE) / block

    used = lib.goertzel_init(sp.to_float32(frequency), sp.to_float32(RATE),
                             block)

    lib.goertzel_process_block(used, sptk.float_array(values), block)
    lib.goertzel_reset(used)
    lib.goertzel_process_block(used, sptk.float_array(values), block)

    fresh = read(lib, block, bin_index, values)

    assert lib.goertzel_magnitude(used) == lib.goertzel_magnitude(fresh)
