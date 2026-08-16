"""Tests for the naming check.

A check that cannot find a fault has no value. These tests give the check a
file with a known fault and examine that it finds the fault. They also give it
correct code and examine that it stays quiet.
"""

import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import check_naming  # noqa: E402


def run_check(tmp_path, name, text, prefix=None):
    path = tmp_path / name
    path.write_text(text)
    faults = []
    check_naming.check_file(str(path), faults, prefix)
    return faults


GOOD_HEADER = """
#ifndef MATRIX_H
#define MATRIX_H

typedef struct{
    uint32_t m;
}matrix_t;

#define MATRIX_MAX_ORDER    16

matrix_t matrix_alloc(uint32_t m, uint32_t n);
void matrix_printf(matrix_t* matrix, int (*func)(const char*, ...));
void matrix_free(matrix_t* matrix);

#endif
"""


def test_correct_code_gives_no_fault(tmp_path):
    assert run_check(tmp_path, "matrix.h", GOOD_HEADER, "matrix_") == []


def test_a_function_with_a_capital_letter_is_a_fault(tmp_path):
    faults = run_check(tmp_path, "matrix.h",
                       "void matrixAddElement(matrix_t* matrix);\n", "matrix_")
    assert len(faults) == 1
    assert "matrixAddElement" in faults[0]
    assert "lower case" in faults[0]


def test_a_function_without_the_name_of_its_module_is_a_fault(tmp_path):
    faults = run_check(tmp_path, "matrix.h",
                       "void add_element(matrix_t* matrix);\n", "matrix_")
    assert len(faults) == 1
    assert "must start with 'matrix_'" in faults[0]


def test_a_parameter_with_a_capital_letter_is_a_fault(tmp_path):
    faults = run_check(tmp_path, "matrix.h",
                       "void matrix_add(matrix_t* rowsA, matrix_t* b);\n", "matrix_")
    assert len(faults) == 1
    assert "rowsA" in faults[0]
    assert "parameter" in faults[0]


def test_a_type_that_does_not_end_with_the_letter_t_is_a_fault(tmp_path):
    faults = run_check(tmp_path, "matrix.h",
                       "typedef struct{\n    int m;\n}MatrixType;\n", "matrix_")
    assert len(faults) == 1
    assert "MatrixType" in faults[0]


def test_a_macro_in_lower_case_is_a_fault(tmp_path):
    faults = run_check(tmp_path, "matrix.h", "#define maxOrder 16\n", "matrix_")
    assert len(faults) == 1
    assert "maxOrder" in faults[0]
    assert "upper case" in faults[0]


def test_a_function_that_is_not_static_needs_the_name_of_its_module(tmp_path):
    faults = run_check(tmp_path, "matrix.c",
                       "void helper(matrix_t* a)\n{\n    return;\n}\n", "matrix_")
    assert len(faults) == 1
    assert "not static" in faults[0]


def test_a_static_function_may_have_any_lower_case_name(tmp_path):
    assert run_check(tmp_path, "matrix.c",
                     "static void helper(matrix_t* a)\n{\n    return;\n}\n",
                     "matrix_") == []


def test_a_call_is_not_read_as_a_declaration(tmp_path):
    # A call inside a function must not look like a declaration to the check.
    text = """void matrix_free(matrix_t* matrix)
{
    matrix_copy(&a, &b);
    ASSERT(matrix != NULL);
    return matrix_alloc(1, 1);
}
"""
    assert run_check(tmp_path, "matrix.c", text, "matrix_") == []


def test_the_names_that_unity_asks_for_are_allowed(tmp_path):
    text = "void setUp(void)\n{\n}\n\nvoid tearDown(void)\n{\n}\n"
    assert run_check(tmp_path, "Test_matrix.c", text, None) == []


def test_the_check_reads_the_whole_repository_without_a_fault():
    assert check_naming.main(["check_naming.py"]) == 0
