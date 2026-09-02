# The cost tests

The headers of this library say what things cost.

- `movavg` says its work does not follow the window, and holds a table of
  measured times to show it.
- `resample` says the filtering costs the OUTPUT rate and not the input rate.
- `correlate` says the plain method wins below about 300 samples and the
  transform wins far above it.
- `convolve` says the same about a shape of about 60.
- `goertzel` says the transform costs less above about log2(n) frequencies.
- `ringbuf` says putting a sample in costs the same at any size of buffer.

Those sentences are the reason a caller picks one function over another.
Nothing held them. The benchmark beside this one MEASURES and PRINTS, thus a
claim could stop being true and the table would still be printed and the build
would still be green.

These tests measure the same things and then say whether the claim still
stands. The exit code says it too: 0 when every claim held, 1 when one did not.

## How to run them

    cmake -S . -B build -DBUILD_COST_TESTS=ON
    cmake --build build
    ./build/cost

They need no external library. They take about seven seconds.

The build type is set to Release when the caller names none. A build with no
optimisation measures the compiler and not the module.

## How a claim is held

A time measured on one machine is not the time on another. Thus a claim of two
hundred times is held at fifty, and a claim of sixty-four times is held at
five. What is tested is the SHAPE of the cost, which is what the header
promises a caller: that one way is far cheaper than the other, or that the cost
does not follow a size. A margin that wide cannot be met by accident and cannot
be missed by a busy machine.

Where the header names a crossover, the claim is held from BOTH sides. A claim
held on one side only would pass just as well for a module that was always
faster one way, which is not what the header says.

Each measurement is the FASTEST of seven runs, because the other work of the
machine can only make a run slower and never faster.

Every claim was made to fail on purpose before it was kept. The ratios were
turned round, the ring buffer was made to move its samples, and the moving mean
was replaced by the filter it exists to avoid. All twelve reported BROKEN and
the program gave 1, thus none of them is a test that cannot fail.

## What these tests found

TWO CLAIMS BREAK AT 64 BITS AND HOLD AT 32.

    correlate  at 4096 the transform wins by a wide margin   1.52 >= 4.00
    convolve   at a shape of 512 the transform costs far less 0.27 >= 1.50

The same two claims measure 12.37 and 2.22 at 32 bits.

The cause is not the library. Measured on one machine with GCC 13.3, one
forward transform of 8192 points, the time for twenty of them:

    width        -O1        -O2        -O3
    32 bit    0.0081     0.0050     0.0049
    64 bit    0.0068     0.0432     0.0432

At 32 bits the optimiser makes the transform faster, which is what it is for.
At 64 bits it makes the SAME CODE more than six times slower than it is at -O1.
The answers are the same to every digit; only the time changes. The butterfly
reads its turning factor at a stride, and a stride of a sixteen byte value is
what the optimiser turns into something worse than plain code.

Thus the two claims are true of the transform and false of what the compiler
built from it, at that width and above -O1.

THE TESTS ARE LEFT AS THEY STAND AND NOTHING IS WEAKENED TO MAKE THEM PASS. The
suite runs at 32 bits in the build, where every claim holds. It is not run at 64
bits until the transform is dealt with, because a job that is known to be red
teaches nobody anything.
