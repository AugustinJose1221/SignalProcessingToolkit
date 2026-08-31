# Interpolation

A set of points says nothing about what lies between them. This module gives a
smooth curve through them.

## cspline

A cubic spline follows a polynomial of the third power between each pair of
neighbouring points. The polynomials are chosen so that the curve has no step
and no corner at any point: the value, the slope and the bend all agree where
two polynomials meet.

A single polynomial through every point would also fit, but a polynomial of a
high order swings wildly between the points. A spline does not, because each
piece is short and only of the third power.

**How to use it.** Three steps:

1. `cspline_alloc` gives the spline, and `cspline_alloc_mempool` gives the
   memory that the calculation needs.
2. `cspline_init` calculates the coefficients for a set of points. The x values
   must rise, and no two may be the same.
3. `cspline_get_interpolated_point` gives the value of the curve at any place.

The memory pool is separate from the spline because it holds nothing after
`cspline_init` gives back. Several splines can share one pool, one after the
other.

**The coefficient arrays hold one value less than the number of points**, since
each one belongs to an interval and not to a point. The evaluation finds the
interval that holds the place you ask for, with the binary search in
`ffitt/util`, and then reads the coefficients of that interval.

**Outside the range of the points**, the curve follows the polynomial of the
nearest interval, which moves away from the points quickly. Do not read far
outside the range.

The empirical mode decomposition in `ffitt/decompose` is the main user of this
module: it draws a spline through the peaks of a signal and another through the
valleys.


## interp

Reading a value between the points of a table, three ways.

| | |
| --- | --- |
| `INTERP_LINEAR` | a straight line between the two neighbours |
| `INTERP_PCHIP` | a smooth curve that **never goes outside** the two neighbours |
| `cspline` | a smooth curve that may |

**The third line is a trap, and it is why this module exists.** A cubic spline
buys its smoothness by letting the curve overshoot: between two points it may
rise above both or fall below both. Measured, on a table that is flat, steps up
from 0 to 10 once, and is flat again — which is what a calibration of something
with a threshold looks like:

| | lowest | highest | outside the table by |
| --- | --- | --- | --- |
| linear | 0.000 | 10.000 | nothing |
| pchip | 0.000 | 10.000 | nothing |
| `cspline` | **-1.094** | 11.078 | 22 percent |

The spline reports **minus one** for a table holding nothing below zero. Read
as a temperature, that is a device saying it is below freezing because the
table happened to step.

The shape is wrong as well as the range. Walking the same table end to end at
600 places, the spline goes **down at 262 of them** and pchip at none. The
table only ever rises; a device watching for a fall would see 262 that are in
the reading and not in the thing being read.

**Which to take.** Pchip when the table is a measurement that must not be
exceeded. `cspline` when the thing behind the table really is smooth and a
little overshoot is honest. Linear when the cost must be as small as it can be
— but a straight line changes slope at every point of the table, and a device
acting on a rate of change sees a step there that is not real.

**Outside the table the answer is held flat.** Carrying a line on past the end
of a calibration says what the device would read where it was never calibrated,
and saying nothing is better than saying that.

**The inputs must rise through the table**, which is what lets a search find
the place in a few steps rather than by walking it.
