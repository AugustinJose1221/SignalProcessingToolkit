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
#include <sptk/linalg/matrix.h>
#include <sptk/linalg/vector.h>
#include <sptk/interpolate/cspline.h>
#include <sptk/estimate/kalman.h>
#include <sptk/linalg/cnum.h>
#include <sptk/linalg/quaternion.h>
#include <sptk/transform/fft.h>
#include <sptk/transform/bluestein.h>
#include <sptk/transform/stft.h>
#include <sptk/util/peakdetect.h>

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
    printf("Cnum %zu %zu\n", sizeof(cnum_t), offsetof(cnum_t, im));
    printf("Fft %zu %zu %zu %zu\n", sizeof(fft_t),
           offsetof(fft_t, twiddle), offsetof(fft_t, reverse),
           offsetof(fft_t, dynamic_alloc));
    printf("Bluestein %zu %zu %zu %zu\n", sizeof(bluestein_t),
           offsetof(bluestein_t, fft), offsetof(bluestein_t, chirp),
           offsetof(bluestein_t, dynamic_alloc));
    printf("Stft %zu %zu %zu %zu\n", sizeof(stft_t),
           offsetof(stft_t, window), offsetof(stft_t, fft),
           offsetof(stft_t, dynamic_alloc));
    printf("Quaternion %zu %zu %zu\n", sizeof(quaternion_t),
           offsetof(quaternion_t, x), offsetof(quaternion_t, z));
    printf("PeakdetectOptions %zu %zu %zu\n", sizeof(peakdetect_options_t),
           offsetof(peakdetect_options_t, minimum_width),
           offsetof(peakdetect_options_t, minimum_distance));
    return 0;
}
"""


@pytest.fixture(scope="module")
def probe_output(tmp_path_factory):
    directory = tmp_path_factory.mktemp("probe")
    source = directory / "probe.c"
    source.write_text(PROBE)
    binary = directory / "probe"

    # THE PROBE MUST BE BUILT AT THE SAME WIDTH AS THE LIBRARY.
    #
    # Every structure that holds a real_t by value changes size with the width.
    # A probe built at 32 bits and compared against Python types made at 64
    # would report a fault where there is none, and worse, would report
    # agreement for the structures that happen to match.
    command = ["gcc", "-std=c99", "-I", sptk.REPOSITORY]
    if sptk.REAL_64:
        command.append("-DSPTK_REAL_64")
    command += [str(source), "-o", str(binary)]

    build = subprocess.run(command, capture_output=True, text=True)
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


def test_cnum_layout(probe_output):
    size, offset_imaginary = probe_output["Cnum"]
    assert ctypes.sizeof(sptk.Cnum) == size
    assert sptk.Cnum.im.offset == offset_imaginary


def test_fft_layout(probe_output):
    size, offset_twiddle, offset_reverse, offset_flag = probe_output["Fft"]
    assert ctypes.sizeof(sptk.Fft) == size
    assert sptk.Fft.twiddle.offset == offset_twiddle
    assert sptk.Fft.reverse.offset == offset_reverse
    assert sptk.Fft.dynamic_alloc.offset == offset_flag


def test_bluestein_layout(probe_output):
    size, offset_fft, offset_chirp, offset_flag = probe_output["Bluestein"]
    assert ctypes.sizeof(sptk.Bluestein) == size
    assert sptk.Bluestein.fft.offset == offset_fft
    assert sptk.Bluestein.chirp.offset == offset_chirp
    assert sptk.Bluestein.dynamic_alloc.offset == offset_flag


def test_stft_layout(probe_output):
    size, offset_window, offset_fft, offset_flag = probe_output["Stft"]
    assert ctypes.sizeof(sptk.Stft) == size
    assert sptk.Stft.window.offset == offset_window
    assert sptk.Stft.fft.offset == offset_fft
    assert sptk.Stft.dynamic_alloc.offset == offset_flag


def test_quaternion_layout(probe_output):
    size, offset_x, offset_z = probe_output["Quaternion"]
    assert ctypes.sizeof(sptk.Quaternion) == size
    assert sptk.Quaternion.x.offset == offset_x
    assert sptk.Quaternion.z.offset == offset_z


def test_peakdetect_options_layout(probe_output):
    size, offset_width, offset_distance = probe_output["PeakdetectOptions"]
    assert ctypes.sizeof(sptk.PeakdetectOptions) == size
    assert sptk.PeakdetectOptions.minimum_width.offset == offset_width
    assert sptk.PeakdetectOptions.minimum_distance.offset == offset_distance


def test_the_library_answers(lib):
    matrix = lib.matrix_create_unit_matrix(3)
    assert lib.matrix_is_unit(ctypes.byref(matrix))
    lib.matrix_free(ctypes.byref(matrix))
