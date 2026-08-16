#!/usr/bin/env python3
"""Make the API documentation from the headers, and examine it.

The headers are the one place that holds the description of each function. This
program reads them and writes docs/API.md. Thus the documentation and the code
cannot say two different things.

    python3 scripts/api_doc.py            write docs/API.md
    python3 scripts/api_doc.py --check    examine, and give 1 if something is wrong

The check finds two faults:

- a function that a header declares and that has no comment above it;
- a docs/API.md that does not agree with the headers.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_naming  # noqa: E402

REPOSITORY = check_naming.REPOSITORY
OUTPUT_PATH = os.path.join(REPOSITORY, "docs", "API.md")

# The order of the modules in the documentation. A reader meets the simple
# modules first and the modules that build on them later.
MODULES = [
    ("matrix", "matrix/matrix.h", "Matrices of float values"),
    ("cnum", "cnum/cnum.h", "Complex numbers"),
    ("cmatrix", "cmatrix/cmatrix.h", "Matrices of complex numbers"),
    ("pmatrix", "pmatrix/pmatrix.h", "Matrices with a parameter"),
    ("fft", "fft/fft.h", "The fast Fourier transform"),
    ("hilbert", "hilbert/hilbert.h", "The Hilbert transform"),
    ("hht", "hht/hht.h", "The Hilbert-Huang transform"),
    ("fir", "fir/fir.h", "Filters with a finite impulse response"),
    ("iir", "iir/iir.h", "Filters with an infinite impulse response"),
    ("vector", "vector/vector.h", "Vectors of float values"),
    ("vector2d", "vector2d/vector2d.h", "Vectors with two values"),
    ("cspline", "cspline/cspline.h", "Cubic splines"),
    ("imf", "imf/imf.h", "Intrinsic mode functions"),
    ("emd", "emd/emd.h", "Empirical mode decomposition"),
    ("kalman", "kalman/kalman.h", "The Kalman filter"),
    ("binarysearch", "utils/binarysearch/binarysearch.h", "Binary search"),
    ("peakdetect", "utils/peakdetect/peakdetect.h", "Peak detection"),
    ("valleydetect", "utils/valleydetect/valleydetect.h", "Valley detection"),
    ("point2d", "point2d/point2d.h", "A point on a plane"),
    ("callback", "common/callback.h", "The print callback"),
]

COMMENT = re.compile(r"^\s*//\s?(.*)$")
TYPEDEF_START = re.compile(r"^\s*typedef\s+struct")
TYPEDEF_END = re.compile(r"^\s*\}\s*(?P<name>\w+)\s*;")
DEFINE = re.compile(r"^\s*#\s*define\s+(?P<name>[A-Z_][A-Z0-9_]*)")
GUARD = re.compile(r"^\s*#\s*ifndef\s+(?P<name>\w+)")


def comment_above(lines, index):
    """Give the comment that stands directly above the given line."""
    collected = []
    position = index - 1

    while position >= 0:
        match = COMMENT.match(lines[position])
        if not match:
            break
        collected.append(match.group(1).rstrip())
        position -= 1

    collected.reverse()
    while collected and collected[0] == "":
        collected.pop(0)
    while collected and collected[-1] == "":
        collected.pop()

    return collected


def line_of_offset(text, offset):
    return text.count("\n", 0, offset)


def read_header(path):
    """Give the declarations of one header with the comment of each."""
    with open(path, encoding="utf-8") as handle:
        raw = handle.read()
    lines = raw.splitlines()

    # The naming check strips the comments before it looks for a declaration.
    # Here the position in the file must stay the same, thus the strings only
    # become empty and the comments stay.
    stripped = re.sub(r'"(\\.|[^"\\])*"', '""', raw)

    types = []
    macros = []
    functions = []

    # The first #ifndef of a header is the include guard. Its #define is not a
    # part of the interface of the module, thus the documentation leaves it out.
    guard = None
    for line in lines:
        match = GUARD.match(line)
        if match:
            guard = match.group("name")
            break

    for number, line in enumerate(lines):
        match = DEFINE.match(line)
        if match and match.group("name") != guard:
            macros.append((match.group("name"), line.strip(), comment_above(lines, number)))

    start = None
    for number, line in enumerate(lines):
        if TYPEDEF_START.match(line):
            start = number
        match = TYPEDEF_END.match(line)
        if match and start is not None:
            body = lines[start:number + 1]
            types.append((match.group("name"), body, comment_above(lines, start)))
            start = None

    for match in check_naming.PROTOTYPE.finditer(stripped):
        number = line_of_offset(stripped, match.start())
        # The declaration may stand over more than one line.
        end = line_of_offset(stripped, match.end())
        declaration = " ".join(part.strip() for part in lines[number:end + 1])
        functions.append((match.group("name"), declaration.strip(),
                          comment_above(lines, number)))

    return types, macros, functions


def build_document():
    parts = []
    parts.append("# API reference\n")
    parts.append("This file comes from the comments in the headers. Do not change it by "
                 "hand.\nTo make it again, give:\n")
    parts.append("```bash\npython3 scripts/api_doc.py\n```\n")
    parts.append("## Modules\n")
    for name, path, title in MODULES:
        parts.append("- [`%s`](#%s) &mdash; %s" % (name, name, title))
    parts.append("")

    for name, path, title in MODULES:
        full_path = os.path.join(REPOSITORY, path)
        if not os.path.exists(full_path):
            continue
        types, macros, functions = read_header(full_path)

        parts.append("---\n")
        parts.append("## %s\n" % name)
        parts.append("%s. Declared in `%s`.\n" % (title, path))

        if macros:
            parts.append("### Macros\n")
            for macro_name, line, comment in macros:
                parts.append("#### `%s`\n" % macro_name)
                parts.append("```c\n%s\n```\n" % line)
                if comment:
                    parts.append("\n".join(comment) + "\n")

        if types:
            parts.append("### Types\n")
            for type_name, body, comment in types:
                parts.append("#### `%s`\n" % type_name)
                if comment:
                    parts.append("\n".join(comment) + "\n")
                parts.append("```c\n%s\n```\n" % "\n".join(body))

        if functions:
            parts.append("### Functions\n")
            for function_name, declaration, comment in functions:
                parts.append("#### `%s`\n" % function_name)
                parts.append("```c\n%s\n```\n" % declaration)
                if comment:
                    parts.append("\n".join(comment) + "\n")

    return "\n".join(parts).rstrip() + "\n"


def find_functions_without_a_comment():
    faults = []
    for name, path, title in MODULES:
        full_path = os.path.join(REPOSITORY, path)
        if not os.path.exists(full_path):
            continue
        _, _, functions = read_header(full_path)
        for function_name, declaration, comment in functions:
            if not comment:
                faults.append("%s: the function '%s' has no comment above it"
                              % (path, function_name))
    return faults


def main(argv):
    check = "--check" in argv[1:]
    document = build_document()

    faults = find_functions_without_a_comment()

    if check:
        if not os.path.exists(OUTPUT_PATH):
            faults.append("docs/API.md is not there. Run scripts/api_doc.py.")
        else:
            with open(OUTPUT_PATH, encoding="utf-8") as handle:
                if handle.read() != document:
                    faults.append("docs/API.md does not agree with the headers. "
                                  "Run scripts/api_doc.py.")

        if faults:
            print("The documentation check found %d fault(s):\n" % len(faults))
            for line in faults:
                print("  " + line)
            return 1

        print("The documentation check found no fault.")
        return 0

    if faults:
        print("Warning: %d function(s) have no comment:" % len(faults))
        for line in faults:
            print("  " + line)

    os.makedirs(os.path.dirname(OUTPUT_PATH), exist_ok=True)
    with open(OUTPUT_PATH, "w", encoding="utf-8") as handle:
        handle.write(document)
    print("Wrote %s" % os.path.relpath(OUTPUT_PATH, REPOSITORY))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
