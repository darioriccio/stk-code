#!/usr/bin/env python3
"""Source/math verification only. Does not compile or run STK, Swift or GLSL."""
import math
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
CORE = (ROOT / 'src/modes/immersive_relay/scintilla_combat.cpp').read_text()
ENGINE = (ROOT / 'src/modes/immersive_relay/spatial/scintilla_engine.cpp').read_text()
INPUT = (ROOT / 'src/modes/immersive_relay/spatial/scintilla_input.cpp').read_text()
ANIMATION = (ROOT / 'src/karts/explosion_animation.cpp').read_text()
TUNING = (ROOT / 'src/modes/immersive_relay/scintilla_tuning.hpp').read_text()


class CombatSourceAndMath(unittest.TestCase):
    def test_start_gate_distinguishes_intent_policy_and_current_input(self):
        header = (ROOT/'src/modes/immersive_relay/scintilla_combat.hpp').read_text()
        expression = re.search(r'bool permitsStart\(bool inputReady\) const \{ return ([^;]+); \}', header)[1]
        # Evaluate the actual Boolean predicate without a C++ compiler. Cases
        # specify permitted operator behavior, including intent before a grant.
        cases = [(False, False, False, True), (False, False, True, True),
                 (True, False, False, False), (True, False, True, False),
                 (False, True, False, False), (False, True, True, True),
                 (True, True, False, False), (True, True, True, True)]
        expression = expression.replace('required()', 'required').replace('configured()', 'configured')
        expression = expression.replace('!', ' not ').replace('||', ' or ').replace('&&', ' and ')
        for intent, configured, input_ready, expected in cases:
            self.assertEqual(eval(expression.strip(), {'__builtins__': {}},
                {'required': intent or configured, 'configured': configured, 'inputReady': input_ready}), expected)
        start = ENGINE.split('bool combatStartPermitted()', 1)[1].split('std::string combatStateJSON()', 1)[0]
        unconfigured = re.search(r'if\(!combat\.configured\(\)\)return ([^;]+);', start)[1]
        unconfigured = unconfigured.replace('mission.required()', 'mission_required')
        unconfigured = unconfigured.replace('defense.required()', 'defense_required')
        unconfigured = unconfigured.replace('combat.permitsStart(false)', 'combat_permitted')
        unconfigured = unconfigured.replace('!', ' not ').replace('&&', ' and ')
        for mission_required in (False, True):
            for defense_required in (False, True):
                for combat_permitted in (False, True):
                    self.assertEqual(eval(unconfigured.strip(), {'__builtins__': {}}, locals()),
                        not mission_required and not defense_required and combat_permitted)
        self.assertIn('!missionTerminal()&&missionStartPermitted()', start)
        self.assertIn('combat.reset(true)', ENGINE)
        self.assertIn('m_required=keep', CORE)

    def test_ready_cannot_combine_an_sc2_reset_with_a_countdown(self):
        router = (ROOT/'src/modes/immersive_relay/remote/relay_command_router.cpp').read_text()
        start = router.index('if (route_path == "/immersive/v1/operator/ready")')
        end = router.index('if (route_path == "/immersive/v1/operator/configure")', start)
        ready = router[start:end]
        self.assertLess(ready.index('RelayWorldController::reset()'), ready.index('ScintillaEngine::combatRequired()'))
        self.assertLess(ready.index('"scintilla_combat_reset_requires_prepare"'), ready.index('controller->pilotReady'))
        self.assertIn('if(route_path=="/immersive/v1/session/start"&&ImmersiveRelay::ScintillaEngine::combatRequired())', router)
        self.assertIn('combat_prefix+"intent"&&method=="POST"', router)
        self.assertIn('closed(j,{"contract","expectedRaceEpoch","expectedIntentRevision","required"})', INPUT)
        self.assertIn('at(j,"required").kind!=J::Bool', INPUT)

    def test_actual_duration_literals_match_half_open_plan(self):
        expected = {'acquire': (600000, True, 72), 'breach': (3500000, False, 420),
                    'cooperation': (1250000, False, 150), 'soloGap': (2500000, False, 300),
                    'soloTotal': (8000000, False, 960), 'stagger': (1000000, True, 120),
                    'recovery': (4000000, True, 480)}
        # Rounding stays in the core; the qualified durations are the built-in
        # tuning defaults (seconds), converted exactly as the core does.
        combat = TUNING.split('struct Combat', 1)[1].split('};', 1)[0]
        seconds = {k: float(v) for k, v in re.findall(r'(\w+)S=([\d.]+)', combat)}
        actual = {k: (round(seconds[name] * 1e6), ceil == 'true') for k, name, ceil in
                  re.findall(r'p\.(\w+)=ticks\(t\.(\w+)S,(true|false)\)', CORE)}
        self.assertEqual(set(actual), set(expected))
        for key, (us, ceil, ticks) in expected.items():
            self.assertEqual(actual[key], (us, ceil))
            self.assertEqual((us*120+(999999 if ceil else 0))//1000000, ticks)
        self.assertIn('m_now>=t.view.deadline', CORE)
        self.assertIn('s.admitted>=t.view.deadline', CORE)

    def test_stagger_curve_matches_independent_bezier(self):
        # Quartic Bezier controls [0,0,8/3,0,0] give the chosen normalized lift.
        expression = re.search(r'Profile::staggerCurve.*?return ([^;]+);', CORE, re.S)[1]
        self.assertEqual(expression, '16*u*u*(1-u)*(1-u)')
        for i in range(1001):
            u = i/1000
            direct = 16*u*u*(1-u)*(1-u)
            bernstein = math.comb(4, 2)*(1-u)**2*u**2*(8/3)
            self.assertAlmostEqual(direct, bernstein, places=14)
            self.assertTrue(0 <= direct <= 1)
            self.assertAlmostEqual(direct, 16*(1-u)**2*u**2, places=14)

    def test_pitch_never_requires_more_lift_than_provided(self):
        # The radius now includes the compound offset about the real body origin.
        # Reserve half the lift for normals at the minimum support alignment.
        for radius in (0.5, 1, 1.5, 2, 4, 8):
            maximum = min(math.radians(8), 0.5*0.25/radius)
            for i in range(101):
                u = i/100
                w = 16*u*u*(1-u)*(1-u)
                displacement = 2*radius*math.sin(maximum*w/2)
                self.assertLessEqual(displacement, 0.5*0.25*w+1e-14)
        self.assertIn('MinimumSupportAlignment*lift/e.radius', CORE)
        self.assertIn('shape->getAabb(identity,lower,upper)', ANIMATION)

    def test_completed_gpu_result_requires_no_live_owners_for_pressure_eviction(self):
        policy = (ROOT / 'src/modes/immersive_relay/spatial/scintilla_readback_cache.hpp').read_text()
        self.assertIn('complete&&!owned(it->second)', policy)
        self.assertIn('reclaimCompletedUnowned(queries,poll', ENGINE)
        self.assertIn('p.visibilityResults[f->id]=v', INPUT)
        self.assertIn('std::shared_ptr<void> queryLease', INPUT)
        self.assertIn('resolveQueue(pending,now)', INPUT)
        self.assertIn('resolveQueue(pendingAims,now)', INPUT)

    def test_shared_range_and_historical_window_are_used(self):
        distance = 0.35/math.sin(math.radians(0.75))
        self.assertAlmostEqual(distance, 26.738794036250827)
        self.assertIn('ScintillaCombat::Profile::inRange', INPUT)
        self.assertIn('ScintillaCombat::Profile::inRange', ENGINE)
        self.assertIn('snapshot.window==p.event.historical.window', INPUT)
        self.assertIn('s.window!=t.view.window', CORE)
        self.assertIn('s.admitted<t.notBefore', CORE)

    def test_resolved_beam_loss_survives_later_positive_proofs(self):
        # Evaluate the actual C++ Boolean guard, without compiling it. Input
        # traces specify the independent requirement: no return to a held lock
        # after an established negative or a target/instance transition.
        finish = INPUT.split('void finish(Pending& p,', 1)[1].split('void resolveQueue(', 1)[0]
        # The loss guard is named so beamGraceS can defer it; without grace it
        # still latches the interruption in the same resolution.
        guard = re.search(r'const bool lost=(source\.proofVisible&&.*?);', finish, re.S)[1]
        self.assertIn('if(lost)source.proofInterrupted=true;', finish)
        self.assertIn('if(graceUs()>0&&(departed||lost))', finish)
        self.assertIn('missionKind(p.event.kind)?p.event.historicalMission.instance:p.event.historical.instance', finish)
        guard = guard.replace('source.proofVisible', 'previous_visible').replace('source.proofTarget', 'previous_target')
        guard = guard.replace('source.proofInstance', 'previous_instance').replace('proofInstance', 'instance')
        guard = guard.replace('source.proofKind', 'previous_kind').replace('p.event.kind', 'kind')
        guard = guard.replace('source.proofMission', 'previous_mission').replace('p.event.historicalMission', 'mission')
        guard = guard.replace('missionKind', 'is_mission').replace('sameMission', 'same_mission')
        guard = guard.replace('&&', ' and ').replace('||', ' or ')
        guard = re.sub(r'!(?!=)', ' not ', guard)
        same = re.search(r'bool sameMission\([^}]+\{return ([^;]+);', INPUT)[1]
        same = re.sub(r'\b([ab])\.(\w+)', r'\1["\2"]', same).replace('&&', ' and ')
        def same_mission(a, b):
            return eval(same, {'__builtins__': {}}, {'a': a, 'b': b})
        def is_mission(kind):
            return kind in ('capsule', 'tether')
        # Keep all SC2 traces, and verify the newly typed identity/phase barrier
        # without allowing a later return to the old capsule to erase a loss.
        traces = [([(True, 7, 700, 'rival', 1)]*10, True),
                  ([(False, 0, 0, 'rival', 1), (True, 7, 700, 'rival', 1)], True),
                  ([(True, 7, 700, 'rival', 1), (False, 0, 0, 'rival', 1), (True, 7, 700, 'rival', 1)], False),
                  ([(True, 7, 700, 'rival', 1), (True, 8, 800, 'rival', 1), (True, 7, 700, 'rival', 1)], False),
                  ([(True, 7, 700, 'rival', 1), (True, 7, 701, 'rival', 1), (True, 7, 700, 'rival', 1)], False),
                  ([(True, 7, 700, 'capsule', 1)]*10, True),
                  ([(True, 7, 700, 'capsule', 1), (True, 7, 700, 'tether', 1)], False),
                  ([(True, 7, 700, 'capsule', 1), (True, 7, 700, 'capsule', 2), (True, 7, 700, 'capsule', 1)], False)]
        output = re.search(r'b.visible=([^;]+);', INPUT)[1]
        # Built-in tuning: no grace, so interrupted(s,now) is the latched flag.
        output = output.replace('m->interrupted(s,now)', 'interrupted').replace('m->graceUs()', '0')
        output = output.replace('b.held', 'held').replace('s.proofInterrupted', 'interrupted').replace('s.proofVisible', 'visible')
        output = output.replace('m->issuedAt', 'issued').replace('&&', ' and ')
        output = re.sub(r'!(?!=)', ' not ', output)
        for trace, expected in traces:
            previous_visible = interrupted = False
            previous_target = previous_instance = 0
            previous_kind, previous_mission = 'rival', {}
            for visible, target, instance, kind, revision in trace:
                mission = dict(id=target, instance=instance, module=1, kind=kind,
                               phase='open', phaseRevision=revision)
                interrupted = interrupted or eval(guard, {'__builtins__': {}}, locals())
                previous_visible, previous_target, previous_instance = visible, target, instance
                previous_kind, previous_mission = kind, mission
            self.assertEqual(eval(output, {'__builtins__': {}}, dict(held=True, interrupted=interrupted,
                visible=visible, low=1000000, issued=990000, now=1000000)), expected)

    def test_beam_barrier_is_epoch_scoped_and_reaches_combat_before_shots(self):
        finish = INPUT.split('void finish(Pending& p,', 1)[1].split('void resolveQueue(', 1)[0]
        self.assertIn('if(p.aim||secondary)', finish)
        self.assertLess(finish.index('p.toolEpoch==source.toolEpoch&&p.proofSerial>=source.proofSerial'),
                        finish.index('source.proofInterrupted=true'))
        self.assertLess(finish.index('if(p.aim)return;'), finish.index('record(p.event)'))
        # Exactly one reset, inside the admitted secondary press. Observation,
        # release, queued samples and primary actions cannot acknowledge it.
        self.assertEqual(INPUT.count('source.proofInterrupted=false;'), 1)
        admitted = INPUT.index('source.held[controlIndex]=true;if(controlIndex==1)')
        self.assertGreater(INPUT.index('source.proofInterrupted=false;'), admitted)
        self.assertLess(INPUT.index('if(!wasRearmed||!source.tracked||!m->active||!point)'), admitted)
        self.assertIn('m_tools[b.participant].epoch==b.controlEpoch', CORE)
        self.assertIn('if(found==beams.end()||!t.compatible)close(t)', CORE)
        self.assertIn('if(tool.blocked||!b->visible)continue', CORE)
        self.assertLess(ENGINE.index('combat.update(simulationTick,admitted,beams)'),
                        ENGINE.index('for(const auto& e:consumer.drainCombatShots())'))
        self.assertEqual(ENGINE.count('consumer.drainCombatShots()'), 1)
        registry = (ROOT/'tools/immersive/tests/scintilla_input_cases.inc').read_text()
        tests = (ROOT/'tools/immersive/tests/scintilla_input_regression.cpp').read_text()
        names = re.findall(r'^SCINTILLA_CASE\((\w+)\)$', registry, re.M)
        self.assertEqual(len(names), len(set(names)))
        for name in names:
            self.assertEqual(len(re.findall(r'void '+name+r'\(\)', tests)), 1, name)

    def test_local_effect_scope_and_game_thread_routes(self):
        relay = ANIMATION.split('ExplosionAnimation::ExplosionAnimation(AbstractKart* kart,int ticks,')[1].split('#endif')[0]
        self.assertNotIn('getAttachment()->clear', relay)
        self.assertNotIn('setInvulnerable', relay)
        self.assertNotIn('Camera::', relay)
        self.assertIn('if(m_relay_stagger)return;', ANIMATION)
        self.assertIn('elapsed>=m_relay_ticks', ANIMATION)
        dispatcher = (ROOT/'src/modes/immersive_relay/remote/relay_game_dispatcher.cpp').read_text()
        self.assertIn('"/immersive/v1/scintilla/"', dispatcher)
        loop = (ROOT/'src/modes/immersive_relay/relay_race_mode.cpp').read_text()
        self.assertIn('ScintillaEngine::updateCombat(0,false)', loop)
        self.assertIn('ScintillaEngine::updateCombat(ticks,', loop)


if __name__ == '__main__':
    unittest.main()
