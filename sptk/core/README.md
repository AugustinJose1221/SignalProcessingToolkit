# Core

Three headers that hold no module of their own. They give the types and the
macros that the other areas share, thus they hold no `.c` file and no function.

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
