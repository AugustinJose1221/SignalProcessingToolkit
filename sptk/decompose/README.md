# Decomposition

A signal that holds several things at once is hard to read. These modules split
it into parts that each hold one thing.

| Module | What it holds |
| --- | --- |
| `emd` | The empirical mode decomposition |
| `imf` | One intrinsic mode function, which is one part of the result |

## emd

A Fourier transform splits a signal into sines that run through the whole
block. That suits a signal built from sines. It suits badly a signal whose
frequency changes, or one that holds a short event.

The empirical mode decomposition builds its parts from the signal itself and
not from a fixed set of curves. One part is an intrinsic mode function, and it
holds one frequency at a time.

**How one part comes out.** The method repeats these steps:

1. Find every peak and every valley of the signal, with `sptk/util`.
2. Draw a spline through the peaks and a spline through the valleys, with
   `sptk/interpolate`. Those two curves are the envelopes.
3. Take the mean of the two envelopes away from the signal.

What is left rides around zero. The method repeats the steps until it changes
little, and the result is one intrinsic mode function. It then takes that
function away from the signal and starts again with the rest. The rest at the
end is the residue.

**The rule that defines the result.** The sum of every function and the residue
gives the signal again. Nothing is lost and nothing is added. The tests of the
module examine that rule, and it is the first thing to examine in your own use.

**Two limits to keep in mind.** The method has no equation behind it; it is a
set of steps, thus there is little theory to say when it works. And it needs at
least three samples to hold a peak, thus `EMD_MINIMUM_SIZE` refuses a signal
that is too short.

The decomposition alone gives you the parts. To read the frequency of each part
through the time, give them to `hht` in `sptk/transform`. Together they are the
Hilbert-Huang transform.

## imf

This module holds one intrinsic mode function: the position and the value of
each point. It gives the memory for a function, and it writes one or several
functions out as text, one column for each.

The decomposition fills these; this module carries them.
