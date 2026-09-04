# The consumer

A project outside this repository that builds against ffitt once it is
installed. It knows nothing of the tree it came from.

It is here because **install rules that nobody consumed are install rules that
do not work.** A library that builds, installs, and cannot be linked against
has not been tested, and nothing else in this repository would have noticed.

Two roads are walked, because callers use both:

```bash
cmake -S . -B build -DCMAKE_INSTALL_PREFIX=/tmp/prefix
cmake --build build
cmake --install build

# by CMake
cmake -S tests/consumer -B /tmp/consumer -DCMAKE_PREFIX_PATH=/tmp/prefix
cmake --build /tmp/consumer && /tmp/consumer/use

# and without it
PKG_CONFIG_PATH=/tmp/prefix/lib/pkgconfig \
  cc -std=c99 $(pkg-config --cflags ffitt) tests/consumer/use.c \
     $(pkg-config --libs ffitt) -o /tmp/use && /tmp/use
```

**The program checks and does not merely print.** It designs a low pass and
asks whether it passes what is below its cutoff and stops what is above. A
library that installed wrong fails that rather than printing something which
looks like an answer.

**It never says which width or which arithmetic it wants.** The package must
carry both: a program built against a 64 bit library must itself be built for
64 bits, and one built against a library holding its own arithmetic must not
reach for the system's. The build runs this against a plain install and against
one made with `FFITT_REAL_64` and `FFITT_NO_LIBM`, and the program prints the
width it really got.
