#!/usr/bin/env python3
"""Pure source/math checks. No compiler, game, graphics context or network.

These checks verify angular mapping and six-face coverage. They do not qualify
GPU allocation, shader compilation, Syphon reception or venue performance.
"""
import math
from pathlib import Path
import random
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
SHADER = (ROOT / 'data/shaders/immersive_equirectangular.frag').read_text()
SAMPLING = (ROOT / 'data/shaders/utils/immersive_sphere_sampling.frag').read_text()


def shader_direction(u, v_top):
    # Evaluate the scalar expressions in the actual shader, with GL's v=1-v_top.
    code = re.search(r'float longitude = (.*?);\s*float latitude = (.*?);\s*vec3 dir = vec3\((.*?)\);', SHADER, re.S)
    if not code:
        raise AssertionError('Native equirectangular direction expression missing')
    scope = {'u': u, 'v': 1-v_top, 'PI': math.pi, 'sin': math.sin, 'cos': math.cos}
    def scalar(expression):
        return eval(expression.replace('uv.x', 'u').replace('uv.y', 'v').strip(), {'__builtins__': {}}, scope)
    scope['longitude'] = scalar(code[1])
    scope['latitude'] = scalar(code[2])
    return tuple(scalar(expression) for expression in code[3].split(','))


def dot(a, b):
    return sum(x*y for x,y in zip(a,b))


