#!/usr/bin/env python3
"""Read-only SC4 finish integration source contracts; never compiles or runs STK.

These checks protect the ordering of native hooks and the mutex boundary. They
are supplemental source evidence, not a substitute for the executable M21-M25
and native race acceptance when builds/runs are authorized.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[3]
IR = ROOT / "src/modes/immersive_relay"
engine = (IR / "spatial/scintilla_engine.cpp").read_text()
race = (IR / "relay_race_mode.cpp").read_text()
controller = (IR / "relay_session_controller.cpp").read_text()
router = (IR / "remote/relay_command_router.cpp").read_text()
publisher = (IR / "remote/relay_state_publisher.cpp").read_text()
serializer = (IR / "scintilla_mission_result_json.hpp").read_text()


def body(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    # Preserve offsets while hiding braces in comments/character/string literals.
    masked = re.sub(
        r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
        lambda match: " " * len(match.group()), source, flags=re.S,
    )
    depth = 1
    for index in range(opening + 1, len(masked)):
        depth += (masked[index] == "{") - (masked[index] == "}")
        if depth == 0:
            return source[opening + 1:index]
    raise AssertionError(f"Unclosed function: {signature}")


def ordered(source, *needles):
    positions = [source.index(needle) for needle in needles]
    assert positions == sorted(positions), needles


checks = []


def check(name, predicate):
    assert predicate, name
    checks.append(name)


update = body(race, "void RelayRaceMode::update(int ticks)")
ordered(update, "refreshCombatInput()", "missionRequired() && ticks!=1",
        "isWorldHeld()", "StandardRace::update(ticks)")
zero_branch = update[update.index("missionRequired() && ticks!=1"):update.index("AbstractKart* player_kart")]
check("zero/unsupported step bypasses all native physics",
      "return;" in zero_branch and "updateCombat(0,false)" in zero_branch)
native_pos = update.index("StandardRace::update(ticks)")
check("mission begin-step precedes native update and post-physics combat",
      update.rindex("beginMissionStep(ticks)", 0, native_pos) < native_pos <
      update.index("updateCombat(ticks,"))

lap = body(race, "void RelayRaceMode::newLap(unsigned int kart_index)")
ordered(lap, "!previous->hasFinishedRace()", "StandardRace::newLap(kart_index)",
        "freezeMissionNaturalFinish(", "notifyLap(")
check("natural finish proves official transition and forwards native facts",
      all(text in lap for text in ("!previous->isEliminated()", "!previous->isGhostKart()",
          "!isNetworkWorld()", "!hasRaceEndedEarly()", "player_kart->hasFinishedRace()",
          "!state.m_test_accelerated_progress", "RaceManager::get()->getNumLaps()",
          "player_kart->getPosition()", "player_kart->getFinishTime()")))
early = body(race, "void RelayRaceMode::endRaceEarly()")
ordered(early, 'terminateMission("early_finish")', "StandardRace::endRaceEarly()")
check("estimated early finish cannot enter natural mission path", True)

begin = body(engine, "bool beginMissionStep(int ticks)")
ordered(begin, "ticks<0 || ticks>1", "mission.advanceTo(mission.tick()+1,true)", "++simulationTick")
check("one bounded advance; zero and inactive do not advance mission time",
      "ticks==1" in begin and "mission.advanceTo(mission.tick(),false)" in begin and
      "missionClockBase=simulationTick-mission.tick()" in begin)
combat = body(engine, "void updateCombat(int ticks,bool gameplayActive)")
check("shared clock has no second SC4 increment or unopened-step credits",
      "ticks>0 && !mission.required()" in combat and
      "gameplayActive&&missionStepOpen&&!missionTerminal()" in combat)
capture = body(engine, "void beforeCapture(")
refresh = body(engine, "void refreshCombatInput()")
check("terminal admission remains closed in input and render callbacks",
      "!missionTerminal()" in capture and "terminalMission" in refresh and
      "!terminalMission&&active" in refresh and "!missionTerminal()" in combat)
check("readiness helper and controller aggregate use the same full mission predicate",
      "missionStartPermitted()" in refresh and
      "missionStartPermitted()" in body(engine, "bool combatStartPermitted()") and
      all(text in body(engine, "bool missionStartPermitted()") for text in (
          "mission.crew()==lastRoster", "mission.profile().fps==stk_config->getPhysicsFPS()",
          "missionStepFailure.empty()", "m_mission_presentation_held")))
check("suspension retains physical grant",
      "consumer.tick(nowUs(),false" in body(engine, "void closeMissionAdmission()") and
      "consumer.reset" not in body(engine, "void closeMissionAdmission()"))

notify = body(controller, "void RelaySessionController::notifyLap(")
ordered(notify, "if(m_mission_required)", "m_mission_result=*mission_result", "publishReportLocked(")
report = body(controller, "void RelaySessionController::publishReportLocked(")
check("report publishes a single immutable typed result",
      "!m_mission_result.valid" in report and "m_last_report.m_mission.valid" in report and
      "report.m_mission = m_mission_result" in body(controller, "RelaySessionReport RelaySessionController::buildReportLocked("))
check("controller never invokes engine while holding its mutex",
      "ScintillaEngine::" not in controller)
progress = body(controller, "void RelaySessionController::updateProgressLocked(")
check("distance/progress cannot bypass native SC4 finish",
      "!m_mission_required || m_active_config.m_test_accelerated_progress" in progress and
      'finalizeMissionArtificialLocked("test_accelerated_progress")' in progress)
for name, reason in (("operatorAbort", "operator_abort"),
                     ("operatorForceFinish", "operator_force_finish"),
                     ("stopSession", "reason")):
    source = body(controller, f"RelaySessionController::{name}(")
    ordered(source, "finalizeMissionArtificialLocked(", "publishReportLocked(")
    check(f"{name}: artificial snapshot precedes report", True)
check("router latches only accepted artificial operations",
      router.index('ScintillaEngine::terminateMission("operator_abort")') > router.index("!controller->operatorAbort(") and
      router.index('ScintillaEngine::terminateMission("operator_force_finish")') > router.index("!controller->operatorForceFinish("))
terminate = body(engine, "void terminateMission(")
check("artificial core and report use identical reason/sequence",
      "mission.abort(result.terminationReason,result.finishSequence)" in terminate and
      "missionTerminalRecord=result" in terminate)
check("API and persisted report share exact serializer",
      "ScintillaMission::writeResultJSON(file,m_last_report.m_mission)" in controller and
      "ScintillaMission::writeResultJSON(out,report.m_mission)" in publisher and
      'else out << "null"' in serializer)
retire = body(engine, "void retireWorld()")
check("world retirement retains result; only new race clears it",
      "newWorld" not in retire and "beginMissionRace" not in retire and
      "missionTerminalRecord=ScintillaMission::Result()" in body(engine, "void newWorld("))
hold = body(controller, "bool RelaySessionController::missionBlocksGameplayLocked() const")
check("mission technical hold is separate from video hold",
      "m_mission_presentation_held" in hold and "m_mission_result.valid" in hold and
      "m_presentation_ready" not in body(controller, "void RelaySessionController::setMissionPresentationHeld("))

print(f"SC4 finish integration: {len(checks)} source checks passed; no build/runtime executed.")
