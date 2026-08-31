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
import ffitt  # noqa: E402

PROBE = r"""
#include <stddef.h>
#include <stdio.h>
#include <ffitt/linalg/matrix.h>
#include <ffitt/linalg/vector.h>
#include <ffitt/interpolate/cspline.h>
#include <ffitt/estimate/kalman.h>
#include <ffitt/linalg/cnum.h>
#include <ffitt/linalg/quaternion.h>
#include <ffitt/transform/fft.h>
#include <ffitt/transform/bluestein.h>
#include <ffitt/transform/stft.h>
#include <ffitt/filter/iir.h>
#include <ffitt/filter/fir.h>
#include <ffitt/filter/rls.h>
#include <ffitt/filter/lattice.h>
#include <ffitt/transform/csd.h>
#include <ffitt/detect/matched.h>
#include <ffitt/detect/changepoint.h>
#include <ffitt/util/generate.h>
#include <ffitt/util/quantise.h>
#include <ffitt/util/peakdetect.h>

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
    printf("Iir %zu %zu %zu\n", sizeof(iir_t),
           offsetof(iir_t, coefficient), offsetof(iir_t, dynamic_alloc));
    printf("Fir %zu %zu %zu\n", sizeof(fir_t),
           offsetof(fir_t, coefficient), offsetof(fir_t, dynamic_alloc));
    printf("Rls %zu %zu %zu %zu\n", sizeof(rls_t),
           offsetof(rls_t, coefficient), offsetof(rls_t, length),
           offsetof(rls_t, dynamic_alloc));
    printf("Lattice %zu %zu %zu %zu\n", sizeof(lattice_t),
           offsetof(lattice_t, weight), offsetof(lattice_t, stages),
           offsetof(lattice_t, dynamic_alloc));
    printf("Generate %zu %zu %zu %zu\n", sizeof(generate_t),
           offsetof(generate_t, seed), offsetof(generate_t, pink),
           offsetof(generate_t, designed));
    printf("Quantise %zu %zu %zu\n", sizeof(quantise_t),
           offsetof(quantise_t, seed), offsetof(quantise_t, designed));
    printf("Csd %zu %zu %zu %zu\n", sizeof(csd_t),
           offsetof(csd_t, window), offsetof(csd_t, fft),
           offsetof(csd_t, dynamic_alloc));
    printf("Matched %zu %zu %zu %zu\n", sizeof(matched_t),
           offsetof(matched_t, length), offsetof(matched_t, root_energy),
           offsetof(matched_t, designed));
    printf("Changepoint %zu %zu %zu %zu\n", sizeof(changepoint_t),
           offsetof(changepoint_t, high), offsetof(changepoint_t, since_high),
           offsetof(changepoint_t, designed));
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
    command = ["gcc", "-std=c99", "-I", ffitt.REPOSITORY]
    if ffitt.REAL_64:
        command.append("-DFFITT_REAL_64")
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
    assert ctypes.sizeof(ffitt.Matrix) == size
    assert ffitt.Matrix.n.offset == offset_n
    assert ffitt.Matrix.elem.offset == offset_elem
    assert ffitt.Matrix.dynamic_alloc.offset == offset_flag


def test_vector_layout(probe_output):
    size, offset_data, offset_flag = probe_output["Vector"]
    assert ctypes.sizeof(ffitt.Vector) == size
    assert ffitt.Vector.data.offset == offset_data
    assert ffitt.Vector.dynamic_alloc.offset == offset_flag


def test_cspline_layout(probe_output):
    size, offset_d, offset_flag = probe_output["CSpline"]
    assert ctypes.sizeof(ffitt.CSpline) == size
    assert ffitt.CSpline.d.offset == offset_d
    assert ffitt.CSpline.dynamic_alloc.offset == offset_flag


def test_cspline_mempool_layout(probe_output):
    size, offset_flag = probe_output["CSplineMempool"]
    assert ctypes.sizeof(ffitt.CSplineMempool) == size
    assert ffitt.CSplineMempool.dynamic_alloc.offset == offset_flag


def test_kalman_layout(probe_output):
    size, offset_prev, offset_gain, offset_scratch, offset_flag = probe_output["Kalman"]
    assert ctypes.sizeof(ffitt.Kalman) == size
    assert getattr(ffitt.Kalman, "_x").offset == offset_prev
    assert ffitt.Kalman.k.offset == offset_gain
    assert ffitt.Kalman.scratch.offset == offset_scratch
    assert ffitt.Kalman.dynamic_alloc.offset == offset_flag


def test_cnum_layout(probe_output):
    size, offset_imaginary = probe_output["Cnum"]
    assert ctypes.sizeof(ffitt.Cnum) == size
    assert ffitt.Cnum.im.offset == offset_imaginary


def test_fft_layout(probe_output):
    size, offset_twiddle, offset_reverse, offset_flag = probe_output["Fft"]
    assert ctypes.sizeof(ffitt.Fft) == size
    assert ffitt.Fft.twiddle.offset == offset_twiddle
    assert ffitt.Fft.reverse.offset == offset_reverse
    assert ffitt.Fft.dynamic_alloc.offset == offset_flag


def test_bluestein_layout(probe_output):
    size, offset_fft, offset_chirp, offset_flag = probe_output["Bluestein"]
    assert ctypes.sizeof(ffitt.Bluestein) == size
    assert ffitt.Bluestein.fft.offset == offset_fft
    assert ffitt.Bluestein.chirp.offset == offset_chirp
    assert ffitt.Bluestein.dynamic_alloc.offset == offset_flag


def test_stft_layout(probe_output):
    size, offset_window, offset_fft, offset_flag = probe_output["Stft"]
    assert ctypes.sizeof(ffitt.Stft) == size
    assert ffitt.Stft.window.offset == offset_window
    assert ffitt.Stft.fft.offset == offset_fft
    assert ffitt.Stft.dynamic_alloc.offset == offset_flag


def test_iir_layout(probe_output):
    size, offset_coefficient, offset_flag = probe_output["Iir"]
    assert ctypes.sizeof(ffitt.Iir) == size
    assert ffitt.Iir.coefficient.offset == offset_coefficient
    assert ffitt.Iir.dynamic_alloc.offset == offset_flag


def test_fir_layout(probe_output):
    size, offset_coefficient, offset_flag = probe_output["Fir"]
    assert ctypes.sizeof(ffitt.Fir) == size
    assert ffitt.Fir.coefficient.offset == offset_coefficient
    assert ffitt.Fir.dynamic_alloc.offset == offset_flag


def test_rls_layout(probe_output):
    size, offset_coefficient, offset_length, offset_flag = probe_output["Rls"]
    assert ctypes.sizeof(ffitt.Rls) == size
    assert ffitt.Rls.coefficient.offset == offset_coefficient
    assert ffitt.Rls.length.offset == offset_length
    assert ffitt.Rls.dynamic_alloc.offset == offset_flag


def test_lattice_layout(probe_output):
    size, offset_weight, offset_stages, offset_flag = probe_output["Lattice"]
    assert ctypes.sizeof(ffitt.Lattice) == size
    assert ffitt.Lattice.weight.offset == offset_weight
    assert ffitt.Lattice.stages.offset == offset_stages
    assert ffitt.Lattice.dynamic_alloc.offset == offset_flag


def test_generate_layout(probe_output):
    size, offset_seed, offset_pink, offset_flag = probe_output["Generate"]
    assert ctypes.sizeof(ffitt.Generate) == size
    assert ffitt.Generate.seed.offset == offset_seed
    assert ffitt.Generate.pink.offset == offset_pink
    assert ffitt.Generate.designed.offset == offset_flag


def test_csd_layout(probe_output):
    size, offset_window, offset_fft, offset_flag = probe_output["Csd"]
    assert ctypes.sizeof(ffitt.Csd) == size
    assert ffitt.Csd.window.offset == offset_window
    assert ffitt.Csd.fft.offset == offset_fft
    assert ffitt.Csd.dynamic_alloc.offset == offset_flag


def test_matched_layout(probe_output):
    size, offset_length, offset_energy, offset_flag = probe_output["Matched"]
    assert ctypes.sizeof(ffitt.Matched) == size
    assert ffitt.Matched.length.offset == offset_length
    assert ffitt.Matched.root_energy.offset == offset_energy
    assert ffitt.Matched.designed.offset == offset_flag


def test_changepoint_layout(probe_output):
    size, offset_high, offset_since, offset_flag = probe_output["Changepoint"]
    assert ctypes.sizeof(ffitt.Changepoint) == size
    assert ffitt.Changepoint.high.offset == offset_high
    assert ffitt.Changepoint.since_high.offset == offset_since
    assert ffitt.Changepoint.designed.offset == offset_flag


def test_quantise_layout(probe_output):
    size, offset_seed, offset_flag = probe_output["Quantise"]
    assert ctypes.sizeof(ffitt.Quantise) == size
    assert ffitt.Quantise.seed.offset == offset_seed
    assert ffitt.Quantise.designed.offset == offset_flag


def test_quaternion_layout(probe_output):
    size, offset_x, offset_z = probe_output["Quaternion"]
    assert ctypes.sizeof(ffitt.Quaternion) == size
    assert ffitt.Quaternion.x.offset == offset_x
    assert ffitt.Quaternion.z.offset == offset_z


def test_peakdetect_options_layout(probe_output):
    size, offset_width, offset_distance = probe_output["PeakdetectOptions"]
    assert ctypes.sizeof(ffitt.PeakdetectOptions) == size
    assert ffitt.PeakdetectOptions.minimum_width.offset == offset_width
    assert ffitt.PeakdetectOptions.minimum_distance.offset == offset_distance


def test_the_library_answers(lib):
    matrix = lib.matrix_create_unit_matrix(3)
    assert lib.matrix_is_unit(ctypes.byref(matrix))
    lib.matrix_free(ctypes.byref(matrix))
