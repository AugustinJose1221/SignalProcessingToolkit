# lattice

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

A filter built as a ladder of stages. Declared in `ffitt/filter/lattice.h`.

[Back to the index](../API.md) | [How the filter modules work](../../ffitt/filter/README.md)

## Macros

### `LATTICE_FLOOR`

```c
#define LATTICE_FLOOR           REAL_C(1.0e-10)
```

The smallest energy a stage will divide by, so that a silent input cannot
make a step run away.

### `LATTICE_LARGEST_RATE`

```c
#define LATTICE_LARGEST_RATE    REAL_C(1.0)
```

### `LATTICE_LARGEST_REFLECTION`

```c
#define LATTICE_LARGEST_REFLECTION  REAL_C(0.99)
```

## Types

### `lattice_t`

```c
typedef struct{
    real_t* reflection;         // One number for each stage
    real_t* forward;            // What each stage could not explain, going on
    real_t* backward;           // The same, held back by one sample
    real_t* held;               // The backward errors of the sample before
    real_t* energy;             // How loud each stage has been
    real_t* weight;             // What each stage contributes to the answer
    uint32_t stages;            // How many stages
    real_t rate;                // How far each step moves
    real_t forgetting;          // How much of the past the energies keep
    real_t before;              // What was left over before this sample
    real_t after;               // What is left over after it
    real_t counted;             // How many samples the energies stand for
    bool designed;              // True once lattice_design has been called
    bool dynamic_alloc;         // True if the memory comes from the heap
}lattice_t;
```

## Functions

### `lattice_is_valid_rate`

```c
bool lattice_is_valid_rate(real_t rate);
```

True if this rate can be used, which is above nothing and not above
LATTICE_LARGEST_RATE.

### `lattice_is_valid_forgetting`

```c
bool lattice_is_valid_forgetting(real_t forgetting);
```

True if this forgetting factor can be used by the energies, which is above
nothing and not above 1.

### `lattice_alloc`

```c
lattice_t lattice_alloc(uint32_t stages);
```

Give a ladder of the given number of stages. The memory comes from the heap.
Give it to lattice_free when it is no longer needed.

### `lattice_static_alloc`

```c
lattice_t lattice_static_alloc(uint32_t stages, real_t* reflection, real_t* forward, real_t* backward, real_t* held, real_t* energy, real_t* weight);
```

Give a ladder that uses the memory the caller holds, taking nothing from the
heap. Each of the six lists must hold stages plus one values.

### `lattice_design`

```c
bool lattice_design(lattice_t* lattice, real_t rate, real_t forgetting);
```

Choose how far each step moves and how much of the past the energies keep.

A rate of about 0.5 and a forgetting factor of about 0.99 suit most work.
This also clears the ladder, thus it is where a run begins.

Give false if either number is one the module cannot use.

### `lattice_process_sample`

```c
real_t lattice_process_sample(lattice_t* lattice, real_t reference, real_t wanted);
```

Put one sample through the ladder and let every stage learn from it.

The reference is what the ladder is given and the wanted value is what it
should have produced. The answer is the error A PRIORI: what the ladder
would have said before it learned anything from this sample. That is the
honest measure, and it is what lattice_error gives as well.

### `lattice_process_block`

```c
bool lattice_process_block(lattice_t* lattice, const real_t* reference, const real_t* wanted, real_t* error, uint32_t count);
```

Run a whole block through, letting every stage learn from every sample.

The error takes what the ladder could not explain at each sample, worked out
BEFORE it learned from that sample, which is what lattice_process_sample
gives and is the honest measure.

Give false if the ladder was never designed.

### `lattice_error_before`

```c
real_t lattice_error_before(const lattice_t* lattice);
```

What was left over before this sample was learned from.

THIS IS THE ONE TO WATCH AND TO RECORD. It says how the filter is doing
without being told the answer first.

### `lattice_error_after`

```c
real_t lattice_error_after(const lattice_t* lattice);
```

What is left over after this sample has been learned from.

THIS IS THE ONE TO USE where the filter is taking something away, because it
has already accounted for the sample in hand. It is always the smaller of
the two, thus it must never be reported as how well the filter is doing.

### `lattice_get_reflection`

```c
real_t lattice_get_reflection(const lattice_t* lattice, uint32_t stage);
```

Give the reflection number of one stage, which says how much that stage
found in common between what came forward and what was held back.

Every one of them lies between -1 and 1. A stage whose number is near either
end has found a great deal; one near nothing has found nothing, and the
stages beyond it are doing no work.

### `lattice_reset`

```c
void lattice_reset(lattice_t* lattice);
```

Clear everything the ladder has learned.

### `lattice_free`

```c
void lattice_free(lattice_t* lattice);
```

Release the memory of a ladder that came from lattice_alloc. This does
nothing for one that came from lattice_static_alloc.
