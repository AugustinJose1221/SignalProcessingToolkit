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
`sptk/util`, and then reads the coefficients of that interval.

**Outside the range of the points**, the curve follows the polynomial of the
nearest interval, which moves away from the points quickly. Do not read far
outside the range.

The empirical mode decomposition in `sptk/decompose` is the main user of this
module: it draws a spline through the peaks of a signal and another through the
valleys.
