#!/usr/bin/env python3
"""Hold the public surface of the library still.

THE LIBRARY IS IN A FEATURE FREEZE. From the release of 0.17.0 it takes fixes,
tests and documentation, and it does not take new public functions.

A freeze that is only written down is a freeze that ends the first time somebody
is in a hurry. This counts the public functions in every header and compares the
count against the one recorded below. A header that gains a function fails the
check, and the only way past it is to change the number here on purpose, which
is a thing a reviewer can see.

REMOVING a function fails it too, and that is deliberate. Taking a function away
is a change to what callers may rely on, and the freeze is exactly the time not
to do it by accident.

To run it:

    python3 scripts/check_freeze.py

To see what the counts are now, which is what to paste below when the freeze is
lifted for a release:

    python3 scripts/check_freeze.py --show
"""

import os
import re
import sys

REPOSITORY = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The public function count of every header at the freeze, taken at 0.17.0.
FROZEN = {
    "adaptive":      12,
    "binarysearch":  1,
    "bluestein":     8,
    "callback":      0,
    "cepstrum":      6,
    "changepoint":   12,
    "cmatrix":       35,
    "cnum":          18,
    "convolve":      5,
    "correlate":     6,
    "csd":           10,
    "cspline":       9,
    "curve":         8,
    "dcblock":       6,
    "dct":           4,
    "defs":          0,
    "delay":         4,
    "detrend":       5,
    "dwt":           7,
    "eigen":         5,
    "ekf":           20,
    "emd":           6,
    "farrow":        12,
    "fft":           11,
    "filtfilt":      5,
    "fir":           23,
    "generate":      14,
    "goertzel":      8,
    "hampel":        10,
    "hht":           3,
    "hilbert":       4,
    "iir":           22,
    "imf":           5,
    "interp":        6,
    "kalman":        18,
    "lattice":       12,
    "lstsq":         8,
    "matched":       7,
    "matrix":        36,
    "medfilt":       10,
    "movavg":        11,
    "peakdetect":    7,
    "pll":           12,
    "pmatrix":       11,
    "point2d":       0,
    "poly":          6,
    "propagate":     5,
    "psd":           11,
    "quantise":      10,
    "quaternion":    17,
    "real":          6,
    "resample":      13,
    "ringbuf":       9,
    "rls":           11,
    "savgol":        8,
    "slide":         14,
    "spectrogram":   5,
    "stats":         10,
    "stft":          15,
    "ukf":           20,
    "valleydetect":  1,
    "vector":        9,
    "vector2d":      8,
    "window":        11,
}


def public_functions(path):
    """Give the names of the public functions a header declares."""
    text = open(path).read()

    # The comments hold example calls and prose, thus they are taken out first.
    text = re.sub(r'//[^\n]*', '', text)

    flat = re.sub(r'\s+', ' ', text)
    module = os.path.basename(path)[:-2]

    found = set()

    for match in re.finditer(r'\b([a-z][a-z0-9_]*)\s*\([^;{]*\)\s*;', flat):
        name = match.group(1)

        if name.startswith(module + '_'):
            found.add(name)

    return found


def counts():
    """Give the count of public functions for every header, by module."""
    answer = {}

    for area in sorted(os.listdir(os.path.join(REPOSITORY, 'ffitt'))):
        folder = os.path.join(REPOSITORY, 'ffitt', area)

        if not os.path.isdir(folder):
            continue

        for name in sorted(os.listdir(folder)):
            if not name.endswith('.h'):
                continue

            path = os.path.join(folder, name)
            answer[name[:-2]] = len(public_functions(path))

    return answer


def main():
    now = counts()

    if '--show' in sys.argv:
        print('FROZEN = {')
        for module in sorted(now):
            print('    %-16s %d,' % ('"%s":' % module, now[module]))
        print('}')
        return 0

    if not FROZEN:
        print('The freeze holds no counts yet. Run with --show and paste the '
              'answer into FROZEN.')
        return 1

    faults = []

    for module in sorted(set(FROZEN) | set(now)):
        was = FROZEN.get(module)
        is_now = now.get(module)

        if was is None:
            faults.append('the module %s is new, and the freeze takes no new '
                          'modules' % module)
        elif is_now is None:
            faults.append('the module %s is gone' % module)
        elif is_now != was:
            faults.append('%s had %d public functions at the freeze and now '
                          'has %d' % (module, was, is_now))

    if faults:
        print('The freeze check found %d fault(s):\n' % len(faults))

        for fault in faults:
            print('  %s' % fault)

        print('\nThe library is in a feature freeze: it takes fixes, tests and')
        print('documentation, and not new public functions. If the freeze is')
        print('being lifted on purpose, run this with --show and paste the new')
        print('counts into FROZEN, so that the change is one a reviewer sees.')

        return 1

    print('The freeze check found no fault. %d modules, %d public functions.'
          % (len(now), sum(now.values())))

    return 0


if __name__ == '__main__':
    sys.exit(main())
