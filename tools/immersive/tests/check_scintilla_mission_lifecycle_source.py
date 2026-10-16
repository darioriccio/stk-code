#!/usr/bin/env python3
"""Supplemental source guards for the SC4 new-race routes; no build or launch.

These assertions check the cross-module call chain. They do not prove a native
reset, retained physical grant, or a second natural race at runtime.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
IR = ROOT / "src/modes/immersive_relay"
router = (IR / "remote/relay_command_router.cpp").read_text()
controller = (IR / "relay_session_controller.cpp").read_text()
world = (IR / "remote/relay_world_controller.cpp").read_text()
race = (IR / "relay_race_mode.cpp").read_text()
engine = (IR / "spatial/scintilla_engine.cpp").read_text()


def body(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    masked = re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                    lambda match: " " * len(match.group()), source, flags=re.S)
    depth = 1
    for index in range(opening + 1, len(masked)):
        depth += (masked[index] == "{") - (masked[index] == "}")
        if depth == 0:
            return source[opening + 1:index]
    raise AssertionError(signature)


checks = []
def check(name, condition):
    assert condition, name
    checks.append(name)


for path, method in (("operator/reset", "operatorReset"),
                     ("session/reset", "resetSession"),
                     ("operator/next-state", "operatorNextState")):
    route = body(router, 'if (route_path == "/immersive/v1/' + path + '")\n    {')
    check(path + " resets native world after accepted session operation",
          route.index("controller->" + method) < route.index("RelayWorldController::reset()"))
    check(path + " excludes synthetic accelerated progress from native reset",
          "!snapshot.m_test_accelerated_progress" in route)
    check(path + " retains the producer rather than closing input",
          "ScintillaEngine::invalidate" not in route and "ScintillaEngine::shutdown" not in route)

next_state = body(controller, "bool RelaySessionController::operatorNextState(")
check("Results to Ready performs the complete controller reset",
      all(text in next_state for text in ("before == ARCADE_STATE_RESULTS",
          'requested_state == "ready_for_players"', "lock.unlock()", "return operatorReset(reject_reason)")))
check("generic next-state cannot bypass authoritative readiness",
      all(text in next_state for text in ("ARCADE_STATE_COUNTDOWN", "ARCADE_STATE_RUNNING",
          "!m_presentation_ready", "!m_scintilla_start_permitted", "missionBlocksGameplayLocked()")))
for method in ("operatorReset", "resetSession"):
    reset = body(controller, "RelaySessionController::" + method + "(")
    check(method + " closes the old SC4 preparation immediately",
          "if (m_mission_required) m_scintilla_start_permitted = false" in reset)
check("native reset reaches the Relay race reset",
      "RaceManager::get()->rerunRace()" in body(world, "void RelayWorldController::reset()") and
      "ScintillaEngine::newWorld(true)" in body(race, "void RelayRaceMode::reset(bool restart)"))
new_world = body(engine, "void newWorld(")
check("new epoch clears core while retaining an eligible physical grant",
      "preserveSpatialGrant&&active&&defense.required()" in new_world and
      "mission.reset(true)" in new_world and "++raceEpoch" in new_world and
      "beginMissionRace(" in new_world)
check("next-state start destinations refresh the native admission gate",
      "const bool combat_start=next_starts_gameplay||" in router and
      "combat_start&&!ImmersiveRelay::ScintillaEngine::combatStartPermitted()" in router)

print(f"SC4 lifecycle: {len(checks)} source checks passed; native lifecycle remains unexecuted.")
