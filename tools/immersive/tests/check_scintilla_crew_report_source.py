#!/usr/bin/env python3
"""Source contracts for SC5 frozen reports and HUD; no native or GPU proof.

These checks inspect call sites and ownership boundaries. C++ regression tests,
the native report lifecycle and rendered font/atlas validation remain separate.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
IR = ROOT / "src/modes/immersive_relay"


def read(relative):
    return (IR / relative).read_text(encoding="utf-8")


def function(source, symbol):
    # Ignore braces in JSON strings and comments while retaining source offsets.
    literals = r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\''
    masked = re.sub(literals, lambda match: " " * len(match.group()), source, flags=re.S)
    match = re.search(re.escape(symbol) + r"\s*\([^;{}]*\)\s*(?:const\s*)?\{", masked)
    if match is None:
        raise AssertionError("Missing function definition: " + symbol)
    start = match.end()
    depth = 1
    for index in range(start, len(masked)):
        depth += (masked[index] == "{") - (masked[index] == "}")
        if depth == 0:
            return source[start:index]
    raise AssertionError("Unclosed function definition: " + symbol)


class CrewReportSource(unittest.TestCase):
    def test_one_serializer_connects_state_http_report_and_persisted_report(self):
        serializer = read("scintilla_crew_result_json.hpp")
        wire = read("spatial/scintilla_crew_wire.hpp")
        publisher = read("remote/relay_state_publisher.cpp")
        controller = read("relay_session_controller.cpp")
        self.assertIn("Crew::writeResultJSON(out,frozen)", wire)
        self.assertIn("ScintillaCrew::writeResultJSON(out,report.m_scintilla_crew)", publisher)
        self.assertIn("ScintillaCrew::writeResultJSON(file,m_last_report.m_scintilla_crew)", controller)
        for field in ("raceEpoch", "missionRevision", "finishSequence", "finishTick",
                      "completedHandoffs", "partyActivations", "rotationAnomalies"):
            self.assertIn("quote(std::to_string(result." + field + "))", serializer)
        self.assertIn("writeReportParticipantJSON(out,result.participants[i])", serializer)
        self.assertIn("quote(std::to_string(person.contributions[c]))", serializer)
        self.assertIn("quote(std::to_string(person.opportunityTicks[c]))", serializer)

    def test_natural_freeze_precedes_admission_close_and_result_publication(self):
        source = read("spatial/scintilla_engine.cpp")
        natural = function(source, "freezeMissionNaturalFinish")
        order = ("mission.freezeNaturalFinish", "missionTerminalRecord=mission.result()",
                 "Crew::observeResult", "crewTerminalRecord=Crew::freezeResult",
                 "closeMissionAdmission()", "publishMissionSnapshot()")
        offsets = [natural.index(token) for token in order]
        self.assertEqual(offsets, sorted(offsets))
        boundary = function(source, "frozenCrewResult")
        self.assertIn("Crew::matchesMissionResult", boundary)
        self.assertIn("getCrewResult()", boundary)
        self.assertIn("result.raceEpoch==raceEpoch", boundary)
        self.assertNotIn("Crew::observeResult", boundary)

    def test_artificial_report_freezes_and_publishing_preserves_original_record(self):
        source = read("relay_session_controller.cpp")
        artificial = function(source, "RelaySessionController::finalizeMissionArtificialLocked")
        self.assertIn("m_mission_result.valid", artificial)
        self.assertIn("ScintillaCrew::freezeResult", artificial)
        self.assertIn("m_crew_result.comparable=false", artificial)
        build = function(source, "RelaySessionController::buildReportLocked")
        self.assertIn("report.m_mission = m_mission_result", build)
        self.assertIn("report.m_scintilla_crew = m_crew_result", build)
        publish = function(source, "RelaySessionController::publishReportLocked")
        self.assertLess(publish.index("m_last_report.m_available"), publish.index("buildReportLocked"))
        self.assertIn("writeReportExclusive", publish)
        self.assertNotIn("ScintillaSpatialInput::", publish)

    def test_party_bucket_depends_on_profile_even_with_no_activations(self):
        source = read("scintilla_crew_result.hpp")
        observe = function(source, "observeResult")
        self.assertIn('result.bucket=result.partyEnabled?"scintilla_sc5_party_v1":"scintilla_sc5_standard_v1"', observe)
        self.assertIn("result.partyActivations=runtime.partyActivations()", observe)
        freeze = function(source, "freezeResult")
        self.assertIn("observation.snapshot.valid", freeze)
        self.assertIn("matchesMissionResult(observation.snapshot,mission)", freeze)
        self.assertIn("mission.natural&&observation.prepared&&!observation.test", freeze)
        self.assertIn("!result.rotationAnomalies&&!result.countersSaturated", freeze)
        self.assertNotIn("partyActivations>0", freeze)

    def test_terminal_participants_are_the_same_frozen_copy_as_the_envelope(self):
        wire = read("spatial/scintilla_crew_wire.hpp")
        participants = function(wire, "writeCrewParticipants")
        self.assertIn("frozen.participants", participants)
        self.assertIn("writeReportParticipantJSON", participants)
        state = function(wire, "crewStateJSON")
        self.assertEqual(state.count("frozenCrewResult()"), 1)
        self.assertIn("writeCrewParticipants(out,frozen)", state)
        self.assertIn("Crew::writeResultJSON(out,frozen)", state)

    def test_terminal_hud_uses_frozen_identity_and_reuses_existing_uniforms(self):
        source = read("spatial/scintilla_engine.cpp")
        bind = function(source, "bindCrewProjectionUniforms")
        terminal = bind[:bind.index("std::map<std::string,int> slots")]
        self.assertIn("terminal.participants", terminal)
        self.assertIn("person.contributions", terminal)
        self.assertIn("terminal.partyEnabled?2:1", terminal)
        self.assertIn("terminal.completedHandoffs", terminal)
        self.assertIn("terminal.comparable", terminal)
        self.assertIn("total+=std::min<uint64_t>(1000-total,count)", terminal)
        self.assertIn("std::min<uint64_t>(999,terminal.completedHandoffs)", terminal)
        self.assertIn("std::min<uint64_t>(999,terminal.partyActivations)", terminal)
        self.assertNotIn("crew.records()", terminal)
        self.assertNotIn("crew.assignments()", terminal)
        self.assertNotIn("partyActivations()>0", terminal)
        projection = function(source, "bindProjectionUniforms")
        self.assertIn("frozenCrewResult()", projection)
        self.assertIn("Crew::matchesMissionResult(terminal,current.missionResult)", projection)
        self.assertIn("person.assignment.participantId", projection)
        self.assertIn("person.assignment.name", projection)
        for prohibited in ("readMissionFile", "drawText", "createImage", "lock("):
            self.assertNotIn(prohibited, bind)
            self.assertNotIn(prohibited, projection)
        shader = (ROOT / "data/shaders/utils/immersive_scintilla.frag").read_text(encoding="utf-8")
        uniforms = re.findall(r"uniform\s+\w+\s+(scintilla_crew_\w+)", shader)
        self.assertEqual(len(uniforms), 8)
        self.assertEqual(len(re.findall(r"uniform\s+sampler2D\s+scintilla_.*text", shader)), 1)
        hud = function(shader, "scintillaCrewResultHUD")
        self.assertIn("p.x<-.76||p.x>.76||p.y<-.08||p.y>.40", hud)
        self.assertIn("i<6", hud)
        self.assertIn("111.0:110.0", hud)
        self.assertIn("person.y>0.0", hud)

    def test_held_atlas_copy_and_shader_uv_share_bounded_nonoverlapping_regions(self):
        source = read("spatial/scintilla_crew_text_native.hpp")
        prepare = function(source, "prepareCrewTextAtlas")
        self.assertLess(prepare.index("isWorldHeld()"), prepare.index("createImage("))
        self.assertIn("core::dimension2du(2048,2048)", prepare)
        self.assertIn("missionResultAtlas->lock(video::ETLM_READ_ONLY)", prepare)
        self.assertIn("outcomes->copyTo(atlas,core::position2di(0,1024))", prepare)
        self.assertIn("1840+int((cell-128)/2)*64", prepare)
        self.assertIn("labels[128+i]=labels[i]", prepare)
        shader = (ROOT / "data/shaders/utils/immersive_scintilla.frag").read_text(encoding="utf-8")
        self.assertIn("1840.0+floor(index/2.0)*64.0", shader)
        self.assertIn("(1024.0+row*160.0)/2048.0", shader)
        # The top 128 cells, original 5 outcome rows and six wide names fit
        # without changing the already-qualified output HUD rectangles.
        regions = [(0, 16 * 64), (1024, 1024 + 5 * 160), (1840, 1840 + 3 * 64)]
        self.assertTrue(all(0 <= low < high <= 2048 for low, high in regions))
        self.assertTrue(all(left[1] <= right[0] for left, right in zip(regions, regions[1:])))
        self.assertIn("vec2(.235,.0146875)", shader)  # exact wide-name aspect 16:1


if __name__ == "__main__":
    unittest.main(verbosity=2)
