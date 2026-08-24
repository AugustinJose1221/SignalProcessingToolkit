# detrend

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Taking the level and the drift out of a block. Declared in `sptk/filter/detrend.h`.

[Back to the index](../API.md) | [How the filter modules work](../../sptk/filter/README.md)

## Functions

### `detrend_is_valid_kind`

```c
bool detrend_is_valid_kind(detrend_kind_t kind);
```

True if the kind is one this module knows.

### `detrend_trend`

```c
bool detrend_trend(const real_t* input, uint32_t size, detrend_kind_t kind, real_t* offset, real_t* slope);
```

Give the trend of a block without changing it.

The offset is the value of the trend at the MIDDLE of the block, which for
both kinds is the mean. The slope is how much the trend rises for each
sample, and it is zero for the constant kind.

Use this where the trend itself is the thing wanted: the drift of a sensor
across a recording is the slope, multiplied by the number of samples.

Give false if the kind is not known, or if the block is too short: the
constant kind needs one sample and the straight line needs two.

### `detrend_trend_at`

```c
real_t detrend_trend_at(real_t offset, real_t slope, uint32_t size, uint32_t index);
```

Give the value of a trend at one sample of the block.

The offset and the slope must be the ones detrend_trend gave, and the size
must be the size of the same block, because the offset is the value at the
middle and the middle is where the size puts it.

### `detrend_block`

```c
bool detrend_block(const real_t* input, real_t* output, uint32_t size, detrend_kind_t kind);
```

Take the trend out of a block.

The output may be the input, and then the block is changed in place.

Give false if the kind is not known, or if the block is too short.

### `detrend_remove`

```c
bool detrend_remove(const real_t* input, real_t* output, uint32_t size, real_t offset, real_t slope);
```

Take a trend that is already known out of a block.

This is for the second block onwards, where the trend was worked out once
from a block that is known to be quiet and must now be taken out of every
block that follows. Working the trend out afresh from each block would take
the signal out along with the trend, where the signal itself rises across
the block.

The output may be the input. The size must be the size the trend was found
with, for the reason detrend_trend_at gives.

Give false if the block is empty.
