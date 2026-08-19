# Core

One module that every area may reach for, and three headers that hold no module
of their own. The three give the types and the macros that the other areas
share, thus they hold no `.c` file and no function.

| Header | What it gives |
| --- | --- |
| `ringbuf.h` | A buffer that holds the last samples and forgets the rest |
| `callback.h` | The type of a function that writes text |
| `defs.h` | `ASSERT` |
| `point2d.h` | A point on a plane |

## ringbuf.h

A program that reads a signal as it arrives needs the samples that came before
the one in hand. A filter needs them, a detector that looks back for a peak
needs them, and a transform needs a whole block of them. But the signal never
ends, thus the program cannot keep it all.

This buffer holds a fixed number of the newest samples. When it is full, a new
sample takes the place of the oldest. Nothing is copied and nothing is moved:
only one position changes. Thus putting a sample in costs the same whether the
buffer holds ten samples or ten thousand.

**Three uses.** As a **delay line**, put the sample that arrived and then take
the sample from a number of steps ago. As a **window**, `ringbuf_copy` writes a
flat list, oldest first, for a transform or a median or a filter of Savitzky
and Golay to read. As **history**, a detector that fires on one sample can look
back to find where the event really stood.

**An age, not a position.** `ringbuf_get` takes an age: 0 is the newest sample,
1 the one before it. The meaning of a number then does not change as the buffer
fills, which the meaning of a position would.

**The size is any number above zero.** It need not be a power of two. The head
turns back to the start with a comparison and not with a remainder, because a
division costs far more than a comparison on a small processor.

## callback.h

`print_t` is the type of a function that writes text.

Every module that writes something takes a function of this type. `printf` has
that type, thus a caller can give `printf` directly. A caller on a target with
no console gives its own function, for example one that writes to a serial
port. A caller that gives `NULL` gets `printf`.

## defs.h

Holds `ASSERT`. In a normal build it is `assert` from the standard library. In
a test build, where `TEST` is defined, it becomes nothing.

The reason: a test gives a module wrong input on purpose, to examine what the
module does with it. An assertion would stop the whole test program instead.

**Two things follow.** An assertion must never hold work that the program
needs, because in a test build it disappears. And an assertion is not a check
that stays in a release: a build with `NDEBUG` takes it away as well, thus a
module must not use an assertion to hold a value inside a safe range. Where
that matters, the module holds the value itself, as `binarysearch` does.

## point2d.h

`point2d_t` holds a point on a plane as two float members.
