"""Tests for the documentation check.

A check that cannot find a fault has no value. These tests give the check a
header with a known fault and examine that it finds the fault.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import api_doc  # noqa: E402


HEADER_WITH_COMMENTS = """#ifndef EXAMPLE_H
#define EXAMPLE_H

// The largest order that the module takes.
#define EXAMPLE_MAX_ORDER   16

// A thing with a size.
typedef struct{
    uint32_t size;
}example_t;

// Give a new thing of the given size.
example_t example_alloc(uint32_t size);

// Release the memory of the thing.
void example_free(example_t* thing);

#endif//EXAMPLE_H
"""

HEADER_WITHOUT_A_COMMENT = """#ifndef EXAMPLE_H
#define EXAMPLE_H

// Give a new thing of the given size.
example_t example_alloc(uint32_t size);

void example_free(example_t* thing);

#endif//EXAMPLE_H
"""


def read(tmp_path, text):
    path = tmp_path / "example.h"
    path.write_text(text)
    return api_doc.read_header(str(path))


def test_the_check_reads_every_function(tmp_path):
    types, macros, functions = read(tmp_path, HEADER_WITH_COMMENTS)
    names = [name for name, _, _ in functions]
    assert names == ["example_alloc", "example_free"]


def test_the_check_reads_the_comment_of_each_function(tmp_path):
    _, _, functions = read(tmp_path, HEADER_WITH_COMMENTS)
    comments = {name: comment for name, _, comment in functions}
    assert comments["example_alloc"] == ["Give a new thing of the given size."]
    assert comments["example_free"] == ["Release the memory of the thing."]


def test_the_check_finds_a_function_with_no_comment(tmp_path):
    _, _, functions = read(tmp_path, HEADER_WITHOUT_A_COMMENT)
    comments = {name: comment for name, _, comment in functions}
    assert comments["example_alloc"] != []
    assert comments["example_free"] == []


def test_the_check_reads_the_types_and_the_macros(tmp_path):
    types, macros, _ = read(tmp_path, HEADER_WITH_COMMENTS)
    assert [name for name, _, _ in types] == ["example_t"]
    assert [name for name, _, _ in macros] == ["EXAMPLE_MAX_ORDER"]
    assert types[0][2] == ["A thing with a size."]
    assert macros[0][2] == ["The largest order that the module takes."]


def test_a_comment_of_another_function_does_not_count(tmp_path):
    # A blank line stands between the comment and the second function, thus
    # the comment belongs to the first function only.
    text = """#ifndef EXAMPLE_H
#define EXAMPLE_H

// Give a new thing.
example_t example_alloc(uint32_t size);

example_t example_copy(example_t* thing);

#endif//EXAMPLE_H
"""
    _, _, functions = read(tmp_path, text)
    comments = {name: comment for name, _, comment in functions}
    assert comments["example_copy"] == []


def test_every_function_of_the_repository_has_a_comment():
    assert api_doc.find_functions_without_a_comment() == []


def test_the_documentation_of_the_repository_is_current():
    assert api_doc.main(["api_doc.py", "--check"]) == 0