class ProjectionSourceTests(unittest.TestCase):
    def assertVector(self, actual, expected):
        for a,e in zip(actual,expected):
            self.assertAlmostEqual(a,e,places=10)

    def test_cardinal_contract_and_vertical_publication(self):
        for u,v,d in [(0.5,0.5,(0,0,1)),(0.75,0.5,(1,0,0)),
                      (0.25,0.5,(-1,0,0)),(0,0.5,(0,0,-1)),
                      (1,0.5,(0,0,-1)),(0.5,0,(0,1,0)),(0.5,1,(0,-1,0))]:
            self.assertVector(shader_direction(u,v),d)
        syphon=(ROOT/'src/modes/immersive_relay/output/syphon_mac/syphon_output_adapter_mac.mm').read_text()
        self.assertIn('flipped:NO',syphon)

    def test_independent_receiver_round_trip(self):
        rng=random.Random(47)
        for _ in range(4000):
            u,v=rng.random(),rng.uniform(0.0001,0.9999)
            x,y,z=shader_direction(u,v)
            # Independent receiver inverse convention, top-left texture origin.
            actual_u=0.5+math.atan2(x,z)/(2*math.pi)
            actual_v=0.5-math.asin(max(-1,min(1,y)))/math.pi
            self.assertAlmostEqual(actual_u,u,places=9)
            self.assertAlmostEqual(actual_v,v,places=9)
            self.assertAlmostEqual(x*x+y*y+z*z,1,places=12)

    def test_seam_continuity_and_poles(self):
        for v in [0,0.001,0.1,0.5,0.9,0.999,1]:
            self.assertVector(shader_direction(0,v),shader_direction(1,v))
        for u in [0,0.25,0.5,0.75,1]:
            self.assertVector(shader_direction(u,0),(0,1,0))
            self.assertVector(shader_direction(u,1),(0,-1,0))

    def test_actual_shared_face_sampler_has_full_sphere_coverage(self):
        # Read ma/sc/tc from the common GLSL, then project rays onto independent
        # face camera bases. Detect flipped/backward faces and any uncovered seam.
        expressions=re.findall(r'addFaceSample\(color_sum, weight_sum, (face_\w+),\s*(.*?),\s*(.*?),\s*(.*?)\);',SAMPLING)
        bases={
            'face_pos_x':((1,0,0),(0,0,-1),(0,1,0)),
            'face_neg_x':((-1,0,0),(0,0,1),(0,1,0)),
            'face_pos_y':((0,1,0),(1,0,0),(0,0,-1)),
            'face_neg_y':((0,-1,0),(1,0,0),(0,0,1)),
            'face_pos_z':((0,0,1),(1,0,0),(0,1,0)),
            'face_neg_z':((0,0,-1),(-1,0,0),(0,1,0)),
        }
        self.assertEqual(set(e[0] for e in expressions),set(bases))
        for u in range(121):
            for v in range(61):
                direction=shader_direction(u/120,v/60)
                visible=0
                for name,*raw in expressions:
                    scope=dict(zip('xyz',direction))
                    actual=[eval(e.replace('dir.',''),{'__builtins__':{}},scope) for e in raw]
                    expected=[dot(direction,b) for b in bases[name]]
                    self.assertVector(actual,expected)
                    ma,sc,tc=actual
                    if ma>0.0001 and max(abs(sc),abs(tc)) < ma*math.tan(math.pi/3):
                        visible+=1
                self.assertGreater(visible,0,(u,v))
        for name in ['immersive_equirectangular.frag','immersive_fisheye_domemaster.frag']:
            text=(ROOT/'data/shaders'/name).read_text()
            self.assertIn('#stk_include "utils/immersive_sphere_sampling.frag"',text)
            self.assertNotIn('vec4 sampleMultiview(',text)

    def test_render_thread_and_presentation_gate_wiring(self):
        dispatcher=(ROOT/'src/modes/immersive_relay/remote/relay_game_dispatcher.cpp').read_text()
        self.assertRegex(dispatcher,r'if\s*\(\s*method\s*!=\s*"POST"')
        controller=(ROOT/'src/modes/immersive_relay/relay_session_controller.cpp').read_text()
        for method in ['startSession','operatorStart','operatorCountdown','operatorResume','confirmHandoff']:
            code=controller.split('bool RelaySessionController::'+method+'(',1)[1].split('\n}',1)[0]
            self.assertIn('!m_presentation_ready',code,method)
        self.assertIn('m_presentation_ready = true;',controller)
        hooks=(ROOT/'src/modes/immersive_relay/relay_main_hooks.cpp').read_text()
        self.assertIn('--relay-presentation-required',hooks)
        self.assertIn('setPresentationReady(false)',hooks)
        launcher=(ROOT/'tools/immersive/RunRelayTholusOS.command').read_text()
        self.assertIn('RELAY_PRESENTATION_REQUIRED:-0',launcher)
        self.assertIn('1024:1024|1536:1536|2048:2048',launcher)
        self.assertIn('2048:1024|4096:2048|8192:4096',launcher)
        pipeline=(ROOT/'src/modes/immersive_relay/render/immersive_render_pipeline.cpp').read_text()
        camera=(ROOT/'src/modes/immersive_relay/render/camera/immersive_camera_rig.cpp').read_text()
        self.assertNotIn('m_presentation_ready',pipeline)
        self.assertIn('m_stats.m_configuration_applied = published;',pipeline)
        stats=(ROOT/'src/modes/immersive_relay/render/immersive_render_stats.cpp').read_text()
        begin=stats.split('void ImmersiveRenderStats::beginFrame(',1)[1].split('void ImmersiveRenderStats::recordFrame',1)[0]
        self.assertNotIn('m_configuration_applied = false',begin)
        set_config=pipeline.split('void ImmersiveRenderPipeline::setConfig(',1)[1].split('ImmersiveRenderConfig ImmersiveRenderPipeline::getConfig',1)[0]
        self.assertIn('m_stats.m_configuration_applied = false;',set_config)
        self.assertNotIn('m_presentation_ready',camera)
        router=(ROOT/'src/modes/immersive_relay/remote/relay_command_router.cpp').read_text()
        ready=router.split('if (route_path == "/immersive/v1/operator/ready")',1)[1].split('\n    }',1)[0]
        # Results Ready may reset a combat race while video is held. It must
        # return before the ordinary Ready path, which gates every actual start.
        self.assertLess(ready.index('operatorReset'),ready.index('"scintilla_combat_reset_requires_prepare"'))
        self.assertLess(ready.index('"scintilla_combat_reset_requires_prepare"'),ready.index('controller->pilotReady'))
        pilot=controller.split('bool RelaySessionController::pilotReady(',1)[1].split('\n}',1)[0]
        for start in ['confirmHandoff(', 'operatorCountdown(', 'transitionArcadeLocked(']:
            self.assertLess(pilot.index('!state.m_presentation_ready'),pilot.index(start))
        self.assertIn('!m_presentation_ready || !m_scintilla_start_permitted',pilot)
        self.assertIn('render_reconfiguration_requires_pause',router)
        self.assertIn('equirectangular_requires_supported_2_to_1_projection',router)


if __name__ == '__main__':
    unittest.main(verbosity=2)
