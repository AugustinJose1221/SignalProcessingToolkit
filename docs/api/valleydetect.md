# valleydetect

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Valley detection. Declared in `sptk/util/valleydetect.h`.

[Back to the index](../API.md) | [How the util modules work](../../sptk/util/README.md)

## Functions

### `valleydetect_get_valley`

```c
uint32_t valleydetect_get_valley(real_t* input, real_t* index_buffer, real_t* valley_buffer, uint32_t size);
```

Find every valley of the signal and give the number of them.

A valley is a sample that is smaller than the sample before it and smaller
than the sample after it. Thus the first sample and the last sample are
never valleys, and a signal with fewer than three samples holds no valley.

The function writes the index of each valley into index_buffer and the value
of each valley into valley_buffer. Both buffers must hold room for as many
values as the signal holds.
