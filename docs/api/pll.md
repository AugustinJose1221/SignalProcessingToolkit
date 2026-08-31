# pll

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Following a tone that will not stay still. Declared in `ffitt/estimate/pll.h`.

[Back to the index](../API.md) | [How the estimate modules work](../../ffitt/estimate/README.md)

## Macros

### `PLL_KEEP`

```c
#define PLL_KEEP                REAL_C(0.999)
```

### `PLL_QUALITY_KEEP`

```c
#define PLL_QUALITY_KEEP(bandwidth) (REAL_C(1.0) - ((bandwidth) / REAL_C(2.0)))
```

THE LOCK MEASURE FOLLOWS THE LOOP AND NOT A FIXED RATE. Held at a fixed rate
it would be slower than a wide loop and would still be reporting the last
answer long after the loop had found a new one. It is held instead at a few
times the bandwidth, thus a loop that arrives quickly says so quickly.

### `PLL_SMALLEST_LOUDNESS`

```c
#define PLL_SMALLEST_LOUDNESS   REAL_C(1.0e-9)
```

## Types

### `pll_t`

```c
typedef struct{
    real_t phase;               // Where the loop thinks the tone is, 0 to 1
    real_t step;                // How far that moves each sample
    real_t free_step;           // Where it was told to start looking
    real_t fast;                // What the error is multiplied by
    real_t slow;                // And what the running total of it is
    real_t gathered;            // That running total
    real_t loudness;            // The running loudness of what arrives
    real_t quality;             // The running measure of how well it is locked
    real_t quality_keep;        // How much of that measure is kept each sample
    real_t bandwidth;           // As it was designed
    real_t damping;             // As it was designed
    bool designed;              // True once pll_design has been called
}pll_t;
```

## Functions

### `pll_is_valid_bandwidth`

```c
bool pll_is_valid_bandwidth(real_t bandwidth);
```

True if this is a bandwidth the loop can be designed at, as a part of the
sample rate. It must be above nothing and well below a half.

### `pll_is_valid_damping`

```c
bool pll_is_valid_damping(real_t damping);
```

True if this is a damping the loop can be designed at.

Below about 0.5 the loop rings: it swings past the answer and comes back
several times before settling. Above about 2 it crawls. 0.707 is the usual
choice and is what gives the fastest arrival with no overshoot.

### `pll_make`

```c
pll_t pll_make(void);
```

Give a loop that is not yet following anything.

### `pll_design`

```c
bool pll_design(pll_t* pll, real_t frequency, real_t sample_rate, real_t bandwidth, real_t damping);
```

Tell the loop where to start looking and how quickly to follow.

The frequency and the sample rate say where to start; the bandwidth says how
quickly to follow, as a part of the sample rate; the damping says how it
behaves on the way. Give 0.707 for the damping where nothing says otherwise.

Give false and leave the loop as it was if the frequency cannot be followed at
this rate, or if the bandwidth or the damping is refused.

### `pll_process_sample`

```c
real_t pll_process_sample(pll_t* pll, real_t sample);
```

Give the loop one sample and take back its own tone at the phase it has
arrived at. That tone is the carrier recovered: it holds the frequency and the
phase of what arrived and none of its noise.

### `pll_process_block`

```c
bool pll_process_block(pll_t* pll, const real_t* input, real_t* output, uint32_t count);
```

Run a whole block through and take back the loop's own tone at every sample.

The loop is left standing where the block ended, thus the next block carries
on from it. Give false if the loop was never designed.

### `pll_get_frequency`

```c
real_t pll_get_frequency(const pll_t* pll, real_t sample_rate);
```

Give the frequency the loop is following, at the sample rate it was designed
at. THIS IS THE MEASUREMENT for a tachometer or a mains watcher.

### `pll_get_phase`

```c
real_t pll_get_phase(const pll_t* pll);
```

Give where in its turn the loop stands, from 0 to 1.

### `pll_lock_quality`

```c
real_t pll_lock_quality(const pll_t* pll);
```

Give how well the loop is following what arrived, from 0 to 1.

THIS MUST BE READ. A loop given noise and no tone settles somewhere and
reports a frequency exactly as confidently as it reports a real one. This is
the only number that tells the two apart: near 1 the loop is following
something, and near 0 it is following nothing.

It is the running mean of how well the loop's own tone lines up with what
arrived, thus it needs a few hundred samples after a change before it means
anything.

### `pll_pull_range`

```c
real_t pll_pull_range(const pll_t* pll);
```

Give roughly how far either side of where it was told to look the loop can
still find a tone, as a part of the sample rate.

### `pll_settle_samples`

```c
uint32_t pll_settle_samples(const pll_t* pll);
```

Give roughly how many samples the loop takes to arrive after a change.

### `pll_reset`

```c
void pll_reset(pll_t* pll);
```

Put the loop back where it was told to start looking, keeping what it was
designed with.
