#!/usr/bin/env python3
"""Write the table of costs in docs/COSTS.md from the benchmark itself.

WHY THIS EXISTS.

The benchmark measured and printed and nothing kept what it printed. A reader
choosing this library had no way to know what an operation costs without
building the benchmark and running it, and the header of a module could say one
thing while the machine said another.

This builds the benchmark twice, once for a float and once for a double, runs
each, and writes the two times into docs/COSTS.md side by side. The table thus
comes from the benchmark and cannot drift away from it.

    python3 scripts/benchmark_table.py            write docs/COSTS.md
    python3 scripts/benchmark_table.py --check    give 1 if a row is missing

--check LOOKS AT THE ROWS AND NEVER AT THE TIMES. A time belongs to the machine
that measured it, thus asking a second machine for the same number would fail
on a repository that is perfectly in order. What it does ask is that the table
holds a row for every operation the benchmark measures, in the same order, so
that an operation added to the benchmark and forgotten here is caught. The
numbers are refreshed by a person on one machine, and the page says which.

THE BUILD IS THE PLAIN ONE, WITH NO OPTIMISATION ASKED FOR. That is what a
reader gets from `cmake -S . -B build`, thus it is the honest number to show
one. The cost tests in perf/cost ask for Release instead, because those hold
one way against another and an unoptimised build measures the compiler.

A time is written in microseconds, which is the unit the benchmark prints and
the unit that keeps three digits for every row from the fastest to the slowest.
"""

import os
import re
import subprocess
import sys
import tempfile

REPOSITORY = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
COSTS = os.path.join(REPOSITORY, "docs", "COSTS.md")

FIRST_MARK = "<!-- BENCHMARK TABLE BEGINS. scripts/benchmark_table.py writes it. -->"
LAST_MARK = "<!-- BENCHMARK TABLE ENDS. -->"

# MODULE OPERATION SIZE REPEATS BEST MEAN PER_SECOND WHAT IT IS
ROW = re.compile(r"^(\S+)\s+(.+?)\s+(\d+)\s+(\d+)\s+"
                 r"([\d.]+)\s+([\d.]+)\s+(\d+)\s\s+(\S.*)$")


def run(*command, **named):
    """Run a command and give what it wrote, or stop and say what broke."""
    finished = subprocess.run(command, capture_output=True, text=True, **named)

    if finished.returncode != 0:
        sys.stderr.write("%s gave %d\n%s\n%s\n"
                         % (" ".join(command), finished.returncode,
                            finished.stdout[-2000:], finished.stderr[-2000:]))
        sys.exit(1)

    return finished.stdout


def measure(wide):
    """Build the benchmark at one width, run it, and give what it measured.

    The answer is a list of (module, what it is, microseconds), in the order
    the benchmark ran them.
    """
    where = tempfile.mkdtemp(prefix="benchmark-table-")

    run("cmake", "-S", REPOSITORY, "-B", where, "-DBUILD_BENCHMARK=ON",
        "-DFFITT_REAL_64=" + ("ON" if wide else "OFF"))
    run("cmake", "--build", where, "-j", "4")

    measured = []

    for line in run(os.path.join(where, "benchmark")).splitlines():
        found = ROW.match(line)

        # A dash marks one size of a sweep. The benchmark measured it and
        # printed it, and the table leaves it out, because the table gives one
        # row for each operation and not one for each size.
        if (found is not None) and (found.group(8) != "-"):
            measured.append((found.group(1), found.group(8),
                             float(found.group(5))))

    if not measured:
        sys.stderr.write("The benchmark measured nothing at %d bits.\n"
                         % (64 if wide else 32))
        sys.exit(1)

    return measured


def table():
    """Give the table as one string of Markdown."""
    narrow = measure(False)
    wide = measure(True)

    if len(narrow) != len(wide):
        sys.stderr.write("The two widths measured %d and %d operations. They "
                         "must measure the same ones.\n"
                         % (len(narrow), len(wide)))
        sys.exit(1)

    lines = ["| Module | What it does | 32 bit | 64 bit |",
             "|---|---|---:|---:|"]

    for (module, what, at_32), (other, again, at_64) in zip(narrow, wide):
        if (module, what) != (other, again):
            sys.stderr.write("The two widths ran different operations: "
                             "%s %s against %s %s.\n"
                             % (module, what, other, again))
            sys.exit(1)

        lines.append("| `%s` | %s | %.2f us | %.2f us |"
                     % (module, what, at_32, at_64))

    return "\n".join(lines)


def written(new_table):
    """Give what docs/COSTS.md would hold with this table in it."""
    with open(COSTS) as handle:
        text = handle.read()

    if (FIRST_MARK not in text) or (LAST_MARK not in text):
        sys.stderr.write("docs/COSTS.md holds no place for the table. It needs "
                         "the two marks:\n  %s\n  %s\n"
                         % (FIRST_MARK, LAST_MARK))
        sys.exit(1)

    head = text.split(FIRST_MARK)[0]
    tail = text.split(LAST_MARK)[1]

    return "%s%s\n\n%s\n\n%s%s" % (head, FIRST_MARK, new_table, LAST_MARK,
                                   tail)


def rows_of(text):
    """Give the module and the words of each row of the table, times left out."""
    inside = text.split(FIRST_MARK)[-1].split(LAST_MARK)[0]
    found = []

    for line in inside.splitlines():
        parts = [part.strip() for part in line.split("|")]

        if (len(parts) == 6) and parts[1].startswith("`"):
            found.append((parts[1].strip("`"), parts[2]))

    return found


def main():
    checking = "--check" in sys.argv[1:]
    wanted = written(table())

    with open(COSTS) as handle:
        holding = handle.read()

    if checking:
        if rows_of(holding) == rows_of(wanted):
            print("The table of costs holds a row for every operation the "
                  "benchmark measures.")
            return 0

        missing = [row for row in rows_of(wanted) if row not in rows_of(holding)]
        extra = [row for row in rows_of(holding) if row not in rows_of(wanted)]

        for module, what in missing:
            print("The table has no row for: %s, %s" % (module, what))

        for module, what in extra:
            print("The table has a row the benchmark does not measure: "
                  "%s, %s" % (module, what))

        print("Make the table again:\n"
              "    python3 scripts/benchmark_table.py")
        return 1

    with open(COSTS, "w") as handle:
        handle.write(wanted)

    print("Wrote the table of costs into docs/COSTS.md.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
