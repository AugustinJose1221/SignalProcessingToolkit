"""Examine that the Python types agree with the C types.

The property tests read the structures of the library directly. If a structure
of the library changes, the Python type must change as well. These tests
compare the size of each structure and the position of each element. Thus a
change in the library cannot make the property tests give a wrong result
without a signal.
"""

import ctypes
import os
import subprocess
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sptk  # noqa: E402

PROBE = r"""
#include <stddef.h>
#include <stdio.h>
#include <matrix/matrix.h>
#include <vector/vector.h>
#include <cspline/cspline.h>
#include <kalman/kalman.h>

int main(void)
{
    printf("Matrix %zu %zu %zu %zu\n", sizeof(matrix_t),
           offsetof(matrix_t, n), offsetof(matrix_t, elem),
           offsetof(matrix_t, dynamic_alloc));
    printf("Vector %zu %zu %zu\n", sizeof(vector_t),
           offsetof(vector_t, data), offsetof(vector_t, dynamic_alloc));
    printf("CSpline %zu %zu %zu\n", sizeof(cspline_t),
           offsetof(cspline_t, d), offsetof(cspline_t, dynamic_alloc));
    printf("CSplineMempool %zu %zu\n", sizeof(cspline_mempool_t),
           offsetof(cspline_mempool_t, dynamic_alloc));
    printf("Kalman %zu %zu %zu %zu %zu\n", sizeof(kalman_t),
           offsetof(kalman_t, _x), offsetof(kalman_t, k),
           offsetof(kalman_t, scratch), offsetof(kalman_t, dynamic_alloc));
    return 0;
}
"""


@pytest.fixture(scope="module")
def probe_output(tmp_path_factory):
    directory = tmp_path_factory.mktemp("probe")
    source = directory / "probe.c"
    source.write_text(PROBE)
    binary = directory / "probe"

    build = subprocess.run(
        ["gcc", "-std=c99", "-I", sptk.REPOSITORY, str(source), "-o", str(binary)],
        capture_output=True, text=True)
    assert build.returncode == 0, build.stderr

    result = subprocess.run([str(binary)], capture_output=True, text=True)
    assert result.returncode == 0, result.stderr

    values = {}
    for line in result.stdout.strip().splitlines():
        parts = line.split()
        values[parts[0]] = [int(part) for part in parts[1:]]
    return values


def test_matrix_layout(probe_output):
    size, offset_n, offset_elem, offset_flag = probe_output["Matrix"]
    assert ctypes.sizeof(sptk.Matrix) == size
    assert sptk.Matrix.n.offset == offset_n
    assert sptk.Matrix.elem.offset == offset_elem
    assert sptk.Matrix.dynamic_alloc.offset == offset_flag


def test_vector_layout(probe_output):
    size, offset_data, offset_flag = probe_output["Vector"]
    assert ctypes.sizeof(sptk.Vector) == size
    assert sptk.Vector.data.offset == offset_data
    assert sptk.Vector.dynamic_alloc.offset == offset_flag


def test_cspline_layout(probe_output):
    size, offset_d, offset_flag = probe_output["CSpline"]
    assert ctypes.sizeof(sptk.CSpline) == size
    assert sptk.CSpline.d.offset == offset_d
    assert sptk.CSpline.dynamic_alloc.offset == offset_flag


def test_cspline_mempool_layout(probe_output):
    size, offset_flag = probe_output["CSplineMempool"]
    assert ctypes.sizeof(sptk.CSplineMempool) == size
    assert sptk.CSplineMempool.dynamic_alloc.offset == offset_flag


def test_kalman_layout(probe_output):
    size, offset_prev, offset_gain, offset_scratch, offset_flag = probe_output["Kalman"]
    assert ctypes.sizeof(sptk.Kalman) == size
    assert getattr(sptk.Kalman, "_x").offset == offset_prev
    assert sptk.Kalman.k.offset == offset_gain
    assert sptk.Kalman.scratch.offset == offset_scratch
    assert sptk.Kalman.dynamic_alloc.offset == offset_flag


def test_the_library_answers(lib):
    matrix = lib.matrix_create_unit_matrix(3)
    assert lib.matrix_is_unit(ctypes.byref(matrix))
    lib.matrix_free(ctypes.byref(matrix))
