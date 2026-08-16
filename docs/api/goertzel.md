# goertzel

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Detection of one frequency. Declared in `sptk/transform/goertzel.h`.

[Back to the index](../API.md)

## Types

### `goertzel_t`

```c
typedef struct{
    float coefficient;          // Comes from the frequency and the block size
    float sine;                 // Holds the phase of the result
    float cosine;               // Holds the phase of the result
    float first;                // The state of one sample ago
    float second;               // The state of two samples ago
    uint32_t block_size;        // The number of samples of one block
    uint32_t count;             // The number of samples that came in
}goertzel_t;
```

## Functions

### `goertzel_init`

```c
goertzel_t goertzel_init(float frequency, float sample_rate, uint32_t block_size);
```

Give a detector for one frequency.

The frequency and the sample rate are both in hertz, and the frequency must
be below half the sample rate. The block size is the number of samples that
the detector reads before it gives a result.

This function takes no memory. The whole state lies inside the structure,
thus a caller on a target with no heap can hold it anywhere.

### `goertzel_process_sample`

```c
void goertzel_process_sample(goertzel_t* goertzel, float sample);
```

Give one sample to the detector.

### `goertzel_process_block`

```c
void goertzel_process_block(goertzel_t* goertzel, const float* input, uint32_t size);
```

Give a block of samples to the detector.

### `goertzel_is_block_complete`

```c
bool goertzel_is_block_complete(goertzel_t* goertzel);
```

True when the detector has read a whole block. Read the result then, and
call goertzel_reset before the next block.

### `goertzel_magnitude_squared`

```c
float goertzel_magnitude_squared(goertzel_t* goertzel);
```

Give the square of the size of the answer at the frequency of the detector.
This function takes no square root, thus it is faster than
goertzel_magnitude. Use it to compare the strength of two frequencies.

### `goertzel_magnitude`

```c
float goertzel_magnitude(goertzel_t* goertzel);
```

Give the size of the answer at the frequency of the detector.

### `goertzel_phase`

```c
float goertzel_phase(goertzel_t* goertzel);
```

Give the phase of the answer in radians, between -pi and pi.

### `goertzel_reset`

```c
void goertzel_reset(goertzel_t* goertzel);
```

Set the state to zero, so that the detector can read a new block. The
frequency and the block size do not change.
