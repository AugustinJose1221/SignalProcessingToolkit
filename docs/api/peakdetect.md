# peakdetect

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Peak detection. Declared in `sptk/util/peakdetect.h`.

[Back to the index](../API.md) | [How the util modules work](../../sptk/util/README.md)

## Types

### `peakdetect_options_t`

```c
typedef struct{
    real_t minimum_height;      // A peak below this is not counted
    real_t minimum_prominence;  // A peak that stands out less is not counted
    real_t minimum_width;       // A peak narrower than this is not counted
    uint32_t minimum_distance;  // No two peaks may stand closer than this
}peakdetect_options_t;
```

## Functions

### `peakdetect_no_rules`

```c
peakdetect_options_t peakdetect_no_rules(void);
```

Give the rules with nothing switched on, so that a caller may set only the
ones it wants.

### `peakdetect_get_peaks`

```c
uint32_t peakdetect_get_peaks(real_t* input, real_t* index_buffer, real_t* peak_buffer, uint32_t size);
```

Find every peak of the signal and give the number of them.

A peak is a sample that is larger than the sample before it and larger than
the sample after it. Thus the first sample and the last sample are never
peaks, and a signal with fewer than three samples holds no peak.

The function writes the index of each peak into index_buffer and the value
of each peak into peak_buffer. Both buffers must hold room for as many
values as the signal holds.

### `peakdetect_prominence`

```c
real_t peakdetect_prominence(const real_t* input, uint32_t size, uint32_t peak);
```

How far the signal must descend from this peak before it can climb to a
higher one, which is the prominence of the peak.

Look from the peak outwards in both directions until the signal rises above
the peak or the signal ends. The lowest point reached on each side is that
side's base. The prominence is the height of the peak above the HIGHER of
the two bases: the peak must clear that one to be worth calling a peak.

Give 0 if the index is not a peak or lies outside the signal.

### `peakdetect_width`

```c
real_t peakdetect_width(const real_t* input, uint32_t size, uint32_t peak, real_t part);
```

How wide the peak is, measured at the given part of the way down from its
top towards its base.

A part of 0.5 measures at half the prominence below the top, which is the
usual choice and is what "the width of a peak" ordinarily means. A part of 1
measures at the base itself.

The two edges are found by looking outwards until the signal falls below
that level, and the place is taken between the two samples either side of
the crossing, thus the width is not limited to whole samples.

Give 0 if the index is not a peak or the part is outside 0 to 1.

### `peakdetect_find`

```c
uint32_t peakdetect_find(const real_t* input, uint32_t size, const peakdetect_options_t* options, uint32_t* index_out, uint32_t room);
```

Find the peaks that pass every rule, and write their indices in the order
they stand in the signal.

The room says how many indices the list can hold. Where more peaks pass than
there is room for, the ones that stand out most are kept.

Give how many indices were written.
