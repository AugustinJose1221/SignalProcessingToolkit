# propagate

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Carrying a state forward through a rate of change. Declared in `ffitt/estimate/propagate.h`.

[Back to the index](../API.md) | [How the estimate modules work](../../ffitt/estimate/README.md)

## Macros

### `PROPAGATE_LARGEST_STATE`

```c
#define PROPAGATE_LARGEST_STATE     16u
```

## Functions

### `propagate_is_valid_method`

```c
bool propagate_is_valid_method(propagate_method_t method);
```

True if the method is one this module knows.

### `propagate_is_valid_count`

```c
bool propagate_is_valid_count(uint32_t count);
```

True if a model of this many states can be carried.

### `propagate_state`

```c
bool propagate_state(propagate_method_t method, propagate_rate_t rate, real_t time, real_t step, real_t* state, const real_t* input, uint32_t count);
```

Carry the state forward by one step.

The state holds count values and is written over with where it has got to.
The time is where the step begins, and the step is how far to go.

Give false if the method or the count is one the module cannot take, or if
the step is not above nothing.

### `propagate_state_over`

```c
bool propagate_state_over(propagate_method_t method, propagate_rate_t rate, real_t time, real_t across, uint32_t steps, real_t* state, const real_t* input, uint32_t count);
```

Carry the state forward across a stretch, in several steps of equal size.

USE THIS TO REACH THE NEXT MEASUREMENT. The sample rate of a filter fixes
how far apart the measurements are, and that distance is often far too large
for one step. Splitting it costs the same as one step of the same total size
would have cost had the method been asked for it, and gives an answer worth
having.

Give false for the same reasons as propagate_state, or if the number of
steps is nothing.

### `propagate_asks_for_each_step`

```c
uint32_t propagate_asks_for_each_step(propagate_method_t method);
```

How many asks for the rate a method makes for each step.

Use it to weigh one method against another: a method of four asks at a step
of 0.1 costs the same as one of two asks at a step of 0.05, and the table in
the header says which of those two gives the better answer.
