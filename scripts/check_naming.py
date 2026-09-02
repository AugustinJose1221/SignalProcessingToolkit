#!/usr/bin/env python3
"""Examine the names in the sources against the scheme of the Linux kernel.

The scheme asks for:

- Names of functions, parameters and variables in lower case, with an
  underscore between the words. No capital letter inside a name.
- A name of a type that ends with _t, in lower case.
- A name of a macro in upper case.
- A function of a module that starts with the name of its module. Thus the
  matrix module gives matrix_add and not add_matrix.
- A function that only its own file uses is static.

Give the paths to examine on the command line, or give none to examine the
whole repository. The program gives 1 if it finds a fault.
"""

import os
import re
import sys

REPOSITORY = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Unity calls these two functions before and after each test. The names come
# from Unity, thus this repository cannot change them.
#
# The two that begin with __wrap_ come from the LINKER. Asked to send a call
# through a function of the test, it looks for exactly that name and no other.
# Test_heap_refusal uses it to make the heap refuse, which is the only way to
# reach the guard that every allocator carries. The names are the linker's and
# not this library's, thus the scheme cannot ask them to change.
ALLOWED_NAMES = {"setUp", "tearDown", "main",
                 "__wrap_malloc", "__wrap_calloc"}

# The library lies under this directory. Each file there holds one module, and
# the name of the file is the name of the module.
LIBRARY_DIRECTORY = "ffitt"

# The headers that hold no module of their own. They give a type or a macro,
# and no function starts with their name.
HEADERS_WITHOUT_A_MODULE = {"callback", "defs", "point2d"}

SKIPPED_DIRECTORIES = ("build", ".git", "vendor")

LOWER_NAME = re.compile(r"^[a-z][a-z0-9_]*$")
TYPE_NAME = re.compile(r"^[a-z][a-z0-9_]*_t$")
MACRO_NAME = re.compile(r"^[A-Z_][A-Z0-9_]*$")

# A prototype in a header, for example:  void matrix_free(matrix_t* matrix);
#
# The part before the name must hold no line break, and it must end with a
# space or a star. Thus a call such as matrix_free(&matrix); does not look
# like a declaration.
KEYWORDS = r"(?!return\b|if\b|while\b|for\b|switch\b|else\b|do\b|sizeof\b|typedef\b)"

PROTOTYPE = re.compile(
    r"^[ \t]*" + KEYWORDS +
    r"(?P<lead>[A-Za-z_][A-Za-z0-9_ \t\*]*[ \t\*])"
    r"(?P<name>[A-Za-z_]\w*)[ \t]*\((?P<params>[^;{]*?)\)[ \t]*;",
    re.MULTILINE)

# A definition in a source file. A function is defined at the start of a line.
DEFINITION = re.compile(
    r"^(?P<static>static[ \t]+)?" + KEYWORDS +
    r"(?P<lead>[A-Za-z_][A-Za-z0-9_ \t\*]*[ \t\*])"
    r"(?P<name>[A-Za-z_]\w*)[ \t]*\((?P<params>[^;{]*?)\)[ \t]*\r?\n?[ \t]*\{",
    re.MULTILINE)

TYPEDEF_END = re.compile(r"^\s*\}\s*(?P<name>\w+)\s*;", re.MULTILINE)
DEFINE = re.compile(r"^\s*#\s*define\s+(?P<name>\w+)", re.MULTILINE)

# A parameter that holds a pointer to a function, for example int (*func)(...)
FUNCTION_POINTER_PARAM = re.compile(r"\(\s*\*\s*(?P<name>\w+)\s*\)")


def strip_comments_and_strings(text):
    """Give the text without its comments and without the text in quotes."""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", "", text)
    text = re.sub(r'"(\\.|[^"\\])*"', '""', text)
    return text


def source_files(paths):
    for path in paths:
        if os.path.isfile(path):
            yield path
            continue
        for root, directories, names in os.walk(path):
            directories[:] = [d for d in directories
                              if d not in SKIPPED_DIRECTORIES and not d.startswith(".")]
            for name in sorted(names):
                if name.endswith((".c", ".h")):
                    yield os.path.join(root, name)


