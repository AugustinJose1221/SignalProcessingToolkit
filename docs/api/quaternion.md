# quaternion

This file comes from the comments in the headers. Do not change it by hand.
To make it again, give:

```bash
python3 scripts/api_doc.py
```

Which way something points. Declared in `sptk/linalg/quaternion.h`.

[Back to the index](../API.md) | [How the linalg modules work](../../sptk/linalg/README.md)

## Macros

### `QUATERNION_TOLERANCE`

```c
#define QUATERNION_TOLERANCE    REAL_C(0.0001)
```

How near two of them must be to count as the same.

## Types

### `quaternion_t`

```c
typedef struct{
    real_t w;                   // The part that carries the angle
    real_t x;                   // The three parts that carry the axis
    real_t y;
    real_t z;
}quaternion_t;
```

## Functions

### `quaternion_make`

```c
quaternion_t quaternion_make(real_t w, real_t x, real_t y, real_t z);
```

Give one from its four numbers.

### `quaternion_identity`

```c
quaternion_t quaternion_identity(void);
```

Give the one that turns nothing at all.

### `quaternion_from_axis_angle`

```c
quaternion_t quaternion_from_axis_angle(real_t x, real_t y, real_t z, real_t angle);
```

Give the one that turns by the given angle, in radians, about the given
axis. The axis need not be of unit length; it is made so first.

An axis of no length at all cannot say which way to turn. It gives back the
one that turns nothing.

### `quaternion_to_axis_angle`

```c
void quaternion_to_axis_angle(quaternion_t q, real_t* x, real_t* y, real_t* z, real_t* angle);
```

Write the axis and the angle back out. Give NULL for anything not wanted.

The angle comes back between 0 and pi, and the axis is turned to suit. A
turn of nothing has no axis to speak of, and the axis then comes back as
1, 0, 0.

### `quaternion_magnitude`

```c
real_t quaternion_magnitude(quaternion_t q);
```

Give how long the four are together. A proper attitude has a length of 1.

### `quaternion_normalise`

```c
quaternion_t quaternion_normalise(quaternion_t q);
```

Give the same attitude with its length put back to 1.

CALL THIS FROM TIME TO TIME. Every multiplication moves the length a little,
and after a few thousand the drift shows. One of length nothing cannot be
made to have length one; that gives back the one that turns nothing.

### `quaternion_conjugate`

```c
quaternion_t quaternion_conjugate(quaternion_t q);
```

Give the turn that undoes this one.

For a proper attitude this is the conjugate, which costs three changes of
sign and nothing else. That is why holding an attitude this way is cheap to
undo, where undoing a matrix is a great deal more work.

### `quaternion_multiply`

```c
quaternion_t quaternion_multiply(quaternion_t a, quaternion_t b);
```

Give the turn that is b followed by a.

THE ORDER MATTERS. This is the same order as multiplying two rotation
matrices: the one on the right happens first.

### `quaternion_add`

```c
quaternion_t quaternion_add(quaternion_t a, quaternion_t b);
```

Give the two added together, one part at a time.

This is NOT a turn followed by another turn; use quaternion_multiply for
that. It is here because carrying an attitude forward through time needs it.

### `quaternion_scale`

```c
quaternion_t quaternion_scale(quaternion_t q, real_t factor);
```

Give every part multiplied by the same number.

### `quaternion_dot`

```c
real_t quaternion_dot(quaternion_t a, quaternion_t b);
```

Give how much the two point the same way, which is 1 when they are the same
attitude and 0 when they are a quarter turn apart.

### `quaternion_is_same_attitude`

```c
bool quaternion_is_same_attitude(quaternion_t a, quaternion_t b, real_t tolerance);
```

True if the two hold the same attitude.

This allows for the sign: q and the negative of q are the same attitude,
reached by turning one way or the other way round. Comparing the four
numbers straight would call them different, and a great deal of trouble
comes from that.

### `quaternion_rotate`

```c
void quaternion_rotate(quaternion_t q, real_t x, real_t y, real_t z, real_t* out_x, real_t* out_y, real_t* out_z);
```

Turn a vector by the attitude, and write the result. The result may be the
same three numbers that were given.

### `quaternion_to_matrix_into`

```c
void quaternion_to_matrix_into(quaternion_t q, matrix_t* dest);
```

Write the rotation matrix of the attitude into the destination, which must
be 3 by 3.

Take this when one attitude is about to be applied to MANY vectors. Turning
one vector by a quaternion costs about as much as by a matrix, but building
the matrix once and using it many times costs less than either.

### `quaternion_from_matrix`

```c
quaternion_t quaternion_from_matrix(matrix_t* matrix);
```

Give the attitude of a rotation matrix, which must be 3 by 3.

The way back is not simply the way out reversed: reading the wrong one of
four possible forms loses accuracy when the turn is near a half circle. This
reads whichever of the four is largest, thus it is accurate for every
attitude.

### `quaternion_integrate`

```c
quaternion_t quaternion_integrate(quaternion_t q, real_t rate_x, real_t rate_y, real_t rate_z, real_t step);
```

Carry an attitude forward by a turn rate measured in the body, over a step
of time.

This is what a gyroscope gives: how fast the thing is turning about each of
its own three axes, in radians for each second. The result is normalised,
because that is what a caller carrying an attitude forward always wants.

The step must be short enough that the attitude does not change much within
it. A gyroscope read at 100 times a second and a thing turning at one turn
each second moves 3.6 degrees between reads, which is short enough.

### `quaternion_slerp`

```c
quaternion_t quaternion_slerp(quaternion_t a, quaternion_t b, real_t part);
```

Give the attitude that lies the given part of the way from one to the other,
turning at a steady rate along the shortest way round.

A part of 0 gives the first and 1 gives the second. This is what smooths a
camera between two attitudes, or fills in between two readings.

Adding the two and normalising is the obvious shortcut and it does not turn
at a steady rate: it hurries through the middle. This does.
