"""Rules that delaying by a part of a sample must keep.

The module is the other half of delay: having measured a delay of 2.35 samples,
something has to apply it. The strongest test here is exactly that round trip,
and it could not be written before both halves existed.
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

ORDERS = st.integers(min_value=1, max_value=8)

# Parts of a sample a float of 32 bits holds exactly.
PARTS = st.sampled_from([0.0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 1.0])

TWO_PI = 2.0 * math.pi


# THE LEVEL ERROR OF THE WEIGHTS, as a share of the signal, measured across a
# hundred delays and written in farrow.h. The weights are worked out from
# products and divisions that grow quickly with the order, thus at 32 bits they
# add up to one less and less exactly. Every test here that compares an answer
# against what the arithmetic says it should be must give it this much room, or
# it is asking the width for more than the width has.
WEIGHT_ROOM = {1: 0.0, 2: 0.0, 3: 1.4e-7, 4: 9.5e-7, 5: 3.6e-5,
               6: 1.4e-5, 7: 3.0e-4, 8: 1.6e-3}


def weight_room(order):
    if ffitt.REAL_64:
        return 1e-10

    # TWENTY TIMES THE MEASURED WORST, and the factor is not timidity. The
    # error is not smooth in the delay: a sweep of a hundred delays reported
    # 1.4e-05 at an order of 6, and generated delays found 5.5e-05 between two
    # of those hundred. A sweep finds the scale of the thing and not its worst.
    #
    # It still catches what it is for. A weight set that had stopped adding up
    # to one would be out by a part in ten or more, which is four orders past
    # this bound.
    return 20.0 * WEIGHT_ROOM[order]


def built(lib, order, part):
    filt = lib.farrow_alloc(order)
    delay = lib.farrow_smallest_delay(order) + part

    assert lib.farrow_set_delay(filt, sp.to_float32(delay))

    return filt, delay


def through(lib, filt, values):
    out = ffitt.real_buffer(len(values))

    assert lib.farrow_process_block(filt, ffitt.float_array(values), out,
                                    len(values))

    return [out[index] for index in range(len(values))]


def tone(count, frequency, shift=0.0):
    return [sp.to_float32(math.sin(TWO_PI * frequency * (index - shift)))
            for index in range(count)]


@given(ORDERS, PARTS, sp.elements(8.0))
@RUNS
def test_a_signal_that_does_not_change_comes_through_unchanged(lib, order,
                                                               part, level):
    """THE RULE THE WEIGHTS MUST OBEY. They add up to one at every delay, thus a
    signal that is not changing has nothing to interpolate and must come out as
    it went in. Weights that did not add up to one would change the level of
    every signal, and a chain of such filters would add that up."""
    filt, _ = built(lib, order, part)

    try:
        found = through(lib, filt, [level] * 60)

        room = (weight_room(order) + 1e-6) * (1.0 + abs(level))

        assert abs(found[-1] - level) <= room
    finally:
        lib.farrow_free(filt)


@given(ORDERS)
@RUNS
def test_a_whole_delay_gives_the_samples_back(lib, order):
    """There is nothing between samples to work out, thus the answer is the
    signal shifted along and not merely close to it."""
    whole = (order + 1) // 2

    filt = lib.farrow_alloc(order)

    try:
        assert lib.farrow_set_delay(filt, sp.to_float32(float(whole)))

        values = [sp.to_float32(math.sin(index * 0.7) * 2.0)
                  for index in range(40)]
        found = through(lib, filt, values)

        room = (weight_room(order) + 1e-5) * 3.0

        for index in range(whole, len(values)):
            assert abs(found[index] - values[index - whole]) <= room
    finally:
        lib.farrow_free(filt)


@given(ORDERS, PARTS)
@RUNS
def test_a_slow_tone_comes_out_delayed_by_what_was_asked_for(lib, order, part):
    """THE REASON THE MODULE EXISTS. A tone well below half the sample rate,
    delayed by a part of a sample, must line up with the same tone worked out
    at that delay directly."""
    frequency = 0.02
    count = 400

    filt, delay = built(lib, order, part)

    try:
        found = through(lib, filt, tone(count, frequency))
        wanted = tone(count, frequency, shift=delay)

        worst = max(abs(found[index] - wanted[index])
                    for index in range(40, count))

        assert worst < 0.01
    finally:
        lib.farrow_free(filt)


@given(ORDERS, PARTS, st.floats(min_value=0.25, max_value=8.0, width=32))
@RUNS
def test_the_answer_scales_with_the_signal(lib, order, part, louder):
    """A delay is a statement about time. Turning the signal up says nothing
    about time, thus the answer must be turned up by exactly as much."""
    values = [sp.to_float32(math.sin(index * 0.3)) for index in range(80)]
    louder_values = [sp.to_float32(value * louder) for value in values]

    plain, _ = built(lib, order, part)
    big, _ = built(lib, order, part)

    try:
        one = through(lib, plain, values)
        other = through(lib, big, louder_values)

        for index in range(len(values)):
            room = (weight_room(order) + 1e-4) * (
                1.0 + (abs(louder) * (1.0 + abs(one[index]))))

            assert abs(other[index] - (one[index] * louder)) <= room
    finally:
        lib.farrow_free(plain)
        lib.farrow_free(big)


@given(PARTS)
@RUNS
def test_a_higher_order_keeps_more_of_a_fast_signal(lib, part):
    """THE REASON TO PAY FOR A HIGHER ORDER, and it is not the delay. Working
    out a value between two samples averages them, and averaging takes the fast
    part of a signal away. A higher order averages over a wider curve and
    quietens it less."""
    frequency = 0.3
    count = 600

    kept = []

    for order in (1, 3, 5, 7):
        filt = lib.farrow_alloc(order)

        try:
            # The delay halfway between two samples, which is the worst place
            # there is.
            assert lib.farrow_set_delay(filt,
                                        lib.farrow_smallest_delay(order))

            found = through(lib, filt, tone(count, frequency))

            kept.append(max(abs(value) for value in found[60:]))
        finally:
            lib.farrow_free(filt)

    for index in range(1, len(kept)):
        assert kept[index] > kept[index - 1]

    # And no order ever gives back more than it was given.
    for value in kept:
        assert value <= 1.0 + 1e-4


@given(ORDERS, PARTS)
@RUNS
def test_a_block_is_the_samples_it_would_have_given_one_at_a_time(lib, order,
                                                                  part):
    """The block is there for speed and must be there for nothing else."""
    values = [sp.to_float32(math.cos(index * 0.4) * 1.5) for index in range(60)]

    together, _ = built(lib, order, part)
    apart, _ = built(lib, order, part)

    try:
        block = through(lib, together, values)
        one_at_a_time = [lib.farrow_process_sample(apart, value)
                         for value in values]

        assert block == one_at_a_time
    finally:
        lib.farrow_free(together)
        lib.farrow_free(apart)


@given(ORDERS, PARTS)
@RUNS
def test_a_reset_filter_answers_as_a_new_one_does(lib, order, part):
    """A sample left over from a run before would colour the start of the next
    one, and a caller measuring from the start would never know."""
    values = [sp.to_float32(math.sin(index * 0.5)) for index in range(50)]

    filt, _ = built(lib, order, part)
    fresh, _ = built(lib, order, part)

    try:
        through(lib, filt, values)
        lib.farrow_reset(filt)

        assert through(lib, filt, values) == through(lib, fresh, values)
        assert abs(lib.farrow_get_delay(filt)
                   - lib.farrow_get_delay(fresh)) <= 1e-6
    finally:
        lib.farrow_free(filt)
        lib.farrow_free(fresh)


@given(st.integers(min_value=0, max_value=20))
def test_only_an_order_the_filter_can_be_built_at_is_taken(lib, order):
    """An order of nothing would take one sample and give it back, which is no
    delay at all."""
    assert lib.farrow_is_valid_order(order) == (1 <= order <= 8)


@given(ORDERS, st.floats(min_value=-4.0, max_value=12.0, width=32))
@RUNS
def test_only_a_delay_inside_the_one_sample_range_is_taken(lib, order, delay):
    """The filter lays a curve through the samples either side of the place
    wanted, thus the place must sit among them. A caller wanting more takes the
    whole samples elsewhere."""
    filt = lib.farrow_alloc(order)

    try:
        smallest = lib.farrow_smallest_delay(order)
        largest = lib.farrow_largest_delay(order)

        assert abs((largest - smallest) - 1.0) <= 1e-6

        inside = smallest <= delay <= largest
        was = lib.farrow_get_delay(filt)

        assert lib.farrow_set_delay(filt, sp.to_float32(delay)) == inside

        if not inside:
            assert lib.farrow_get_delay(filt) == was
    finally:
        lib.farrow_free(filt)


@given(st.sampled_from([3, 5, 7]),
       st.sampled_from([0.125, 0.25, 0.375, 0.5, 0.625, 0.75]))
@RUNS
def test_a_delay_applied_is_a_delay_that_can_be_measured(lib, order, part):
    """THE ROUND TRIP, AND THE TEST THIS RELEASE WAS FOR.

    delay_by_phase measures a delay below a sample. farrow applies one. Neither
    could be checked against anything but itself before the other existed: a
    measurement agreeing with itself says nothing.

    A band of noise is delayed by a known part of a sample and the delay is then
    measured back off the pair. What comes out must be what went in."""
    # THE SIGNAL IS BUILT TO FIT THE WINDOW IT IS MEASURED IN, and that is not
    # a convenience either. The way that reads a delay from the phase works on
    # one block, and a block holding part of a turn of some frequency spreads
    # that frequency across every bin. The phase of the spread has nothing to do
    # with the delay: measured, a signal periodic in 512 samples read in a
    # window of 256 gave 2.57 samples where 1.62 was applied.
    #
    # The signal here is built to repeat every WINDOW samples and then laid down
    # twice, thus the second half is a whole number of turns of every frequency
    # in it and the filter has had the first half to fill up in.
    window = 256

    # AND THE BAND IS KEPT WELL BELOW HALF THE SAMPLE RATE. The delay this
    # filter applies is not the same at every frequency: the table in farrow.h
    # gives the error against frequency and it climbs steeply past a fifth of
    # the rate, thus one delay measured across a band is a weighted average of
    # them. A band reaching to 0.234 of the rate came back 0.073 samples short
    # at an order of 3, which is what that table says it should.
    lowest = 10
    highest = 25

    state = 7
    phases = {}

    for bin_index in range(lowest, highest + 1):
        state = ((state * 1103515245) + 12345) & 0xFFFFFFFF
        phases[bin_index] = TWO_PI * (((state >> 16) % 20000) / 10000.0 - 1.0)

    one_turn = []

    for index in range(window):
        total = 0.0

        for bin_index in range(lowest, highest + 1):
            turn = TWO_PI * bin_index / window
            total += math.sin((turn * index) + phases[bin_index])

        one_turn.append(sp.to_float32(total))

    first = one_turn + one_turn

    filt, delay = built(lib, order, part)

    try:
        second = through(lib, filt, first)
    finally:
        lib.farrow_free(filt)

    # The second half of each, where the filter is full and the block holds a
    # whole number of turns of everything in it.
    one = ffitt.float_array(first[window:])
    other = ffitt.float_array(second[window:])

    fft = lib.fft_alloc(window)
    work_a = (ffitt.Cnum * window)()
    work_b = (ffitt.Cnum * window)()
    found = ffitt.real_buffer(1)

    try:
        assert lib.delay_by_phase(one, other, window, fft, work_a, work_b,
                                  found)
    finally:
        lib.fft_free(fft)

    assert abs(found[0] - delay) < 0.02
