# Core

One module that every area may reach for, and three headers that hold no module
of their own. The three give the types and the macros that the other areas
share, thus they hold no `.c` file and no function.

| Header | What it gives |
| --- | --- |
| `real.h` | `real_t`, the one type that holds every number |
| `ringbuf.h` | A buffer that holds the last samples and forgets the rest |
| `callback.h` | The type of a function that writes text |
| `defs.h` | `ASSERT` |
| `point2d.h` | A point on a plane |

## When the heap gives nothing

Every module that takes memory offers two ways to get it: a `*_alloc` that
takes it from the heap, and a `*_static_alloc` that uses memory the caller
already holds. The static way cannot fail. The heap way can, and every module
answers it THE SAME WAY:

- what was already got is given back,
- every pointer is left NULL,
- the size fields are set to nothing,
- `dynamic_alloc` is set to false, because a thing that holds nothing owns
  nothing, and giving it to `*_free` must stay harmless,
- and that struct is given back.

Thus a caller examines a size, or a pointer, to know whether it got what it
asked for. A struct that says it holds nothing is safe to free and safe to
throw away.

**Why this is written down.** Eleven modules did not do it. They read the
answer of `malloc` straight into the struct and carried on, and four of them —
`fir`, `iir`, `medfilt` and `savgol` — then cleared the list they had just
failed to get, which is a write through NULL. The other seven handed back a
struct whose size said it held memory that it did not. The modules that did
guard, `rls` and `bluestein` among them, already worked this way; this is their
rule, written down and made to hold everywhere.

## real.h

**Every number in the library is a `real_t`.** No module anywhere spells
`float` or `double`. The width is decided one time, for the whole build, and
never module by module.

```bash
cmake -S . -B build                        # 32 bits, the default
cmake -S . -B build -DFFITT_REAL_64=ON      # 64 bits
```

**Which to choose.** Take 32 bits on a small processor: a number is half the
memory, and a processor with a unit for 32 bit arithmetic and none for 64 bit
runs the wider build tens of times more slowly, because every operation becomes
a call to a library that does it in software.

Take 64 bits when the numbers are large, when the filters are slow, or when the
answer matters more than the time. A number at 32 bits holds about seven
digits, and three kinds of work run out of them:

| The trouble | What it looks like |
| --- | --- |
| A large offset | A reading at 8 000 000 counts with a signal of a few thousand on top spends six of the seven digits on the part that carries nothing |
| A long sum | Five samples near eight million have a variance of exactly 2; at 32 bits `stats_variance` gives 2.25 |
| A slow filter | A section lifts its own rounding error by a large factor. `IIR_MIN_CUTOFF` follows the width: 0.001 at 32 bits and 0.000001 at 64, a thousand times lower |

The guide of each area gives measured numbers, and the tests hold both widths,
so that what each one costs is written down and not forgotten.

**Write every number with `REAL_C`.** A number written as `0.5` is a double and
one written as `0.5f` is a float, and either one is wrong in one of the two
builds. `0.5` in a 32 bit build quietly makes the arithmetic around it run in
64 bits and then throws the extra away; `0.5f` in a 64 bit build rounds to
seven digits before the wider arithmetic ever sees it. `REAL_C(0.5)` writes the
right one.

**Use the macros for mathematics.** `REAL_SQRT`, `REAL_SIN` and the rest stand
for `sqrtf` or `sqrt` as the build asks. They cost nothing, because the
compiler puts the right call in where they stand.

**Use the functions only where a function must be handed over.** A macro has no
address, thus none of them can be GIVEN to something that takes a function.
`real_sin`, `real_cos` and the rest are ordinary functions with addresses that
always agree with `real_t`. The `pmatrix` module needs them: before `real_t`
existed a caller could give it `sinf` directly, and that now builds without a
word and gives nonsense at 64 bits.

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
