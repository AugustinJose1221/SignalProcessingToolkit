#!/usr/bin/env python3
"""Examine that every example is wired in everywhere it must be.

An example holds its code inside a condition on RUN_EXAMPLE, and its name must
stand in run_example.h. A name that is NOT there counts as zero inside an #if,
and zero is the value of RUN_NONE. The example then gives a main function in
the default build, which is the one build that must give none.

That fault has come into this repository twice, and both times it passed every
other check: the build of each example chooses it by name, and the one example
whose name is missing is the one that compiles. Only the default build shows
it, and nothing was looking at the default build.

    python3 scripts/check_examples.py    give 1 if something is wrong

The check finds five faults:

- an example that names a RUN_ value which run_example.h does not define;
- an example source that holds no RUN_ condition at all;
- two examples given the same number;
- an example source that CMakeLists.txt does not build;
- an example that the workflow does not walk through.
"""

import os
import re
import sys

REPOSITORY = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EXAMPLES = os.path.join(REPOSITORY, "examples")
INDEX = os.path.join(EXAMPLES, "run_example.h")
BUILD = os.path.join(REPOSITORY, "CMakeLists.txt")
WORKFLOW = os.path.join(REPOSITORY, ".github", "workflows", "tests.yml")


def defined_names():
    """Give the names run_example.h defines, and the number of each."""
    found = {}
    with open(INDEX) as handle:
        for line in handle:
            match = re.match(r"\s*#define\s+(RUN_[A-Z_]+_EXAMPLE)\s+(\d+)",
                             line)
            if match:
                found[match.group(1)] = int(match.group(2))
    return found


def used_names():
    """Give the name each example source asks for."""
    found = {}
    for name in sorted(os.listdir(EXAMPLES)):
        if not name.endswith(".c"):
            continue
        path = os.path.join(EXAMPLES, name)
        with open(path) as handle:
            text = handle.read()
        match = re.search(r"RUN_EXAMPLE\s*==\s*(RUN_[A-Z_]+_EXAMPLE)", text)
        found[name] = match.group(1) if match else None
    return found


def faults():
    reported = []
    defined = defined_names()
    used = used_names()

    for source, name in sorted(used.items()):
        if name is None:
            reported.append(
                "examples/%s holds no condition on RUN_EXAMPLE" % source)
            continue
        if name not in defined:
            reported.append(
                "examples/%s asks for %s, which run_example.h does not define"
                % (source, name))

    seen = {}
    for name, number in sorted(defined.items()):
        if number in seen:
            reported.append("%s and %s are both given the number %d"
                            % (seen[number], name, number))
        seen[number] = name

    with open(BUILD) as handle:
        build_text = handle.read()

    for source in sorted(used):
        if ("examples/" + source) not in build_text:
            reported.append("CMakeLists.txt does not build examples/%s"
                            % source)

    if os.path.exists(WORKFLOW):
        with open(WORKFLOW) as handle:
            workflow_text = handle.read()

        for source, name in sorted(used.items()):
            if name is None:
                continue
            # The workflow walks through the short names, thus RUN_FFT_EXAMPLE
            # appears there as FFT.
            short = name[len("RUN_"):-len("_EXAMPLE")]
            if not re.search(r"\b%s\b" % short, workflow_text):
                reported.append(
                    "the workflow does not walk through %s, for examples/%s"
                    % (short, source))

    return reported


def main():
    reported = faults()

    if reported:
        print("The example check found %d fault(s):\n" % len(reported))
        for fault in reported:
            print("  " + fault)
        print("\nAn example whose name is missing from run_example.h counts as")
        print("zero, and zero is RUN_NONE, thus it gives a main function in")
        print("the default build.")
        return 1

    print("The example check found no fault.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