def module_prefix_for(path):
    """Give the prefix that every function of this file must carry.

    The name of the file is the name of the module, thus matrix.c and matrix.h
    both give the prefix matrix_. A file outside the library, such as a test or
    a benchmark, carries no prefix.
    """
    relative = os.path.relpath(os.path.abspath(path), REPOSITORY)

    if not relative.startswith(LIBRARY_DIRECTORY + os.sep):
        return None

    module = os.path.splitext(os.path.basename(relative))[0]

    if module in HEADERS_WITHOUT_A_MODULE:
        return None

    return module + "_"


def parameter_names(parameters):
    """Give the name of each parameter of a declaration."""
    names = []
    for part in parameters.split(","):
        part = part.strip()
        if not part or part in ("void", "..."):
            continue
        pointer = FUNCTION_POINTER_PARAM.search(part)
        if pointer:
            names.append(pointer.group("name"))
            continue
        part = re.sub(r"\[[^\]]*\]", "", part).strip()
        words = re.findall(r"[A-Za-z_]\w*", part)
        if len(words) < 2:
            # A parameter with a type and no name.
            continue
        names.append(words[-1])
    return names


# A NAME THAT BEGINS WITH TWO UNDERSCORES BELONGS TO THE IMPLEMENTATION.
#
# C keeps every such name for the compiler and its own headers, thus a library
# that uses one is standing where it was not invited. Fourteen headers guarded
# themselves with names of that shape, __DEF_H__ among them, while every other
# header of the library used the clean form. This finds them.
RESERVED_NAME = re.compile(r"^__")


def check_file(path, faults, prefix=None):
    """Examine one file. The prefix is the name of its module, or None."""
    def fault(name, message):
        with open(path, encoding="utf-8", errors="replace") as handle:
            for number, line in enumerate(handle, start=1):
                if re.search(r"\b%s\b" % re.escape(name), line):
                    faults.append("%s:%d: %s" % (path, number, message))
                    return
        faults.append("%s: %s" % (path, message))

    with open(path, encoding="utf-8", errors="replace") as handle:
        raw = handle.read()
    text = strip_comments_and_strings(raw)

    for match in TYPEDEF_END.finditer(text):
        name = match.group("name")
        if not TYPE_NAME.match(name):
            fault(name, "the type '%s' must be in lower case and end with _t" % name)

    for match in DEFINE.finditer(text):
        name = match.group("name")
        if not MACRO_NAME.match(name):
            fault(name, "the macro '%s' must be in upper case" % name)
        if RESERVED_NAME.match(name):
            fault(name, "the macro '%s' begins with two underscores, which "
                        "names a thing that belongs to the compiler and not "
                        "to this library" % name)

    if path.endswith(".h"):
        declarations = list(PROTOTYPE.finditer(text))
    else:
        declarations = list(DEFINITION.finditer(text))

    for match in declarations:
        name = match.group("name")
        if name in ALLOWED_NAMES:
            continue
        if not LOWER_NAME.match(name):
            fault(name, "the function '%s' must be in lower case, with an "
                        "underscore between the words" % name)
            continue

        is_static = bool(match.groupdict().get("static"))
        if prefix and path.endswith(".h") and not name.startswith(prefix):
            fault(name, "the function '%s' must start with '%s', which is the "
                        "name of its module" % (name, prefix))
        if prefix and path.endswith(".c") and not is_static and not name.startswith(prefix):
            fault(name, "the function '%s' is not static, thus it must start "
                        "with '%s'" % (name, prefix))

        for parameter in parameter_names(match.group("params")):
            if not LOWER_NAME.match(parameter):
                fault(parameter, "the parameter '%s' of '%s' must be in lower "
                                 "case" % (parameter, name))


def main(argv):
    paths = argv[1:] or [REPOSITORY]
    faults = []
    for path in source_files(paths):
        check_file(path, faults, module_prefix_for(path))

    if faults:
        print("The naming check found %d fault(s):\n" % len(faults))
        for line in faults:
            print("  " + line)
        print("\nThe scheme asks for names in lower case with an underscore "
              "between the words,")
        print("and for a function name that starts with the name of its module.")
        return 1

    print("The naming check found no fault.")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
