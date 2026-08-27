# matched

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Looking for a known shape. Declared in `sptk/detect/matched.h`.

[Back to the index](../API.md) | [How the detect modules work](../../sptk/detect/README.md)

## Macros

### `MATCHED_LARGEST_LENGTH`

```c
#define MATCHED_LARGEST_LENGTH      65536u
```

### `MATCHED_SCORE_COUNT`

```c
#define MATCHED_SCORE_COUNT(count, length)      (((count) - (length)) + 1u)
```

How many scores a reading of this many samples gives for a shape of this
length. The shape must lie whole inside the reading, thus the last offset
that can be scored is the one where its end reaches the end of the reading.

## Types

### `matched_t`

```c
typedef struct{
    const real_t* pattern;      // The shape being looked for
    uint32_t length;            // How many samples long it is
    real_t root_energy;         // The square root of its energy
    bool designed;              // True once matched_design has been called
}matched_t;
```

## Functions

### `matched_is_valid_length`

```c
bool matched_is_valid_length(uint32_t length);
```

Give whether a shape of this length can be looked for. A shape of no samples
says nothing, and the bound above is what the sums are held to.

### `matched_make`

```c
matched_t matched_make(void);
```

Give a filter that is not yet looking for anything. Give it a shape with
matched_design before asking it for a score.

### `matched_design`

```c
bool matched_design(matched_t* matched, const real_t* pattern, uint32_t length);
```

Tell the filter which shape to look for.

THE FILTER KEEPS THE POINTER AND DOES NOT COPY THE SHAPE. The shape must
stand still for as long as the filter is used, which is what lets a shape of
any length be used without taking memory from the heap.

Give false and leave the filter as it was if the length is not valid or if
the shape holds no energy at all, because a shape of nothing would be found
everywhere.

### `matched_score_block`

```c
bool matched_score_block(const matched_t* matched, const real_t* signal, uint32_t count, real_t* score);
```

Score every offset of a reading.

The output holds MATCHED_SCORE_COUNT(count, length) values, and the value at
offset k says how much the reading looks like the shape when the shape begins
at sample k. Divide by the noise of the reading to read the score in standard
deviations.

Give false if the filter holds no shape or if the reading is shorter than the
shape.

### `matched_score_at`

```c
real_t matched_score_at(const matched_t* matched, const real_t* signal);
```

Score one offset. The reading must hold at least as many samples from here on
as the shape is long.

### `matched_best`

```c
bool matched_best(const matched_t* matched, const real_t* signal, uint32_t count, uint32_t* where, real_t* score);
```

Give the largest score of a reading and where it stands.

This is the answer where a reading is known to hold the shape once and the
only question is when. Where it may hold the shape more than once, or not at
all, score the whole reading and use a threshold.

Give false, and leave both answers as they were, on the same grounds as
matched_score_block.

### `matched_threshold_for`

```c
real_t matched_threshold_for(real_t false_alarm_rate, uint32_t offsets);
```

Give how many standard deviations a score must reach before it is called a
find, for a wanted rate of false alarms.

THE RATE IS FOR THE WHOLE SEARCH AND NOT FOR ONE OFFSET, and the difference
is the mistake this function exists to stop. A threshold that is wrong once
in a thousand offsets is wrong about once in every reading of a thousand
offsets, thus a search that looks at 10 000 offsets with that threshold cries
wolf ten times over. Give the number of offsets that will be looked at and
the rate wanted across all of them together.

The number rests on the noise being even about nothing and on one offset
saying nothing about the next. Where the noise is coloured, the offsets lean
on each other, fewer of them are really free, and the threshold this gives is
higher than it needs to be.

Give a rate above 0 and below 1, and at least one offset. Give 0 otherwise.
