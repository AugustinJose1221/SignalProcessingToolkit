# Detection

These three modules answer questions of the form **did something happen, and
when**. The rest of the library changes a signal or measures it; these say
whether an event is in it.

They share one shape. Each one turns a reading into a number that can be
compared against a threshold, and each one gives the threshold as well as the
number. **The threshold is the module's answer, not the caller's guess**, and
it is where most of the value is: a detector without one is a number nobody can
act on.

| Module | The question it answers |
| --- | --- |
| `matched` | Is this known shape in the reading, and where |
| `delay` | How far does this reading stand behind that one |
| `changepoint` | Has the reading changed, and when did it start |

## The one thing common to all three

**A detector always answers.** Give `matched` a reading with nothing in it and
it still reports a largest score somewhere. Give `delay` two readings with
nothing in common and it still reports the lag where they agree best. That
answer is not wrong; it is simply about noise.

Every one of them therefore gives a second number that says whether the first
one means anything: a score in units of the noise, a strength between -1 and 1,
a running sum against a threshold. **Read the second number.** A detector used
without it finds an event in every reading it is given.

## matched

Slide a known shape along the reading and add up the products at each offset.
Where the reading holds the shape, every product is positive at once. Where it
holds noise, they cancel.

Of everything that can be done to a reading with a known shape buried in noise,
this gives the largest answer for the noise it lets through. Nothing does
better, which is why radar, sonar and every tag reader are built on it.

The score is divided by the square root of the energy of the shape, thus a
reading of pure noise gives a score whose spread is the spread of the noise.
Divide the score by the noise of the reading and it says **how many standard
deviations this offset stands out by** — a number that means the same thing
whatever shape was looked for and however loud it was.

`matched_threshold_for` turns a wanted rate of false alarms into a threshold in
those units. **Give it the number of offsets that will be looked at.** A
threshold that is wrong once in a thousand offsets cries wolf ten times in a
search across ten thousand offsets, and that is the mistake the parameter is
there to stop.

The shape must be the shape that will **arrive**, not the shape that was sent.
A path that stretches or colours it leaves the filter matched to something else,
and the score falls away with nothing to say why.

## delay

A delay of a whole number of samples is easy, and `correlate_best_lag` already
gives it. The whole number is rarely the answer: at 48 000 samples a second, one
sample of delay between two microphones a hand apart is seven degrees of
bearing.

Two ways, and reaching for the right one is most of the work.

| | `DELAY_CORRELATE` | `DELAY_PHASE` |
| --- | --- | --- |
| Needs | Nothing | A transform |
| Works on | Anything | A reading that fills a band |
| Fineness | A fitted curve through three points | Every bin the two share |
| Settles as the reading grows | No | Yes |
| Leans when | The peak is not shaped like the curve | The path colours one reading |

`DELAY_CORRELATE` fits a curve through the peak and its two neighbours. That is
enough to get well inside a sample, and the lean it leaves does **not** go away
with more samples: it is a property of the shape of the peak.

`DELAY_PHASE` reads the slope of the phase across the spectrum. It is finer and
it settles, and it asks for a reading that fills a band bin by bin. A handful of
loud tones far apart leaves it nothing to work with, because it reads the turn
from one bin to the **next**: measured on nine tones spread across the band, a
delay of 7 samples came back as 1.6.

**Use both where it matters.** They agree on a reading that suits them and part
company on one that does not, and that parting is the only warning either of
them gives.

## changepoint

A bearing runs a little warmer. A pump draws a little more current. The change
is **smaller than the noise**, thus no threshold on one sample can find it: one
low enough to catch it fires all day, and one high enough to be quiet never
fires.

What finds it is that the change keeps happening and the noise does not. Add up
how far each sample stands from where it should be, take off half the smallest
change worth finding at every step, and hold the sum at nothing from below:

    high = max(0, high + (sample - expected)/spread - change/2)
    low  = max(0, low  - (sample - expected)/spread - change/2)

The half taken off is what stops the sum running away on noise alone. It puts
the noise on the losing side, where it drifts down to nothing and is held there,
while a real change gives the sum something at every sample.

Holding at nothing is what makes the answer mean **when the reading last looked
ordinary**, rather than how far it has run since the program started.

The threshold is the whole of the trade, and `changepoint.h` holds a measured
table of what each one costs. The delay is roughly the threshold divided by the
change, and `changepoint_delay_for` gives it exactly.

**It believes what it was told, for ever.** A reading whose ordinary level
drifts of its own accord walks away from a level that no longer means anything.
Take the drift off first with `dcblock` or `detrend`, and give this what is
left.
