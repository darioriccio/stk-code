#!/usr/bin/env python3
"""SC5 wire source contracts; not execution of HTTP, C++, or the game.

The native regression cases cover preview state and physical continuity using
the production cores. These checks pin their admission/transaction wiring and
the exact receipt ordering, pending authorized native integration execution.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
IR = ROOT / "src/modes/immersive_relay"


def source(path):
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", path.read_text(encoding="utf-8"), flags=re.S)


def body(text, signature):
    start = text.index("{", text.index(signature))
    depth, end = 1, start + 1
    while depth:
        if text[end] == "{":
            depth += 1
        elif text[end] == "}":
            depth -= 1
        end += 1
    return text[start + 1:end - 1]


class CrewWireSourceContracts(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.wire = source(IR / "spatial/scintilla_crew_wire.hpp")
        cls.command = body(cls.wire, "Reply crewCommand(")
        cls.intent = body(cls.command, 'if(operation=="intent")')

    def test_exact_receipt_precedes_live_identity_and_gate_checks(self):
        command = self.command
        duplicate = command.index("Receipts::Duplicate")
        self.assertLess(duplicate, command.index("refreshRoster()"))
        self.assertLess(duplicate, command.index("request.expected!=crewIdentity()"))
        self.assertLess(duplicate, command.index("controller->isWorldHeld()"))
        self.assertIn("return Reply(reply->httpStatus,reply->body)", command)
        self.assertIn("crewRequests.commit(request,r.json,r.status)", command)
        self.assertIn("crewRequests.commit(request,result.json,result.status)", command)

    def test_selection_requires_held_tick_zero_and_unconfigured_downstream(self):
        self.assertIn("!controller->isWorldHeld()", self.command)
        self.assertIn("world->getTimeTicks()!=0", self.intent)
        self.assertIn("!request.required||request.required!=crew.required()||request.profileId!=crew.profileId()", self.intent)
        barrier = "changesSelection&&(combat.configured()||defense.configured()||mission.configured())"
        self.assertIn(barrier, self.intent)
        self.assertLess(self.intent.index(barrier), self.intent.index("auto nextCrew=crew"))
        self.assertLess(self.intent.index(barrier), self.intent.index("controller->setCrewStartState("))

    def test_intent_is_staged_before_any_live_gate_or_core_write(self):
        first_write = self.intent.index("controller->setCrewStartState(")
        for validation in ("nextCrew.setIntent(", "nextMission.setRequired(",
                           "nextCombat.setRequired(", "nextDefense.setRequired(",
                           "native.size()<2||native.size()>6", "nextCrew.bindRace("):
            self.assertLess(self.intent.index(validation), first_write)
        for assignment in ("crew=std::move(nextCrew)", "mission=std::move(nextMission)",
                           "combat=std::move(nextCombat)", "defense=std::move(nextDefense)"):
            self.assertGreater(self.intent.index(assignment), first_write)
        for forbidden in ("crew.setIntent(", "mission.setRequired(", "combat.setRequired(", "defense.setRequired("):
            self.assertNotIn(forbidden, self.intent)

    def test_preview_discard_clears_configured_requirement_without_rewinding_revision(self):
        core = source(IR / "scintilla_crew.cpp")
        intent = body(core, "bool Runtime::setIntent(")
        self.assertIn("configured()&&(m_input!=0||m_turn!=0)&&(!value||selected!=m_profileId)", intent)
        self.assertLess(intent.index("m_intentRevision=="), intent.index("if(!value)reset(false)"))
        self.assertLess(intent.index("if(!value)reset(false)"), intent.index("++m_intentRevision"))
        reset = body(core, "void Runtime::reset(")
        self.assertIn("const uint64_t revision=m_intentRevision", reset)
        self.assertIn("*this=Runtime()", reset)
        self.assertIn("m_intentRevision=revision", reset)
        self.assertIn("return m_required||configured()", source(IR / "scintilla_crew.hpp"))
        identity = body(self.wire, "CrewProtocol::Identity crewIdentity(")
        self.assertIn('if(!crew.required())', identity)
        self.assertIn('mission.required()?"front_rear_cargo_v2":"front_rear_readonly_v1"', identity)

    def test_possession_baseline_is_captured_at_epoch_commit_and_never_on_retry(self):
        consumer = source(IR / "spatial/scintilla_input.cpp")
        self.assertEqual(consumer.count("m->captureBindingContinuity()"), 3)
        capture = body(consumer, "void captureBindingContinuity(")
        self.assertIn("source.bindingLossRevision=source.trackingLossRevision", capture)
        self.assertIn("source.bindingContinuity=source.identity.continuity", capture)
        rebind = body(consumer, "Reply Runtime::rebindParticipants(")
        self.assertLess(rebind.index("return same?reply()"), rebind.index("captureBindingContinuity()"))
        self.assertLess(rebind.index("++m->inputEpoch"), rebind.index("captureBindingContinuity()"))
        bindings = body(consumer, "std::vector<BindingState> Runtime::bindingStates(")
        for check in ("s.trackingLossRevision!=UINT64_MAX", "s.trackingLossRevision==s.bindingLossRevision",
                      "s.identity.continuity==s.bindingContinuity"):
            self.assertIn(check, bindings)

    def test_lost_then_recovered_gesture_is_rejected_before_binding_commit(self):
        confirm = body(self.command, 'if(operation=="bindings-confirm")')
        self.assertLess(confirm.index("!source->continuityIntact"), confirm.index("crewVerifiedBindings=std::move(verified)"))
        self.assertLess(confirm.index("!source->continuityIntact"), confirm.index('proof.verification=="retained"'))
        self.assertIn("!b.continuityIntact", body(self.wire, "bool crewBindingsLive("))


if __name__ == "__main__":
    unittest.main()
