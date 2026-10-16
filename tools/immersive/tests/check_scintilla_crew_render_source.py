#!/usr/bin/env python3
"""SC5 CPU/shader/qualification integration guards; no compilation or GPU proof."""
from pathlib import Path
import re
import unittest

ROOT=Path(__file__).resolve().parents[3]
IR=ROOT/'src/modes/immersive_relay'


class CrewRenderSource(unittest.TestCase):
    def test_opaque_crew_surfaces_cannot_erase_a_centered_pointer(self):
        # Evaluate composition at a fully covered cross pixel using the pass
        # order from the real shader. This is arithmetic/source proof, not GLSL
        # execution. The former late opaque HUD pass fails this counterexample.
        shader=(ROOT/'data/shaders/utils/immersive_scintilla.frag').read_text()
        overlay=shader[shader.index('vec3 scintillaOverlay('):shader.index('float scintillaAlpha(')]
        active=overlay[overlay.index('vec2 topUV='):]
        hud=active.index('scintillaCrewHUD(')
        pointer=active.index('color=mix(color,palette[i],max(max(crossMark,dots),tool))')
        palette_start=active.index('vec3 palette[5]')
        palette_text=active[palette_start:active.index('for(int i=0;i<5;i++)',palette_start)]
        palette=[tuple(map(float,value.split(','))) for value in re.findall(r'vec3\(([^)]+)\)',palette_text)]
        self.assertEqual(len(palette),5)
        for width,height in ((256,256),(768,768),(1536,1536),(2048,1024),(8192,8192)):
            radius=.06*min(width,height)/2
            self.assertGreater(radius,0) # Party's centre is inside its opaque mask at every admitted size.
            for tint in palette:
                color=(0.,0.,0.)
                for _,layer in sorted(((hud,'hud'),(pointer,'pointer'))):
                    color=(.42,.14,.65) if layer=='hud' else tint
                self.assertEqual(color,tint,(width,height,tint))
        self.assertLess(hud,pointer)
        self.assertNotIn('scintillaCrewHUD(',active[pointer:])
        # SC5 consumes the translated badge state after legacy panels, then
        # draws pointers last. World-space feedback outside the HUD survives.
        self.assertLess(active.index('color=scintillaFeedback('),hud)
        self.assertLess(active.index('color=scintillaToolFeedback('),hud)

    def test_terminal_crew_plate_and_legacy_path_keep_their_own_boundaries(self):
        shader=(ROOT/'data/shaders/utils/immersive_scintilla.frag').read_text()
        overlay=shader[shader.index('vec3 scintillaOverlay('):shader.index('float scintillaAlpha(')]
        active=overlay[overlay.index('vec2 topUV='):]
        hud=active.index('color=scintillaCrewHUD(')
        pointer=active.index('vec4 r=scintilla_reticles[i]')
        self.assertIn('if(scintilla_mission_result.x>.5)return color;',active[hud:pointer])
        end=active[active.index('if(scintilla_crew_enabled!=0)return color;'):]
        self.assertIn('color=scintillaFeedback(',end)
        self.assertIn('color=scintillaToolFeedback(',end)
        self.assertIn('return scintillaMissionHUD(color,uvGL,dimensions);',end)
        # Neither feedback helper may paint another opaque crew surface later.
        feedback=shader[shader.index('vec3 scintillaFeedback('):shader.index('vec3 scintillaCrewHUD(')]
        self.assertNotIn('scintillaCrewHUD(',feedback)

    def test_legacy_panels_cannot_cover_crew_text_while_spatial_feedback_survives(self):
        shader=(ROOT/'data/shaders/utils/immersive_scintilla.frag').read_text()
        overlay=shader[shader.index('vec3 scintillaOverlay('):shader.index('float scintillaAlpha(')]
        active=overlay[overlay.index('vec2 topUV='):]
        passes=sorted(((active.index('color=scintillaFeedback('),'shot'),
                       (active.index('color=scintillaToolFeedback('),'panel'),
                       (active.index('color=scintillaCrewHUD('),'crew')))
        text=(.98,.98,.98);hit=(.25,1.,.35)
        for covered_by_hud in (True,False):
            color=(.1,.1,.1)
            for _,layer in passes:
                if layer=='shot' and not covered_by_hud:color=hit
                if layer=='panel' and covered_by_hud:color=tuple(.1*c+.9*.015 for c in color)
                if layer=='crew' and covered_by_hud:color=text
            self.assertEqual(color,text if covered_by_hud else hit)

    def test_party_geometry_and_explicit_measure_revision_share_the_same_circle(self):
        shader=(ROOT/'data/shaders/utils/immersive_scintilla.frag').read_text()
        layout=(IR/'scintilla_layout.hpp').read_text()
        self.assertIn('Context::PartyBoost',layout)
        self.assertIn('scintilla_measure_layout_revision==3',shader)
        self.assertGreaterEqual(shader.count('vec2(.22,.85))<=.06'),2)
        self.assertIn('radius<=1.0',shader) # all node painting is inside its reserved circle

    def test_hud_uses_real_project_glyphs_in_held_memory_and_bounded_texture_cache(self):
        source=(IR/'spatial/scintilla_crew_text_native.hpp').read_text()
        self.assertIn('FontWithFace::FontCharCollector',source)
        self.assertIn('getFont<RegularFace>()',source)
        self.assertIn('isWorldHeld()',source)
        self.assertIn('createImageFromData',source)
        self.assertIn('pages.size()>=16',source)
        self.assertIn('manager->removeTexture',source)
        self.assertNotIn('writeImageToFile',source);self.assertNotIn('dumpGlyphPage(',source)
        self.assertNotIn('FontDrawer::',source)

    def test_hint_identity_is_bound_to_real_instance_and_slot_mapping_is_injective(self):
        source=(IR/'spatial/scintilla_engine.cpp').read_text()
        self.assertIn('t.id==hint.targetId&&bound',source)
        self.assertIn('t.combat.instance==hint.instance&&t.combat.window==hint.phaseRevision',source)
        self.assertIn('t.instance==hint.instance&&t.mission.phaseRevision==hint.phaseRevision',source)
        self.assertIn('!used[r.slot]',source);self.assertIn('!used[slot]',source)
        self.assertIn('Runtime::partyFeedbackForRendering(r)',source)
        self.assertIn('f.participantId==r.participantId&&f.controlEpoch==r.toolEpoch',source)
        self.assertIn('current.crewInputEpoch=consumer.inputEpoch();current.crewRosterRevision=rosterEpoch',source)

    def test_new_uniforms_are_bound_without_an_extra_sampler(self):
        source=(IR/'spatial/scintilla_engine.cpp').read_text()
        shader=(ROOT/'data/shaders/utils/immersive_scintilla.frag').read_text()
        for name in re.findall(r'uniform\s+\w+\s+(scintilla_crew_\w+)',shader):
            self.assertIn('"'+name,source)
        self.assertIn('uniform vec4 scintilla_crew_target_masks[8]',shader)
        self.assertIn('"scintilla_crew_target_masks[0]"),8,targetMasks',source)
        self.assertIn('scintilla_crew_tools[i].x',shader)
        self.assertEqual(len(re.findall(r'uniform sampler2D scintilla_.*text',shader)),1)

    def test_new_badges_preserve_release_owner_and_existing_mission_outcomes(self):
        source=(IR/'spatial/scintilla_engine.cpp').read_text()
        text=(IR/'spatial/scintilla_crew_text_native.hpp').read_text()
        for helper in ('toolFeedbackForRendering','missionFeedbackForRendering','feedbackForRendering'):
            self.assertIn('Runtime::'+helper+'(r',source)
        self.assertIn('ownerName(slot,combatTool.owner,combat.crew())',source)
        self.assertIn('ownerName(slot,missionTool.owner,mission.crew())',source)
        self.assertIn('missionTool.outcome',source)
        for label in ('Bersaglio occupato','Guscio aperto','Raccolto','Recuperato','Carico perso','Rifiutato','Rilascia e ripremi'):
            self.assertIn(label,text)

    def test_diagnostic_does_not_require_crew_intent_and_profiles_have_separate_reports(self):
        source=(IR/'spatial/scintilla_engine.cpp').read_text()
        native=(IR/'spatial/scintilla_measurement_native.hpp').read_text()
        self.assertIn('{"crewProfile"}',source)
        self.assertIn('measurement.crewProfile=diagnosticProfile',source)
        self.assertIn('crew.required()?crew.profileId():measurement.started&&!measurement.closed?measurement.crewProfile',native)
        self.assertIn('qualification_v2/"+effectiveCrewProfile()+"/report.json',native)
        self.assertIn('rel=="immersive_relay/scintilla/qualification_v2"',native)
        self.assertIn('crewPartyZoneManifestHash()',native)


if __name__=='__main__':unittest.main()
