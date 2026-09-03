#!/usr/bin/env python3
"""Make the API documentation from the headers, and examine it.

The headers are the one place that holds the description of each function. This
program reads them and writes docs/API.md. Thus the documentation and the code
cannot say two different things.

    python3 scripts/api_doc.py            write the files under docs/
    python3 scripts/api_doc.py --check    examine, and give 1 if something is wrong

The program writes one file for each module in docs/api/, and an index in
docs/API.md. One file for each module keeps each file short, and a reader who
works with one module opens one file only.

The check finds four faults:

- a function that a header declares and that has no comment above it;
- a file under docs/ that does not agree with the headers;
- a file in docs/api/ that belongs to no module any more;
- an area of the library that holds no guide.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_naming  # noqa: E402

REPOSITORY = check_naming.REPOSITORY
INDEX_PATH = os.path.join(REPOSITORY, "docs", "API.md")
MODULE_DIRECTORY = os.path.join(REPOSITORY, "docs", "api")

# The order of the modules in the documentation. A reader meets the simple
# modules first and the modules that build on them later.
# The library lies under this directory.
LIBRARY_DIRECTORY = "ffitt"

# The headers that hold no module of their own. They give a type or a macro,
# and no document is made for them.
HEADERS_WITHOUT_A_MODULE = {"callback", "defs"}

MODULES = [
    ("matrix", "ffitt/linalg/matrix.h", "Matrices of float values"),
    ("cnum", "ffitt/linalg/cnum.h", "Complex numbers"),
    ("cmatrix", "ffitt/linalg/cmatrix.h", "Matrices of complex numbers"),
    ("pmatrix", "ffitt/linalg/pmatrix.h", "Matrices with a parameter"),
    ("poly", "ffitt/linalg/poly.h", "Polynomials, and where they cross nothing"),
    ("eigen", "ffitt/linalg/eigen.h", "The directions a matrix stretches"),
    ("quaternion", "ffitt/linalg/quaternion.h", "Which way something points"),
    ("lstsq", "ffitt/linalg/lstsq.h", "Fitting a curve through readings"),
    ("fft", "ffitt/transform/fft.h", "The fast Fourier transform"),
    ("bluestein", "ffitt/transform/bluestein.h", "A transform of any size"),
    ("window", "ffitt/transform/window.h", "Windows for a transform"),
    ("correlate", "ffitt/transform/correlate.h", "How alike two signals are"),
    ("convolve", "ffitt/transform/convolve.h", "Sliding one signal along another"),
    ("psd", "ffitt/transform/psd.h", "Power at each frequency"),
    ("csd", "ffitt/transform/csd.h", "What two signals have in common"),
    ("stft", "ffitt/transform/stft.h", "The transform in short pieces"),
    ("spectrogram", "ffitt/transform/spectrogram.h", "What the short pieces mean"),
    ("hilbert", "ffitt/transform/hilbert.h", "The Hilbert transform"),
    ("hht", "ffitt/transform/hht.h", "The Hilbert-Huang transform"),
    ("fir", "ffitt/filter/fir.h", "Filters with a finite impulse response"),
    ("iir", "ffitt/filter/iir.h", "Filters with an infinite impulse response"),
    ("vector", "ffitt/linalg/vector.h", "Vectors of float values"),
    ("vector2d", "ffitt/linalg/vector2d.h", "Vectors with two values"),
    ("cspline", "ffitt/interpolate/cspline.h", "Cubic splines"),
    ("interp", "ffitt/interpolate/interp.h", "Reading between the points of a table"),
    ("imf", "ffitt/decompose/imf.h", "Intrinsic mode functions"),
    ("emd", "ffitt/decompose/emd.h", "Empirical mode decomposition"),
    ("pll", "ffitt/estimate/pll.h", "Following a tone that will not stay still"),
    ("propagate", "ffitt/estimate/propagate.h", "Carrying a state forward through a rate of change"),
    ("kalman", "ffitt/estimate/kalman.h", "The Kalman filter"),
    ("ekf", "ffitt/estimate/ekf.h", "The extended Kalman filter"),
    ("ukf", "ffitt/estimate/ukf.h", "The unscented Kalman filter"),
    ("slide", "ffitt/transform/slide.h", "One frequency, answered at every sample"),
    ("goertzel", "ffitt/transform/goertzel.h", "Detection of one frequency"),
    ("cepstrum", "ffitt/transform/cepstrum.h", "Finding what repeats in a spectrum"),
    ("dct", "ffitt/transform/dct.h", "Turning a signal into cosines"),
    ("dwt", "ffitt/transform/dwt.h", "The discrete wavelet transform"),
    ("savgol", "ffitt/filter/savgol.h", "The filter of Savitzky and Golay"),
    ("movavg", "ffitt/filter/movavg.h", "The mean of the last samples"),
    ("medfilt", "ffitt/filter/medfilt.h", "The median of the last samples"),
    ("dcblock", "ffitt/filter/dcblock.h", "Taking the level of a signal away"),
    ("detrend", "ffitt/filter/detrend.h", "Taking the level and the drift out of a block"),
    ("hampel", "ffitt/filter/hampel.h", "Replacing only the samples that are wrong"),
    ("lattice", "ffitt/filter/lattice.h", "A filter built as a ladder of stages"),
    ("rls", "ffitt/filter/rls.h", "A filter that solves least squares at every sample"),
    ("adaptive", "ffitt/filter/adaptive.h", "A filter that finds its own coefficients"),
    ("resample", "ffitt/filter/resample.h", "Changing the rate of a signal"),
    ("filtfilt", "ffitt/filter/filtfilt.h", "Filtering with no delay"),
    ("farrow", "ffitt/filter/farrow.h", "Delaying by a part of a sample"),
    ("matched", "ffitt/detect/matched.h", "Looking for a known shape"),
    ("delay", "ffitt/detect/delay.h", "How far one reading stands behind another"),
    ("changepoint", "ffitt/detect/changepoint.h", "Saying when a reading has changed"),
    ("generate", "ffitt/util/generate.h", "Making the signals to test with"),
    ("curve", "ffitt/util/curve.h", "The shapes a peak can have"),
    ("quantise", "ffitt/util/quantise.h", "Putting a signal into steps"),
    ("stats", "ffitt/util/stats.h", "Measures of a list of samples"),
    ("binarysearch", "ffitt/util/binarysearch.h", "Binary search"),
    ("peakdetect", "ffitt/util/peakdetect.h", "Peak detection"),
    ("valleydetect", "ffitt/util/valleydetect.h", "Valley detection"),
    ("real", "ffitt/core/real.h", "The one type that holds every number"),
    ("ringbuf", "ffitt/core/ringbuf.h", "A buffer of the last samples"),
    ("point2d", "ffitt/core/point2d.h", "A point on a plane"),
    ("callback", "ffitt/core/callback.h", "The print callback"),
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


# The areas of the library. Each one is a directory under ffitt/, and each one
# holds a README.md that says how its modules work.
AREAS = [
    ("transform", "Transforms", ["fft", "bluestein", "window", "psd", "csd",
                                 "stft", "spectrogram", "correlate",
                                 "convolve", "goertzel", "slide", "hilbert",
                                 "hht", "dwt", "dct", "cepstrum"]),
    ("filter", "Filters", ["fir", "iir", "savgol", "movavg", "medfilt",
                           "dcblock", "detrend", "hampel", "adaptive", "rls", "lattice",
                           "resample", "filtfilt", "farrow"]),
    ("estimate", "Estimation", ["kalman", "ekf", "ukf", "propagate", "pll"]),
    ("decompose", "Decomposition", ["emd", "imf"]),
    ("interpolate", "Interpolation", ["cspline", "interp"]),
    ("linalg", "Linear algebra", ["matrix", "cmatrix", "pmatrix", "cnum",
                                  "quaternion", "eigen", "poly", "lstsq",
                                  "vector",
                                  "vector2d"]),
    ("detect", "Detection", ["matched", "delay", "changepoint"]),
    ("util", "Utilities", ["generate", "curve", "quantise", "stats", "binarysearch",
                           "peakdetect", "valleydetect"]),
    ("core", "Core", ["real", "ringbuf", "point2d", "callback"]),
]

GENERATED_NOTE = ("This file comes from the comments in the headers. Do not change it by "
                  "hand.\nTo make it again, give:\n\n```bash\npython3 scripts/api_doc.py\n```\n")


def area_of(module):
    """Give the area that a module belongs to, or None."""
    for area, title, names in AREAS:
        if module in names:
            return area
    return None


def module_title(name):
    for module_name, path, title in MODULES:
        if module_name == name:
            return title
    return name


def build_index():
    """Give the text of docs/API.md, which points to the file of each module."""
    known = {name for name, path, title in MODULES}
    parts = ["# API reference\n", GENERATED_NOTE]
    parts.append("Each module has its own file. Open the file of the module that you "
                 "work with.\n")
    parts.append("Each area also holds a guide that says how its modules work and "
                 "which one to\nreach for. The guide explains the method; the file "
                 "of a module gives the exact\nname and shape of every function.\n")

    for area, title, names in AREAS:
        parts.append("## %s\n" % title)
        parts.append("[How the %s modules work](../ffitt/%s/README.md)\n" % (area, area))
        parts.append("| Module | What it holds |")
        parts.append("| --- | --- |")
        for name in names:
            if name in known:
                parts.append("| [`%s`](api/%s.md) | %s |" % (name, name, module_title(name)))
        parts.append("")

    listed = {name for area, title, names in AREAS for name in names}
    rest = [name for name, path, title in MODULES if name not in listed]
    if rest:
        parts.append("## Other\n")
        parts.append("| Module | What it holds |")
        parts.append("| --- | --- |")
        for name in rest:
            parts.append("| [`%s`](api/%s.md) | %s |" % (name, name, module_title(name)))
        parts.append("")

    return "\n".join(parts).rstrip() + "\n"


def build_module_document(name, path, title):
    """Give the text of the file of one module."""
    full_path = os.path.join(REPOSITORY, path)
    types, macros, functions = read_header(full_path)

    parts = ["# %s\n" % name, GENERATED_NOTE]
    parts.append("%s. Declared in `%s`.\n" % (title, path))
    area = area_of(name)
    if area:
        parts.append("[Back to the index](../API.md) | "
                     "[How the %s modules work](../../ffitt/%s/README.md)\n"
                     % (area, area))
    else:
        parts.append("[Back to the index](../API.md)\n")

    if macros:
        parts.append("## Macros\n")
        for macro_name, line, comment in macros:
            parts.append("### `%s`\n" % macro_name)
            parts.append("```c\n%s\n```\n" % line)
            if comment:
                parts.append("\n".join(comment) + "\n")

    if types:
        parts.append("## Types\n")
        for type_name, body, comment in types:
            parts.append("### `%s`\n" % type_name)
            if comment:
                parts.append("\n".join(comment) + "\n")
            parts.append("```c\n%s\n```\n" % "\n".join(body))

    if functions:
        parts.append("## Functions\n")
        for function_name, declaration, comment in functions:
            parts.append("### `%s`\n" % function_name)
            parts.append("```c\n%s\n```\n" % declaration)
            if comment:
                parts.append("\n".join(comment) + "\n")

    return "\n".join(parts).rstrip() + "\n"


def build_documents():
    """Give a dictionary of the path of each file and the text that belongs in it."""
    documents = {INDEX_PATH: build_index()}

    for name, path, title in MODULES:
        if not os.path.exists(os.path.join(REPOSITORY, path)):
            continue
        documents[os.path.join(MODULE_DIRECTORY, "%s.md" % name)] = \
            build_module_document(name, path, title)

    return documents


def find_areas_without_a_guide():
    """Give the areas whose directory holds no README.md."""
    missing = []

    for area, title, names in AREAS:
        guide = os.path.join(REPOSITORY, "ffitt", area, "README.md")
        if not os.path.exists(guide):
            missing.append("ffitt/%s/README.md is not there. Each area needs a "
                           "guide that says how its modules work." % area)

    return missing


def find_modules_with_no_document():
    """Name every module of the library that MODULES does not list.

    WHY THIS IS EXAMINED. MODULES is written by hand, and a module left out of
    it is simply skipped: no file is made for it, no fault is reported, and the
    check passes. The slide module was added and documented nowhere, and this
    check said all was well. The other direction - a file for a module that
    went away - was already caught; this is the half that was missing.
    """
    faults = []
    listed = {name for name, path, title in MODULES}

    for area, title, names in AREAS:
        directory = os.path.join(REPOSITORY, LIBRARY_DIRECTORY, area)

        if not os.path.isdir(directory):
            continue

        for name in sorted(os.listdir(directory)):
            if not name.endswith(".h"):
                continue

            module = name[:-2]

            if (module not in listed) and (module not in HEADERS_WITHOUT_A_MODULE):
                faults.append("the module %s/%s has no place in MODULES, thus "
                              "no documentation is made for it."
                              % (area, module))

    return faults


def find_files_that_belong_to_no_module(documents):
    """Give the files in docs/api that no module writes any more."""
    if not os.path.isdir(MODULE_DIRECTORY):
        return []

    expected = set(documents)
    extra = []

    for name in sorted(os.listdir(MODULE_DIRECTORY)):
        path = os.path.join(MODULE_DIRECTORY, name)
        if os.path.isfile(path) and path not in expected:
            extra.append(path)

    return extra


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


# A link in a Markdown file of this repository, and where it points.
MARKDOWN_LINK = re.compile(r"\[[^\]]*\]\(([^)#]+?)(?:#[^)]*)?\)")


def find_links_that_point_nowhere():
    """Name every link of a Markdown file that points at a file not there.

    WHY THIS IS EXAMINED. README.md was split, and the parts that moved out of
    it left a pointer behind. A pointer that names a file that is not there is
    worse than no pointer: it reads as an answer and gives none. Nothing caught
    such a link, thus a file could be moved or renamed and every pointer to it
    left broken with the build still green.

    A link that names something outside this repository is left alone; only a
    path is followed.
    """
    faults = []

    for root, directories, names in os.walk(REPOSITORY):
        directories[:] = [d for d in directories
                          if d not in ("build", "vendor", "node_modules")
                          and not d.startswith(".")]

        for name in sorted(names):
            if not name.endswith(".md"):
                continue

            path = os.path.join(root, name)

            with open(path, encoding="utf-8", errors="replace") as handle:
                text = handle.read()

            for match in MARKDOWN_LINK.finditer(text):
                target = match.group(1).strip()

                if target.startswith(("http://", "https://", "mailto:")):
                    continue

                if not os.path.exists(os.path.join(root, target)):
                    faults.append("%s points at %s, which is not there."
                                  % (os.path.relpath(path, REPOSITORY), target))

    return faults


def main(argv):
    check = "--check" in argv[1:]
    documents = build_documents()

    faults = find_functions_without_a_comment()

    faults.extend(find_areas_without_a_guide())

    faults.extend(find_modules_with_no_document())

    faults.extend(find_links_that_point_nowhere())

    for path in find_files_that_belong_to_no_module(documents):
        faults.append("%s belongs to no module any more. Remove it."
                      % os.path.relpath(path, REPOSITORY))

    if check:
        for path, text in sorted(documents.items()):
            relative = os.path.relpath(path, REPOSITORY)
            if not os.path.exists(path):
                faults.append("%s is not there. Run scripts/api_doc.py." % relative)
                continue
            with open(path, encoding="utf-8") as handle:
                if handle.read() != text:
                    faults.append("%s does not agree with the headers. "
                                  "Run scripts/api_doc.py." % relative)

        if faults:
            print("The documentation check found %d fault(s):\n" % len(faults))
            for line in faults:
                print("  " + line)
            return 1

        print("The documentation check found no fault.")
        return 0

    if faults:
        print("Warning:")
        for line in faults:
            print("  " + line)

    os.makedirs(MODULE_DIRECTORY, exist_ok=True)
    for path, text in sorted(documents.items()):
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(text)

    print("Wrote %d file(s) under docs/" % len(documents))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
