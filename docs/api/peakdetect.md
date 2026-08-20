# peakdetect

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Peak detection. Declared in `sptk/util/peakdetect.h`.

[Back to the index](../API.md) | [How the util modules work](../../sptk/util/README.md)

## Functions

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
