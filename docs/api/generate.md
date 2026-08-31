# generate

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Making the signals to test with. Declared in `ffitt/util/generate.h`.

[Back to the index](../API.md) | [How the util modules work](../../ffitt/util/README.md)

## Macros

### `GENERATE_BROWN_KEEP`

```c
#define GENERATE_BROWN_KEEP     REAL_C(0.999)
```

### `GENERATE_DEFAULT_PART`

```c
#define GENERATE_DEFAULT_PART   REAL_C(0.5)
```

The part of a turn a pulse fills, where none is given.

### `GENERATE_PINK_PARTS`

```c
#define GENERATE_PINK_PARTS     7u
```

How many running parts the pink noise is made from.

Each part changes half as often as the one before it, and together they give
a slope of about 3 dB for each doubling of frequency. Seven parts hold that
slope across about seven octaves, which covers any sample rate this library
is used at.

## Types

### `generate_t`

```c
typedef struct{
    generate_kind_t kind;       // Which shape
    real_t phase;               // Where in the turn, from 0 to 1
    real_t step;                // How far the phase moves each sample
    real_t sweep;               // How far the step moves each sample
    real_t last_step;           // What the step was, for the sweep to end on
    uint32_t seed;              // Where the random values stand
    real_t pink[GENERATE_PINK_PARTS];   // The running parts of the pink noise
    real_t part;                // The part of a turn a pulse fills
    real_t running;             // The running sum of the brown noise
    real_t last_pink;           // The pink value before this one, for the blue
    real_t spare;               // The second of a pair of normal draws
    uint32_t counted;           // How many samples have been made
    bool has_spare;             // True while spare holds a draw not yet given
    bool designed;              // True once generate_design has been called
}generate_t;
```

## Functions

### `generate_is_valid_kind`

```c
bool generate_is_valid_kind(generate_kind_t kind);
```

True if the kind is one this module knows.

### `generate_is_valid_frequency`

```c
bool generate_is_valid_frequency(real_t frequency, real_t sample_rate);
```

True if this frequency can be made at this sample rate, which means above
nothing and below half the rate.

A frequency at or above half the sample rate cannot be told from a lower
one, and asking for it gives an answer about a frequency nobody wanted.

### `generate_make`

```c
generate_t generate_make(generate_kind_t kind);
```

Give a maker of the given shape, standing at the start of its turn.

This takes no memory at all: everything it holds is in the type. Thus there
is no free, and one may be made on the stack.

### `generate_design`

```c
bool generate_design(generate_t* generate, real_t frequency, real_t sample_rate);
```

Choose the frequency and the sample rate.

The phase is left where it stands, thus the frequency of a running maker may
be changed at any sample and the wave carries on from where it was. That is
what makes it possible to follow something.

The two noises ignore the frequency. Give false if the kind is unknown, or
if the frequency cannot be made at this rate and the kind is not a noise.

### `generate_design_sweep`

```c
bool generate_design_sweep(generate_t* generate, real_t from, real_t to, real_t sample_rate, uint32_t samples);
```

Set the frequency to move steadily from one to another across a number of
samples, which makes a chirp.

A CHIRP IS THE MOST USEFUL TEST SIGNAL THERE IS, because it visits every
frequency in one run. One chirp through a filter shows the whole of what the
filter does, where a set of tones shows only the frequencies that were
chosen.

Give false if either frequency cannot be made at this rate, or the number of
samples is nothing.

### `generate_set_seed`

```c
void generate_set_seed(generate_t* generate, uint32_t seed);
```

Set where the random values start, so that a run can be repeated exactly.

A TEST THAT CANNOT BE REPEATED IS NOT A TEST. The same seed gives the same
values on every machine and at either width, thus a fault found once can be
found again.

### `generate_is_valid_part`

```c
bool generate_is_valid_part(real_t part);
```

True if this is a part of a turn a pulse can fill, which means above nothing
and below one. A pulse that filled none of the turn or all of it would have
no corners and would not be a pulse.

### `generate_set_part`

```c
bool generate_set_part(generate_t* generate, real_t part);
```

Choose how much of each turn a pulse fills.

GENERATE_PULSE is high for this part of the turn and low for the rest, thus
a part of a half gives the square wave and a part of a tenth gives a narrow
pulse standing once each turn.

GENERATE_GAUSSIAN_PULSE reads it as the WIDTH of its bump, as a part of the
turn: the bump falls away by the same amount at this distance either side of
the middle of the turn as a normal spread falls away at one standard
deviation. A part of about an eighth gives a bump that has died away by the
ends of its turn; anything much wider runs into the turn beside it.

Every other kind ignores it. Give false and leave the maker as it was if the
part is not one generate_is_valid_part accepts.

### `generate_get_part`

```c
real_t generate_get_part(const generate_t* generate);
```

Give the part of a turn a pulse fills.

### `generate_sample`

```c
real_t generate_sample(generate_t* generate);
```

Make the next sample.

### `generate_block`

```c
bool generate_block(generate_t* generate, real_t* output, uint32_t count);
```

Fill a list with the next samples.

Give false if the maker has not been designed.

### `generate_reset`

```c
void generate_reset(generate_t* generate);
```

Put the maker back to the start of its turn, without changing the frequency.

### `generate_get_phase`

```c
real_t generate_get_phase(const generate_t* generate);
```

Give where in the turn the maker stands, from 0 to 1.

Use it to make two shapes that keep step with each other: set one from the
other after each sample.

### `generate_set_phase`

```c
void generate_set_phase(generate_t* generate, real_t phase);
```

Set where in the turn the maker stands, from 0 to 1.
