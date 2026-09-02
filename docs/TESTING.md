# Tests

The repository holds four kinds of test. Each asks a different question, and
none of them can answer another's.

| Kind | Where | The question it answers |
| --- | --- | --- |
| Unit | [tests](../tests) | Does this call give this answer? |
| Property | [tests/property](../tests/property) | Does this rule hold for EVERY input? |
| Conformation | [perf/conformation](../perf/conformation) | Does an outside authority agree? |
| Cost | [perf/cost](../perf/cost) | Do the headers still tell the truth about what things cost? |

## Unit tests

Ceedling and Unity examine known examples: a call, an answer, and a comparison.

```bash
ceedling test:all
```

The width is chosen with `FFITT_DEFINE`, which the build reads:

```bash
FFITT_DEFINE=FFITT_REAL_64 ceedling test:all
```

One of them needs the linker's help. `Test_heap_refusal` asks what every
allocator of the library does when the heap gives nothing, and there is no way
to ask for that from inside C. `project.yml` tells the linker to send `malloc`
and `calloc` through a pair of functions in that one test, which refuse on
command.

## Property based tests

Hypothesis examines rules that must hold for every input, and hunts for the
input that breaks one. A rule that passes one run has been given a few hundred
cases. See [tests/property/README.md](../tests/property/README.md).

```bash
python3 -m venv .venv && .venv/bin/pip install -r tests/property/requirements.txt
.venv/bin/pytest tests/property/
```

The width is chosen with `FFITT_REAL_64`, which the bindings read and pass to
the C build they make:

```bash
FFITT_REAL_64=1 .venv/bin/pytest tests/property/
```

## Conformation tests

These compare what this library works out against what the GNU Scientific
Library works out. They are the only place where anything here is held against
an outside authority.

GSL is a dependency of the TEST and never of the library. Thus they are off by
default:

```bash
cmake -S . -B build -DBUILD_CONFORMATION_TESTS=ON && cmake --build build
./build/conformation
```

Nothing here asks for exactly equal. The two libraries take different roads to
the same place and round differently, and the reference of GSL is held in
single precision even where this library is built for double.

## Cost tests

The headers of this library say what things cost: that a moving mean does not
follow its window, that a resampler costs the OUTPUT rate and not the input
rate, that a transform beats the plain way above a size they name. Those
sentences decide which function a caller picks, and nothing held them to it.
The benchmark measures and prints, thus a claim could stop being true and the
table would still be printed and the build would still be green.

These measure the same things and say whether the claim still stands. The exit
code says it too.

```bash
cmake -S . -B build -DBUILD_COST_TESTS=ON && cmake --build build
./build/cost
```

What is tested is the SHAPE of a cost and never the number, because a number
belongs to the machine that measured it. See
[perf/cost/README.md](../perf/cost/README.md), which also holds the one finding
these tests have made that is still open.

## What the build runs

The workflow runs all four at both widths, with one exception it names: the
cost tests run at 32 bits only, because two of their twelve claims break at 64
bits for a reason that is not the library's.

It also holds the coverage: the build fails below 98 percent of lines, below
100 percent of functions and below 90 percent of branches. The assertions are
left out of the branch count, and
[CONTRIBUTING.md](../CONTRIBUTING.md) says why.
