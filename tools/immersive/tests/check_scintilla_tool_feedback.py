#!/usr/bin/env python3
"""Compiler-free source/geometry checks. Does not execute C++, GLSL or STK."""
from itertools import product
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
CORE = (ROOT/'src/modes/immersive_relay/scintilla_combat.cpp').read_text()
HEADER = (ROOT/'src/modes/immersive_relay/scintilla_combat.hpp').read_text()
INPUT = (ROOT/'src/modes/immersive_relay/spatial/scintilla_input.cpp').read_text()
ENGINE = (ROOT/'src/modes/immersive_relay/spatial/scintilla_engine.cpp').read_text()
SHADER = (ROOT/'data/shaders/utils/immersive_scintilla.frag').read_text()


class ToolFeedbackSourceChecks(unittest.TestCase):
    def test_status_codes_uniforms_and_both_projections_agree(self):
        self.assertIn('enum class ToolStatus { None=0, ReleaseRequired=1, TargetBusy=2 }', HEADER)
        self.assertIn('uniform vec4 scintilla_tools[5];', SHADER)
        badge = SHADER.split('vec3 scintillaToolFeedback(', 1)[1].split('vec3 scintillaOverlay(', 1)[0]
        guard = re.search(r'if\((\(!missionBadge&&status[^\n]+)\)continue;', badge)[1]
        guard = guard.replace('&&', ' and ').replace('||', ' or ')
        guard = re.sub(r'!(?!=)', ' not ', guard)
        # SC4 adds typed statuses; the legacy SC2/SC3 path must still admit
        # exactly release/busy, and never reinterpret any other status.
        for missionBadge, status in product((False, True), range(-1, 12)):
            hidden = eval(guard.strip(), {'__builtins__': {}}, locals())
            expected = 1 <= status <= 9 if missionBadge else status in (1, 2)
            self.assertEqual(not hidden, expected)
        self.assertIn('if(status==1)', SHADER)
        self.assertIn('tools[r.slot*4]=static_cast<int>(tool.status)', ENGINE)
        self.assertIn('glUniform4fv(glGetUniformLocation(program,"scintilla_tools[0]"),5,tools)', ENGINE)
        for projection in ('immersive_equirectangular.frag', 'immersive_fisheye_domemaster.frag'):
            self.assertIn('#stk_include "utils/immersive_scintilla.frag"', (ROOT/'data/shaders'/projection).read_text())

    def test_actual_render_gate_hides_released_or_mismatched_feedback(self):
        block = INPUT.split('Runtime::toolFeedbackForRendering(const Reticle& r)', 1)[1].split('const char*', 1)[0]
        guard = re.search(r'return ([^?]+)\?', block)[1]
        guard = guard.replace('r.toolFeedback.participant!=r.participantId', 'not same_person')
        guard = guard.replace('r.toolFeedback.controlEpoch!=r.toolEpoch', 'not same_epoch')
        guard = guard.replace('r.valid', 'valid').replace('r.toolHeld', 'held').replace('||', ' or ')
        guard = re.sub(r'!(?!=)', ' not ', guard)
        for valid, held, same_person, same_epoch in product((False, True), repeat=4):
            hidden = eval(guard.strip(), {'__builtins__': {}}, locals())
            self.assertEqual(hidden, not all((valid, held, same_person, same_epoch)))
        self.assertNotIn('untilUs', block)
        self.assertNotIn('feedback.kind', block)

    def test_sticky_input_loss_precedes_any_same_epoch_core_view(self):
        block = INPUT.split('void setToolFeedback(', 1)[1].split('void clearControls(', 1)[0]
        combat = block.split('void setMissionFeedback(', 1)[0]
        self.assertLess(combat.index('if(s.proofInterrupted)'), combat.index('else if(value.participant'))
        self.assertIn('value.controlEpoch==s.toolEpoch', block)
        guard = re.search(r'if\(([^\n]+)\)return;', combat)[1]
        guard = guard.replace('missionKind(s.proofKind)', 'mission_kind')
        guard = guard.replace('s.tracked', 'tracked').replace('s.held[1]', 'held')
        guard = guard.replace('||', ' or ')
        guard = re.sub(r'!(?!=)', ' not ', guard)
        for combatEnabled, prepared, active, tracked, held, mission_kind in product((False, True), repeat=6):
            hidden = eval(guard.strip(), {'__builtins__': {}}, locals())
            self.assertEqual(hidden, not all((combatEnabled, prepared, active, tracked, held)) or mission_kind)
        mission = block.split('void setMissionFeedback(', 1)[1]
        self.assertIn('if(s.proofInterrupted)', mission)
        self.assertIn('ScintillaMission::Status::ReleaseRequired', mission)
        self.assertIn('setToolFeedback(i,reticles[i].toolFeedback)', INPUT)
        self.assertIn('if(!enabled)for(auto& r:m->reticles)r.toolFeedback=ScintillaCombat::ToolSnapshot()', INPUT)
        release = INPUT.split('if(edge=="released"){', 1)[1].split('if(!m->historyReady)', 1)[0]
        for reset in ('reticle.toolFeedback=ScintillaCombat::ToolSnapshot()',
                      'reticle.defenseFeedback=ScintillaDefense::ToolFeedback()',
                      'reticle.missionFeedback=ScintillaMission::Feedback()',
                      'reticle.toolEpoch=source.toolEpoch'):
            self.assertLess(release.index(reset), release.index('return ok("released")'))
        self.assertNotIn('proofInterrupted=false', release)
        self.assertIn('"tool_release_required"', INPUT)
        self.assertIn('"target_busy"', INPUT)
        self.assertIn('Release the tool button, then press it again.', INPUT)
        self.assertIn('fire when the shield opens', INPUT.lower())

    def test_authoritative_state_publishes_after_effects_and_contends_by_current_owner(self):
        update = ENGINE.split('void updateCombat(', 1)[1].split('void beforeCapture(', 1)[0]
        self.assertGreater(update.rindex('refreshCombatInput()'), update.index('combat.confirmEffect('))
        self.assertIn('if(!world){combat.suspendInput();defense.suspend("world_unavailable");consumer.setCombatContext(false,simulationTick,{},raceEpoch);return;}', update)
        refresh = ENGINE.split('void refreshCombatInput()', 1)[1].split('bool combatStartPermitted()', 1)[0]
        self.assertIn('consumer.setCombatToolFeedback(combat.toolSnapshots())', refresh)
        snapshot = CORE.split('Runtime::toolSnapshots() const', 1)[1].split('Snapshot Runtime::snapshot(', 1)[0]
        self.assertIn('if(tool.blocked)', snapshot)
        self.assertLess(snapshot.index('ToolStatus::ReleaseRequired'), snapshot.index('ToolStatus::TargetBusy'))
        self.assertIn('t.instance==tool.aimInstance&&t.opener>=0&&t.opener!=slot(person)', snapshot)
        self.assertIn('t.phase==Phase::Acquiring||t.phase==Phase::Breached', snapshot)
        self.assertIn('view.owner=t.opener', snapshot)
        self.assertIn('tool.aimTarget=tool.aimInstance=0', CORE)
        self.assertIn('if(tool.blocked||!b->visible)continue', CORE)

    def test_persistent_badges_fit_both_projection_domains_and_do_not_overlap(self):
        # Use the actual GLSL anchor and scale expressions. Check whole clipping
        # boxes, stricter than checking only the icon centers or outline points.
        block = SHADER.split('vec3 scintillaToolFeedback(', 1)[1].split('vec3 scintillaOverlay(', 1)[0]
        x_expr, y_expr = re.search(r'vec2 anchor=vec2\(([^,]+),([^;]+)\);', block).groups()
        x_expr = x_expr.replace('float(i)', 'slot')
        scale_expr = re.search(r'float scale=([^;]+);', block)[1].replace('dimensions.x', 'width')
        radius = float(re.search(r'if\(max\(abs\(p.x\),abs\(p.y\)\)>([\d.]+)\)', block)[1])
        for width, aspect in product((256, 512, 1024, 2048, 4096), (1, 2)):
            height = width/aspect
            scale = eval(scale_expr, {'__builtins__': {}, 'min': min}, {'width': width})
            previous_right = -1
            for slot in range(5):
                x = eval(x_expr, {'__builtins__': {}}, {'slot': slot})
                y = eval(y_expr, {'__builtins__': {}}, {})
                dx, dy = radius*scale/width, radius*scale/height
                self.assertGreater(x-dx, previous_right)
                previous_right = x+dx
                for cx, cy in product((x-dx, x+dx), (y-dy, y+dy)):
                    self.assertTrue(0 < cx < 1 and 0 < cy < 1)
                    self.assertLess((2*cx-1)**2+(2*cy-1)**2, 1)
        held_gate = re.search(r'float tool=([^?]+)\?', SHADER)[1]
        held_gate = held_gate.replace('r.w', 'buttons').replace('scintilla_tools[i].x', 'combat_status')
        held_gate = held_gate.replace('scintilla_mission_tools[i].x', 'mission_status').replace('&&', ' and ')
        for buttons, combat_status, mission_status in product((0, 1, 2, 3), (0, 1, 2), (0, 1, 2)):
            shown = eval(held_gate, {'__builtins__': {}}, locals())
            self.assertEqual(shown, buttons > 1.5 and combat_status < .5 and mission_status < .5)
        overlay = SHADER.split('vec3 scintillaOverlay(', 1)[1].split('float scintillaAlpha(', 1)[0]
        self.assertIn('if(scintilla_enabled==0)return scintillaMissionHUD(color,uvGL,dimensions)', overlay)
        self.assertLess(overlay.index('color=scintillaToolFeedback(color,uvGL,dimensions)'),
                        overlay.rindex('return scintillaMissionHUD(color,uvGL,dimensions)'))
        self.assertNotIn('untilUs', block)
        self.assertNotIn('f.w', block)
        # Icons use different shapes as well as color; the owner and recipient
        # have distinct rows with existing identity dots.
        self.assertIn('vec2(-12,17)', block)  # Release arrow points up.
        self.assertIn('vec2(12,3)', block)    # Press arrow points down.
        self.assertIn('float shackle=', block)
        self.assertIn('scintillaIdentityColor(owner)', block)
        self.assertIn('scintillaIdentityColor(i)', block)

    def test_new_regressions_are_registered_once_in_existing_native_runners(self):
        required = {
            'scintilla_combat': {'toolReleaseFeedbackPersistsAcrossEveryLockClosure',
                'toolBusyFeedbackUsesTheActualOwnerAndNeverStealsALock',
                'toolFeedbackClearsForSuspendReprepareResetAndSolo',
                'toolFeedbackReconcilesCompletionBeforeTheNextTick'},
            'scintilla_input': {'combatToolFeedbackIsPersistentIndependentAndEpochChecked',
                'combatToolInterruptionInstructionCannotBeHiddenByAStaleCoreView',
                'combatBeamInterruptionSurvivesPositiveBeforeGameplay',
                'combatBeamOldEpochFailureCannotPoisonANewPress',
                'defenseWarningDoesNotInterruptWorldOffense',
                'missionFeedbackIsEpochCheckedPersistentAndContextSpecific',
                'missionStickyLossAndCoalescedRealReleaseReachTheCore',
                'missionNewDefenseWarningDoesNotInterruptTheSameCapsule'},
        }
        for prefix, macro, minimum in (('scintilla_combat', 'COMBAT_CASE', 25), ('scintilla_input', 'SCINTILLA_CASE', 80)):
            registry = (ROOT/f'tools/immersive/tests/{prefix}_cases.inc').read_text()
            tests = (ROOT/f'tools/immersive/tests/{prefix}_regression.cpp').read_text()
            names = re.findall(r'^'+macro+r'\((\w+)\)$', registry, re.M)
            self.assertGreaterEqual(len(names), minimum)
            self.assertEqual(len(names), len(set(names)))
            self.assertTrue(required[prefix] <= set(names), required[prefix] - set(names))
            definitions = re.findall(r'^void (\w+)\(\)\s*\{', tests, re.M)
            self.assertEqual(set(names), set(definitions))
            for name in names:
                self.assertEqual(len(re.findall(r'void '+name+r'\(\)', tests)), 1, name)


if __name__ == '__main__':
    unittest.main(verbosity=2)
