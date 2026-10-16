#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Source/math checks, with binary32 rounding after every scalar operation.

Evaluates the actual C++/GLSL comparison expressions against independently
encoded D24 samples. No compiler, native executable, shader or graphics context.
This cannot qualify driver arithmetic, rasterization or physical occlusion.
"""
import ast
from decimal import Decimal, ROUND_CEILING, ROUND_FLOOR, ROUND_HALF_UP, localcontext
import math
from pathlib import Path
import random
import re
import struct
import unittest

ROOT = Path(__file__).resolve().parents[3]
CPP_PATH = ROOT / 'src/modes/immersive_relay/spatial/scintilla_depth_visibility.hpp'
SHADER_PATH = ROOT / 'data/shaders/utils/immersive_scintilla.frag'


def binary32(value):
    return struct.unpack('f', struct.pack('f', value))[0]


def normalize(expression):
    return re.sub(r'(?<=\d)f\b', '', expression)


def scalar(node, scope):
    """Restricted arithmetic evaluator; every operation rounds to binary32."""
    if isinstance(node, ast.Expression):
        return scalar(node.body, scope)
    if isinstance(node, ast.Constant):
        return binary32(node.value)
    if isinstance(node, ast.Name):
        return scope[node.id]
    if isinstance(node, ast.BinOp):
        a, b = scalar(node.left, scope), scalar(node.right, scope)
        if isinstance(node.op, ast.Add):
            value = a + b
        elif isinstance(node.op, ast.Sub):
            value = a - b
        elif isinstance(node.op, ast.Mult):
            value = a * b
        elif isinstance(node.op, ast.Div):
            value = a / b
        else:
            raise AssertionError(ast.dump(node))
        return binary32(value)
    if isinstance(node, ast.Compare) and len(node.ops) == 1 and isinstance(node.ops[0], ast.Gt):
        return scalar(node.left, scope) > scalar(node.comparators[0], scope)
    raise AssertionError(ast.dump(node))


class SourceRule:
    def __init__(self, source, function):
        self.body = re.search(r'bool ' + function + r'\(float scene,float z,float n,float f\)\s*\{(.*?)\n\}', source, re.S)[1]
        self.expressions = [(name, normalize(expression)) for name, expression in
                            re.findall(r'(?:const )?float (\w+)=(.*?);', self.body)]
        self.result = normalize(re.search(r'return (markerGapLower>sceneGapUpper);', self.body)[1])
        self.nodes = [(name, ast.parse(expression, mode='eval')) for name, expression in self.expressions]
        self.result_node = ast.parse(self.result, mode='eval')

    def visible(self, scene, z, n, f):
        scene, z, n, f = map(binary32, (scene, z, n, f))
        # Input validity is source-checked separately, not simulated GPU code.
        if not all(map(math.isfinite, (scene, z, n, f))) or not (0 <= scene <= 1 and 0 < n < z < f):
            return False
        scope = dict(scene=scene, z=z, n=n, f=f)
        for name, node in self.nodes:
            scope[name] = scalar(node, scope)
        return scalar(self.result_node, scope)


def captured_depth(z, n, f, rounding=ROUND_HALF_UP):
    """Independent 60-digit projection and explicit UNORM24 conversion."""
    with localcontext() as context:
        context.prec = 60
        z, n, f = map(Decimal.from_float, (float(z), float(n), float(f)))
        depth = f * (z - n) / (z * (f - n))
        depth = max(Decimal(0), min(Decimal(1), depth))
        code = (depth * 16777215).to_integral_value(rounding=rounding)
        return binary32(float(code / 16777215))


class DepthVisibilityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.cpp = SourceRule(CPP_PATH.read_text(), 'visible')
        cls.shader = SourceRule(SHADER_PATH.read_text(), 'scintillaDepthVisible')

    def assertBoth(self, scene, z, n, f, expected):
        values = scene, z, n, f
        self.assertEqual(self.cpp.visible(*values), expected, values)
        self.assertEqual(self.shader.visible(*values), expected, values)

    def test_actual_rules_and_engine_shader_wiring_match(self):
        self.assertEqual(self.cpp.expressions, self.shader.expressions)
        self.assertEqual(len(self.cpp.expressions), 5)
        self.assertEqual(self.cpp.result, self.shader.result)
        guard = 'if(!(scene>=0.0 && scene<=1.0 && n>0.0 && z>n && z<f))return false;'
        self.assertIn(guard, normalize(self.cpp.body))
        self.assertIn(guard, self.shader.body)
        for value in ('scene', 'z', 'n', 'f'):
            self.assertIn('!std::isfinite(' + value + ')', self.cpp.body)
        self.assertIn('any(isnan(vec4(scene,z,n,f)))', self.shader.body)
        self.assertIn('any(isinf(vec4(scene,z,n,f)))', self.shader.body)
        engine = (ROOT / 'src/modes/immersive_relay/spatial/scintilla_engine.cpp').read_text()
        shader = SHADER_PATH.read_text()
        self.assertIn('DepthVisibility::visible(found->second.depth,static_cast<float>(z),', engine)
        self.assertIn('if(scintillaDepthVisible(scene,z,n,f))', shader)
        self.assertNotIn('0.000002', engine + shader)
        for projection in ('immersive_equirectangular.frag', 'immersive_fisheye_domemaster.frag'):
            self.assertIn('#stk_include "utils/immersive_scintilla.frag"', (ROOT / 'data/shaders' / projection).read_text())
        rtt = (ROOT / 'src/graphics/rtts.cpp').read_text()
        self.assertIn('GL_DEPTH24_STENCIL8', rtt)
        self.assertIn('glCopyTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24', engine)

    def test_depth_cube_blit_matches_packed_face_rtt_and_propagates_failure(self):
        rtt = (ROOT / 'src/graphics/rtts.cpp').read_text()
        cube = (ROOT / 'src/modes/immersive_relay/render/cubemap_render_target.cpp').read_text()
        self.assertIn('m_depth_stencil_tex = generateRTT(res, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8)', rtt)
        allocation = cube.split('glGenTextures(1, (GLuint*)&m_depth_cube_texture);', 1)[1].split('glBindTexture(GL_TEXTURE_CUBE_MAP, 0);', 1)[0]
        self.assertIn('GL_DEPTH24_STENCIL8, (GLsizei)size, (GLsizei)size,', allocation)
        self.assertIn('0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL)', allocation)
        copy = cube.split('bool CubemapRenderTarget::copyDepthFaceToCubeMap(', 1)[1].split('bool CubemapRenderTarget::renderFaces(', 1)[0]
        self.assertIn('glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,', copy)
        self.assertIn('glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,', copy)
        self.assertRegex(copy, r'glBlitFramebuffer\([\s\S]*?GL_DEPTH_BUFFER_BIT, GL_NEAREST\);\s*const GLenum copy_error = glGetError\(\);')
        self.assertIn('return copy_error == GL_NO_ERROR;', copy)

    def test_depth_arithmetic_const_initializers_are_valid_before_glsl_420(self):
        # This arithmetic-only helper must compile on the native Mac's GLSL
        # 4.10. A C++ const local may depend on arguments; a GLSL const cannot.
        constants = set()
        for name, expression in re.findall(r'const float (\w+)=(.*?);', self.shader.body):
            dependencies = {node.id for node in ast.walk(ast.parse(normalize(expression), mode='eval'))
                            if isinstance(node, ast.Name)}
            self.assertLessEqual(dependencies, constants, (name, expression))
            constants.add(name)

    def test_audit_wall_counterexample(self):
        n, f = binary32(0.2), 1000.0
        for rounding in (ROUND_FLOOR, ROUND_HALF_UP, ROUND_CEILING):
            scene = captured_depth(99.95, n, f, rounding)
            previous = f/(f-n) - f*n/((f-n)*100.0)
            self.assertGreaterEqual(scene + 0.000002, previous)
            self.assertBoth(scene, 100.0, n, f, False)
            self.assertBoth(captured_depth(100.05, n, f, rounding), 100.0, n, f, True)

    def test_equal_ambiguous_and_clear_depth_boundaries(self):
        n, f = binary32(0.2), 1000.0
        for rounding in (ROUND_FLOOR, ROUND_HALF_UP, ROUND_CEILING):
            scene = captured_depth(100.0, n, f, rounding)
            for z in (99.999, 100.0, 100.001):
                self.assertBoth(scene, z, n, f, False)
            self.assertBoth(scene, 99.95, n, f, True)
        for z in (0.201, 1.0, 10.0, 100.0, 500.0):
            self.assertBoth(1.0, z, n, f, True)
        self.assertBoth(1.0, 999.99, n, f, False)

    def test_invalid_values_and_strict_clip_planes(self):
        for scene in (-1.0, 1.000001, math.nan, math.inf, -math.inf):
            self.assertBoth(scene, 100.0, 0.2, 1000.0, False)
        for z in (-1.0, 0.0, 0.2, 1000.0, 1001.0, math.nan, math.inf, -math.inf):
            self.assertBoth(1.0, z, 0.2, 1000.0, False)
        for n in (-1.0, 0.0, math.nan, math.inf, -math.inf):
            self.assertBoth(1.0, 100.0, n, 1000.0, False)
        for f in (-1.0, 0.0, 0.2, 100.0, math.nan, math.inf, -math.inf):
            self.assertBoth(1.0, 100.0, 0.2, f, False)

    def test_occluders_reject_behind_and_preserve_separated_front_markers(self):
        rng = random.Random(6)
        for _ in range(5000):
            n = binary32(10**rng.uniform(-2, 0.3))
            f = binary32(n * 10**rng.uniform(1, 6))
            wall = n + (f-n) * 10**rng.uniform(-4, -0.005)
            behind = min(f, wall * (1 + 10**rng.uniform(-10, -1)))
            for rounding in (ROUND_FLOOR, ROUND_HALF_UP, ROUND_CEILING):
                scene = captured_depth(wall, n, f, rounding)
                self.assertBoth(scene, behind, n, f, False)
                self.assertBoth(scene, (wall+n)*0.5, n, f, True)

    def test_depth_code_edges_are_conservative(self):
        n, f = binary32(0.2), 1000.0
        for code in (1, 2, 16, 100, 2**20, 2**23, 16700000, 16777200, 16777214):
            depth = code / 16777215
            wall = f*n/(f-depth*(f-n))
            for rounding in (ROUND_FLOOR, ROUND_HALF_UP, ROUND_CEILING):
                scene = captured_depth(wall, n, f, rounding)
                self.assertBoth(scene, wall, n, f, False)
                self.assertBoth(scene, wall*(1+1e-6), n, f, False)


if __name__ == '__main__':
    unittest.main(verbosity=2)
