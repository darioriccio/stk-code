#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Explicit macOS local-GPU test; writes all artifacts to --output-dir.

Expected visibility uses separated geometric fixtures and 60-digit independent
D24 quantization, not the production comparison expression. This test cannot
qualify optical presentation, spatial tracking or venue hardware.
"""
import argparse
from decimal import Decimal, localcontext, ROUND_HALF_UP
from pathlib import Path
import platform
import os
import subprocess

ROOT = Path(__file__).resolve().parents[3]


def d24(z, n, f):
    with localcontext() as context:
        context.prec = 60
        z, n, f = map(lambda v: Decimal(str(v)), (z, n, f))
        depth = f * (z - n) / (z * (f - n))
        code = (depth * 16777215).to_integral_value(rounding=ROUND_HALF_UP)
        return str(code / 16777215)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    if platform.system() != 'Darwin':
        parser.error('requires macOS OpenGL 4.1; no fallback or skipped-green result')
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=False)
    temporary = output / 'temporary'
    temporary.mkdir()
    environment = dict(os.environ, TMPDIR=str(temporary))
    rows = []
    for n, f in ((0.2, 1000), (0.1, 500), (0.5, 2000)):
        for z in (n * 1.5, 1, 10, 100, f * 0.5, f * 0.9):
            for factor, visible in ((0.99, 0), (1.0, 0), (1.01, 1)):
                rows.append((f'geometry-{len(rows)}', d24(z * factor, n, f), z, n, f, visible))
    rows += [('audit-wall', d24(99.95, 0.2, 1000), 100, 0.2, 1000, 0),
             ('audit-clear', d24(100.05, 0.2, 1000), 100, 0.2, 1000, 1)]
    for scene, marker, n, f in [('nan', 1, .2, 1000), ('inf', 1, .2, 1000),
                                (-.01, 1, .2, 1000), (1.01, 1, .2, 1000),
                                (1, .2, .2, 1000), (1, 1000, .2, 1000),
                                (1, 'nan', .2, 1000), (1, 1, 0, 1000)]:
        rows.append((f'invalid-{len(rows)}', scene, marker, n, f, 0))
    cases = output / 'cases.tsv'
    cases.write_text('\n'.join('\t'.join(map(str, row)) for row in rows) + '\n')
    binary = output / 'scintilla_depth_gpu'
    subprocess.run(['xcrun', 'clang++', '-std=c++11', '-O2', '-Wno-deprecated-declarations',
                    str(ROOT / 'tools/immersive/tests/scintilla_depth_gpu.cpp'),
                    '-framework', 'OpenGL', '-o', str(binary)], check=True, env=environment)
    subprocess.run([str(binary), str(ROOT / 'data/shaders/utils/immersive_scintilla.frag'), str(cases)], check=True, env=environment)


if __name__ == '__main__':
    main()
