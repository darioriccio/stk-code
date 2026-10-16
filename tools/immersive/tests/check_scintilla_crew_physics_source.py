#!/usr/bin/env python3
"""SC5 source contracts only; no claim of native controls/Bullet execution.

The numerical target covers the pure algorithm. Brake-before/during, native
PlayerController analog event ordering and later vanilla pads still need the
real STK game fixture when builds/runtime checks are authorized.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[3]
IR = ROOT / "src/modes/immersive_relay"


def source(path):
    text = path.read_text(encoding="utf-8")
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)


def body(text, signature):
    start = text.index("{", text.index(signature))
    depth = 1
    end = start + 1
    while depth:
        if text[end] == "{":
            depth += 1
        elif text[end] == "}":
            depth -= 1
        end += 1
    return text[start + 1:end - 1]


class CrewPhysicsSourceContracts(unittest.TestCase):
    def test_party_commit_preflight_precedes_only_two_native_writes(self):
        text = source(IR / "scintilla_crew_physics.cpp")
        apply = body(text, "bool apply(")
        self.assertLess(apply.index("durations("), apply.index("read(kart,&flight)"))
        self.assertLess(apply.index("if(!status.eligible)return false"), apply.index("increaseMaxSpeed("))
        self.assertEqual(apply.count("increaseMaxSpeed("), 1)
        self.assertEqual(apply.count("setLinearVelocity("), 1)
        self.assertLess(apply.index("increaseMaxSpeed("), apply.index("setLinearVelocity("))
        self.assertLess(apply.index("setLinearVelocity("), apply.index("beginPartyBrakeLease("))
        self.assertIn("MS_INCREASE_ZIPPER,1.0f,0.0f,burst,fade", apply)
        for forbidden in ("instantSpeedIncrease", "applyBoost", "adjustSpeed", "setMinSpeed",
                          "resetMinSpeed", "setAngularVelocity", "update(0)"):
            self.assertNotIn(forbidden, text)

    def test_native_cap_formula_and_fade_activity_are_still_the_verified_owner(self):
        native = source(ROOT / "src/karts/max_speed.cpp")
        update = body(native, "void MaxSpeed::update(")
        self.assertIn("m_current_max_speed = m_kart->getKartProperties()->getEngineMaxSpeed()", update)
        self.assertIn("m_current_max_speed += speedup.getSpeedIncrease()", update)
        self.assertIn("m_current_max_speed *= slowdown_factor", update)
        self.assertIn("slowdown_factor = std::min(slowdown_factor", update)
        self.assertIn("m_duration > -m_fade_out_time", source(ROOT / "src/karts/max_speed.hpp"))
        preflight = body(source(IR / "scintilla_crew_physics.cpp"), "Status read(")
        self.assertIn("category<MaxSpeed::MS_INCREASE_MAX", preflight)
        self.assertIn("isSpeedIncreaseActive(category)", preflight)
        for gate in ("partyDriverVeto()", "getBody()", "getVehicle()", "getKartProperties()",
                     "isEliminated()", "hasFinishedRace()", "getKartAnimation()", "isInvulnerable()",
                     "isOnGround()", "getSpeed()<=0", "material->isZipper()", "floor>0",
                     "tangent.dot(forward)<=0", "ScintillaCrewBoostMath::prepare("):
            self.assertIn(gate, preflight)

    def test_raw_brake_is_observed_before_held_reset_and_dry_run_is_pure(self):
        controller = source(IR / "relay_player_controller.cpp")
        action = body(controller, "bool RelayPlayerController::action(")
        dry = body(action, "if(dry_run)")
        self.assertIn("PlayerController::action(action,value,true)", dry)
        self.assertIn("action==PA_PAUSE_RACE", dry)
        self.assertNotIn("LocalPlayerController::action", dry)
        self.assertNotIn("m_party_raw_brake=", dry)
        self.assertNotIn("resetInputState()", dry)
        raw = action.index("if(action==PA_BRAKE)m_party_raw_brake=value")
        self.assertLess(action.index("if(dry_run)"), raw)
        self.assertLess(raw, action.index("resetInputState()"))
        self.assertLess(action.index("LocalPlayerController::action"), action.index("enforcePartyBrake()"))
        header = source(IR / "relay_player_controller.hpp")
        self.assertIn("m_party_raw_brake>32768/2", header)
        native_brake = source(ROOT / "src/karts/controller/player_controller.cpp")
        self.assertIn("if(value > 32768/2)", native_brake)

    def test_brake_priority_has_local_party_lease_and_freezes_while_held(self):
        text = source(IR / "relay_player_controller.cpp")
        enforce = body(text, "void RelayPlayerController::enforcePartyBrake(")
        self.assertIn("if(m_party_brake_ticks<=0||!partyDriverVeto())return", enforce)
        for control in ("setAccel(0.0f)", "setBrake(true)", "setNitro(false)"):
            self.assertIn(control, enforce)
        update = body(text, "void RelayPlayerController::update(")
        live = body(update, "if (!RelaySessionController::get()->isHumanInputLocked())")
        self.assertIn("enforcePartyBrake()", live)
        self.assertIn("m_party_brake_ticks-=std::min(m_party_brake_ticks,ticks)", live)
        self.assertEqual(update.count("m_party_brake_ticks-="), 1)
        lease = body(text, "void RelayPlayerController::beginPartyBrakeLease(")
        self.assertIn("ticks>0&&ticks<=750", lease)
        self.assertNotIn("increaseMaxSpeed", text)
        self.assertNotIn("setLinearVelocity", text)
        reset = body(text, "void RelayPlayerController::reset(")
        self.assertIn("m_party_raw_brake=0", reset)
        self.assertIn("m_party_brake_ticks=0", reset)

    def test_zone_admission_requires_native_evidence_and_capture_bytes_before_report(self):
        text = source(IR / "spatial/scintilla_crew_zones_native.hpp")
        prepare = body(text, "bool prepareCrewPartyZones(")
        self.assertLess(prepare.index("CrewZones::validateNative("), prepare.index("CrewZones::nativeEvidence("))
        self.assertLess(prepare.index("CrewZones::nativeEvidence("), prepare.index("for(const auto& hash:proofs)"))
        self.assertLess(prepare.index("missionSHA256(capture)!=hash"), prepare.index("CrewZones::qualify("))
        self.assertLess(prepare.index("CrewZones::qualify("), prepare.index("crewZoneQualified=true"))
        self.assertIn('qualification_v2/party_zone_proofs/', prepare)
        self.assertIn('1024*1024,&capture', prepare)


if __name__ == "__main__":
    unittest.main()
