# changepoint

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Saying when a reading has changed. Declared in `sptk/detect/changepoint.h`.

[Back to the index](../API.md) | [How the detect modules work](../../sptk/detect/README.md)

## Macros

### `CHANGEPOINT_DEFAULT_CHANGE`

```c
#define CHANGEPOINT_DEFAULT_CHANGE      REAL_C(1.0)
```

### `CHANGEPOINT_DEFAULT_THRESHOLD`

```c
#define CHANGEPOINT_DEFAULT_THRESHOLD   REAL_C(5.0)
```

## Types

### `changepoint_t`

```c
typedef struct{
    real_t expected;            // Where the reading sits when nothing is wrong
    real_t deviation;           // How far it wanders there
    real_t smallest_change;     // The smallest change worth finding
    real_t threshold;           // How far a sum must run before it says so
    real_t high;                // The running sum upwards
    real_t low;                 // And downwards
    uint32_t since_high;        // Samples since the upward sum left nothing
    uint32_t since_low;         // And the downward one
    uint32_t counted;           // Samples since the last alarm
    bool designed;              // True once changepoint_design has been called
}changepoint_t;
```

## Functions

### `changepoint_is_valid_deviation`

```c
bool changepoint_is_valid_deviation(real_t deviation);
```

Give whether the reading wanders by an amount this can work with. A wander of
nothing means every sample is exact, and then a change of any size shows in
one sample and needs none of this.

### `changepoint_is_valid_change`

```c
bool changepoint_is_valid_change(real_t smallest_change);
```

Give whether a change of this size is one that can be looked for. The size is
in units of how far the reading wanders, thus 1.0 means a change the size of
the noise and 0.5 means half of it.

### `changepoint_is_valid_threshold`

```c
bool changepoint_is_valid_threshold(real_t threshold);
```

Give whether a sum must run this far. The threshold is in the same units, and
it is the whole of the trade between how often the alarm is wrong and how
long it takes.

### `changepoint_make`

```c
changepoint_t changepoint_make(void);
```

Give a watcher that is not yet watching anything.

### `changepoint_design`

```c
bool changepoint_design(changepoint_t* changepoint, real_t expected, real_t deviation, real_t smallest_change, real_t threshold);
```

Tell the watcher what ordinary looks like and what to look for.

The expected value and the deviation are what the reading does when nothing
is wrong, and they are usually measured from a stretch of reading known to be
good: stats_mean and stats_deviation give both.

The smallest change and the threshold are in units of the deviation. A
smallest change of 1.0 and a threshold of 5.0 is the usual place to start.

THE THRESHOLD IS THE WHOLE OF THE TRADE, and these are the two numbers it
trades between. Measured on twenty million samples of normal noise with a
smallest change of 1.0, and none of it changing at all:

  threshold    one wrong alarm in    samples to find a change of 1.0
  ---------    ------------------    -------------------------------
       4.0                   168                                  8
       5.0                   465                                 10
       6.0                  1265                                 12
       8.0                  9281                                 16

Reading down the table is the choice. A watcher on a bearing that is read
once a second sees 86 400 samples a day, thus a threshold of 5.0 cries wolf
about 185 times a day and a threshold of 8.0 about nine times. Neither number
is right; which one is depends on what a wrong alarm costs and what a missed
change costs, and the arithmetic cannot say.

The right hand column is changepoint_delay_for, which is exact. The middle
column moves with the SHAPE of the noise as well as its spread: the same
measurement on an even spread rather than a normal one gave one wrong alarm
in 372 samples at a threshold of 5.0 and one in 5115 at 8.0. Measure it on
your own reading where it matters.

Give false and leave the watcher as it was if any of the four is refused.

### `changepoint_process_sample`

```c
changepoint_way_t changepoint_process_sample(changepoint_t* changepoint, real_t sample);
```

Give the watcher one sample and hear whether the reading has changed.

THE ANSWER IS GIVEN ONCE AND THE SUMS ARE THEN PUT BACK TO NOTHING, so that
the next alarm is about the next change and not about the same one still
running. Where the change is still there, the sums build again and the alarm
comes again after the same delay.

### `changepoint_began_ago`

```c
uint32_t changepoint_began_ago(const changepoint_t* changepoint);
```

Give how many samples ago the change that was just reported began.

This is the number that makes the alarm useful. The alarm arrives late by
design, thus knowing WHEN it arrived says little; knowing when the reading
started running away says which batch, which shift or which load it was.

Only meaningful straight after changepoint_process_sample gave something
other than CHANGEPOINT_NONE.

### `changepoint_running_high`

```c
real_t changepoint_running_high(const changepoint_t* changepoint);
```

Give how far the upward sum has run, in units of the deviation. It reaches
the threshold when the alarm goes. Reading it between alarms says how close
the reading is standing to one.

### `changepoint_running_low`

```c
real_t changepoint_running_low(const changepoint_t* changepoint);
```

And the downward sum.

### `changepoint_delay_for`

```c
real_t changepoint_delay_for(const changepoint_t* changepoint, real_t change);
```

Give roughly how many samples it takes to find a change of the given size,
once that change has begun. The size is in units of the deviation.

This is the number to choose a threshold by. A threshold twice as high is
wrong half as often and takes twice as long, and which of those matters is
not something the arithmetic can say.

Give 0 where the change is not one the watcher would find at all, which is
any change smaller than half the smallest change it was designed for: below
that the sum wanders about nothing and never arrives.

### `changepoint_reset`

```c
void changepoint_reset(changepoint_t* changepoint);
```

Put both sums back to nothing and forget how long they have been running.
What the watcher was told about the reading is kept.
