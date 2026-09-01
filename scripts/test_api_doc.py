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


def test_the_program_writes_one_file_for_each_module_and_an_index():
    documents = api_doc.build_documents()

    assert api_doc.INDEX_PATH in documents

    for name, path, title in api_doc.MODULES:
        expected = os.path.join(api_doc.MODULE_DIRECTORY, "%s.md" % name)
        assert expected in documents, "no file for the module %s" % name

    # The index and one file for each module.
    assert len(documents) == len(api_doc.MODULES) + 1


def test_the_index_points_to_the_file_of_each_module():
    index = api_doc.build_index()

    for name, path, title in api_doc.MODULES:
        assert "(api/%s.md)" % name in index, "the index does not point to %s" % name


def test_the_file_of_a_module_holds_its_functions_and_no_others():
    document = api_doc.build_module_document("goertzel", "ffitt/transform/goertzel.h",
                                             "Detection of one frequency")

    assert "### `goertzel_init`" in document
    assert "### `goertzel_magnitude`" in document
    # A function of another module must not stand in this file.
    assert "matrix_alloc" not in document


def test_the_check_finds_a_file_that_belongs_to_no_module(tmp_path, monkeypatch):
    # Put a file into the directory that no module writes. The check must see
    # it, so that a module that goes away leaves no old file behind.
    directory = tmp_path / "api"
    directory.mkdir()
    (directory / "gone.md").write_text("# gone\n")

    monkeypatch.setattr(api_doc, "MODULE_DIRECTORY", str(directory))

    extra = api_doc.find_files_that_belong_to_no_module({})

    assert len(extra) == 1
    assert extra[0].endswith("gone.md")


def test_the_check_is_quiet_when_every_file_belongs_to_a_module(tmp_path, monkeypatch):
    directory = tmp_path / "api"
    directory.mkdir()
    known = directory / "matrix.md"
    known.write_text("# matrix\n")

    monkeypatch.setattr(api_doc, "MODULE_DIRECTORY", str(directory))

    assert api_doc.find_files_that_belong_to_no_module({str(known): "# matrix\n"}) == []


def test_every_area_of_the_library_holds_a_guide():
    assert api_doc.find_areas_without_a_guide() == []


def test_the_check_finds_an_area_with_no_guide(tmp_path, monkeypatch):
    # Point the program at an empty tree. Every area must then be reported,
    # thus the check would see a guide that goes away.
    monkeypatch.setattr(api_doc, "REPOSITORY", str(tmp_path))

    missing = api_doc.find_areas_without_a_guide()

    assert len(missing) == len(api_doc.AREAS)
    assert "ffitt/transform/README.md" in missing[0]


def test_every_module_belongs_to_an_area():
    for name, path, title in api_doc.MODULES:
        # The three headers of the core area hold no module of their own, thus
        # two of them stand in the area list and defs stands nowhere.
        if name == "defs":
            continue
        assert api_doc.area_of(name) is not None, "%s belongs to no area" % name
