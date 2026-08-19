# iir

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Filters with an infinite impulse response. Declared in `sptk/filter/iir.h`.

[Back to the index](../API.md) | [How the filter modules work](../../sptk/filter/README.md)

## Macros

### `IIR_COEFFICIENT_COUNT`

```c
#define IIR_COEFFICIENT_COUNT       5u
```

The number of coefficients of one section: b0, b1, b2, a1 and a2.

### `IIR_STATE_COUNT`

```c
#define IIR_STATE_COUNT             2u
```

The number of values of the state of one section.

### `IIR_COEFFICIENT_SIZE`

```c
#define IIR_COEFFICIENT_SIZE(sections)  ((sections) * IIR_COEFFICIENT_COUNT)
```

The number of float values that a filter with the given number of sections
needs for its coefficients.

### `IIR_STATE_SIZE`

```c
#define IIR_STATE_SIZE(sections)        ((sections) * IIR_STATE_COUNT)
```

The number of float values that a filter with the given number of sections
needs for its state.

### `IIR_MIN_CUTOFF`

```c
#define IIR_MIN_CUTOFF      0.001f
```

The smallest cutoff that a design in single precision can hold.

A section keeps its poles near the circle when the cutoff is low, and how
near decides how much precision the coefficients need. A float holds about
seven digits. Below this cutoff those digits run out: the coefficients round
to values that no longer describe the filter that was asked for, and the
filter gives a wrong answer WITHOUT SAYING SO.

Measured, at the gain that should be 1.0 at zero frequency:

    cutoff    0.0100   0.0020   0.0010   0.0005   0.0001
    gain      1.0000   1.0014   0.9959   0.9909   0.6849

Thus 0.002 and above is safe, 0.001 costs about one percent, and below
0.0005 the answer means nothing. The limit stands at 0.001.

A cutoff below this is nearly always a sign that the sample rate is too high
for the work. A cutoff of 0.5 Hz against 32 kHz is 0.000016 and cannot be
held; the same cutoff against 500 Hz is 0.001 and can. Bring the rate down
first, as the guide of this area says.

## Types

### `iir_t`

```c
typedef struct{
    uint32_t sections;          // The number of biquad sections
    float* coefficient;         // Five coefficients for each section
    float* state;               // Two values for each section
    bool dynamic_alloc;         // True if the memory comes from the heap
}iir_t;
```

## Functions

### `iir_is_valid_cutoff`

```c
bool iir_is_valid_cutoff(float cutoff);
```

True if a design can hold the given cutoff, which is a part of the sample
rate. Ask this before a design when the cutoff comes from a measurement or
from a setting, because a design that cannot hold its cutoff gives back a
filter that looks right and is not.

### `iir_alloc`

```c
iir_t iir_alloc(uint32_t sections);
```

Give a filter with the given number of sections. The memory comes from the
heap. The filter lets everything pass until a design function or
iir_set_section gives it coefficients. Give the filter to iir_free when you
no longer need it.

### `iir_static_alloc`

```c
iir_t iir_static_alloc(uint32_t sections, float* coefficient, float* state);
```

Give a filter that uses the memory that the caller holds. The list
coefficient must hold IIR_COEFFICIENT_SIZE(sections) float values, and the
list state must hold IIR_STATE_SIZE(sections) of them. This function takes
no memory from the heap.

### `iir_design_low_pass`

```c
bool iir_design_low_pass(iir_t* iir, float cutoff);
```

Build the coefficients of a filter of Butterworth that lets the low
frequencies pass. The order of the filter is two times the number of
sections.
Give false and leave the filter as it was if the cutoff is outside
IIR_MIN_CUTOFF to 0.5.

### `iir_design_high_pass`

```c
bool iir_design_high_pass(iir_t* iir, float cutoff);
```

Build the coefficients of a filter of Butterworth that lets the high
frequencies pass.
Give false and leave the filter as it was if the cutoff is outside
IIR_MIN_CUTOFF to 0.5.

### `iir_set_section`

```c
void iir_set_section(iir_t* iir, uint32_t section, float b0, float b1, float b2, float a0, float a1, float a2);
```

Write the five coefficients of one section. The three coefficients b belong
to the input, and the two coefficients a belong to the feedback. The
function divides every coefficient by a0, thus the caller may give the
coefficients as another program calculated them.

### `iir_process_sample`

```c
float iir_process_sample(iir_t* iir, float sample);
```

Give the filtered value of one sample.

### `iir_process_block`

```c
void iir_process_block(iir_t* iir, const float* input, float* output, uint32_t size);
```

Filter a block of samples. The input and the output may be the same list.

### `iir_reset`

```c
void iir_reset(iir_t* iir);
```

Set the state of every section to zero. The filter then behaves as a filter
that has seen no sample yet.

### `iir_get_gain`

```c
float iir_get_gain(iir_t* iir, float frequency);
```

Give the size of the answer of the filter at the given frequency, which is a
part of the sample rate. A value of 1 says that the frequency passes
unchanged, and a value of 0 says that the filter stops it.

### `iir_free`

```c
void iir_free(iir_t* iir);
```

Release the memory of a filter that came from iir_alloc. This function does
nothing for a filter that came from iir_static_alloc.
