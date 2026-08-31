# dcblock

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Taking the level of a signal away. Declared in `ffitt/filter/dcblock.h`.

[Back to the index](../API.md) | [How the filter modules work](../../ffitt/filter/README.md)

## Macros

### `DCBLOCK_MIN_CUTOFF`

```c
#define DCBLOCK_MIN_CUTOFF      REAL_C(0.000000001)
```

### `DCBLOCK_MIN_CUTOFF`

```c
#define DCBLOCK_MIN_CUTOFF      REAL_C(0.000001)
```

## Types

### `dcblock_t`

```c
typedef struct{
    real_t level;               // The level that the tracker follows now
    real_t pole;                // How fast it follows
    bool started;               // True once the first sample has set the level
}dcblock_t;
```

## Functions

### `dcblock_is_valid_cutoff`

```c
bool dcblock_is_valid_cutoff(real_t cutoff);
```

True if the tracker can hold the given cutoff.

### `dcblock_init`

```c
dcblock_t dcblock_init(real_t cutoff);
```

Give a tracker for the given cutoff, which is a part of the sample rate.

The cutoff decides how fast the tracker follows. Set it well below the
slowest thing worth keeping: to keep breathing at 0.1 Hz, a cutoff of 0.01
Hz follows the drift and leaves the breathing alone.

This function takes no memory. A tracker whose cutoff cannot be held gives
back a tracker that passes every sample through unchanged, which
dcblock_is_valid_cutoff can tell the caller about first.

### `dcblock_process_sample`

```c
real_t dcblock_process_sample(dcblock_t* dcblock, real_t sample);
```

Take the level away from one sample and give what is left.

### `dcblock_process_block`

```c
void dcblock_process_block(dcblock_t* dcblock, const real_t* input, real_t* output, uint32_t size);
```

Take the level away from a whole block. The input and the output may be the
same list.

### `dcblock_get_level`

```c
real_t dcblock_get_level(const dcblock_t* dcblock);
```

Give the level that the tracker holds now.

This is worth reading on its own. It is the slow part of the signal, thus it
carries the drift, the wander of a contact, and anything else that moves
more slowly than the cutoff.

### `dcblock_set_level`

```c
void dcblock_set_level(dcblock_t* dcblock, real_t level);
```

Set the level directly.

Use this where the level is already known, for example from a calibration.
The tracker then does not have to find it, and it is settled at once.

### `dcblock_reset`

```c
void dcblock_reset(dcblock_t* dcblock);
```

Forget the level. The next sample sets it again, as the first one did.
