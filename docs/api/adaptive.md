# adaptive

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

A filter that finds its own coefficients. Declared in `sptk/filter/adaptive.h`.

[Back to the index](../API.md) | [How the filter modules work](../../sptk/filter/README.md)

## Macros

### `ADAPTIVE_FLOOR`

```c
#define ADAPTIVE_FLOOR      REAL_C(1.0e-10)
```

The smallest energy that the normalised rule will divide by, so that a
silent reference cannot make the step run away.

## Types

### `adaptive_t`

```c
typedef struct{
    ringbuf_t history;          // The last samples of the reference
    real_t* coefficient;        // What the filter has learned so far
    uint32_t length;            // How many coefficients
    adaptive_rule_t rule;       // Which rule moves them
    real_t rate;                // How far each step moves
    real_t leak;                // How fast a coefficient falls back to nothing
    real_t energy;              // The energy of what is in the filter now
    bool dynamic_alloc;         // True if the memory comes from the heap
}adaptive_t;
```

## Functions

### `adaptive_is_valid_rule`

```c
bool adaptive_is_valid_rule(adaptive_rule_t rule);
```

True if the module knows this rule.

### `adaptive_alloc`

```c
adaptive_t adaptive_alloc(uint32_t length);
```

Give a filter with the given number of coefficients, all of them nothing.
The memory comes from the heap. Give it to adaptive_free when done.

The length must cover the delay between the reference and the noise in the
signal. A filter shorter than that delay can never learn the answer, however
long it runs.

### `adaptive_static_alloc`

```c
adaptive_t adaptive_static_alloc(uint32_t length, real_t* coefficient, real_t* history);
```

Give a filter that uses the memory of the caller. Both lists must hold as
many values as the length. This function takes no memory from the heap.

### `adaptive_design`

```c
bool adaptive_design(adaptive_t* adaptive, adaptive_rule_t rule, real_t rate);
```

Choose the rule and the rate.

For ADAPTIVE_NORMALISED the rate must lie between 0 and 2, and 0.1 to 0.5
suits most work. For ADAPTIVE_PLAIN the rate that is safe depends on how
large the reference is, and there is no number that suits every signal;
that is why the normalised rule exists.

Give false if the rule is unknown or the rate is not above zero.

### `adaptive_set_leak`

```c
bool adaptive_set_leak(adaptive_t* adaptive, real_t leak);
```

Set how fast a coefficient falls back towards nothing, from 0 to 1.

With no leak, a coefficient that the reference never drives can drift for
ever and hold whatever it drifted to. A small leak, say 0.0001, pulls every
coefficient gently back to nothing, thus only what the reference keeps
driving stays. Set it where the reference is quiet for long stretches.

Give false if the leak is outside 0 to 1.

### `adaptive_reset`

```c
void adaptive_reset(adaptive_t* adaptive);
```

Forget everything that has been learned.

### `adaptive_process_sample`

```c
real_t adaptive_process_sample(adaptive_t* adaptive, real_t reference, real_t wanted);
```

Put one pair in and give what the filter makes of the reference.

THIS IS THE NOISE, NOT THE ANSWER. Take adaptive_error for the answer.

### `adaptive_error`

```c
real_t adaptive_error(adaptive_t* adaptive, real_t reference, real_t wanted);
```

Put one pair in and give what is left when the filter has taken away what it
could. THIS IS THE ANSWER for taking noise away.

### `adaptive_process_block`

```c
bool adaptive_process_block(adaptive_t* adaptive, const real_t* reference, const real_t* wanted, real_t* output, real_t* error, uint32_t count);
```

Run a whole block through, learning from every sample of it.

The output takes what the filter made of the reference and the error takes
what is left when that has been taken away. EITHER MAY BE NULL, and a caller
that wants only one should give NULL for the other rather than a list it
throws away.

THE ERROR IS ALMOST ALWAYS THE ANSWER. The output is the interference as the
filter learned it; the error is what remains, which is the thing that was
wanted. A caller taking the output has taken the noise.

The lists may not overlap the two inputs. Give false if the filter holds no
coefficients.

### `adaptive_get_coefficient`

```c
real_t adaptive_get_coefficient(const adaptive_t* adaptive, uint32_t index);
```

Give one coefficient that the filter has learned.

Worth reading. The coefficients are the answer to what the path between the
two sensors does, and where the largest one stands is the delay between them
in samples.

### `adaptive_get_energy`

```c
real_t adaptive_get_energy(const adaptive_t* adaptive);
```

Give how much of the reference is in the filter now.

This says whether the filter can learn at all. An energy near nothing means
the reference is silent, thus there is nothing to learn from and whatever
the filter holds is what it learned earlier.

### `adaptive_free`

```c
void adaptive_free(adaptive_t* adaptive);
```

Release the memory of a filter that came from adaptive_alloc. This function
does nothing for one that came from adaptive_static_alloc.
