#!/usr/bin/env python3
"""Read-only SC4 applied-impact/presentation authority contracts; no compiler/GPU."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[3]
e=(ROOT/"src/modes/immersive_relay/spatial/scintilla_engine.cpp").read_text()
h=(ROOT/"src/modes/immersive_relay/spatial/scintilla_input.hpp").read_text()
i=(ROOT/"src/modes/immersive_relay/spatial/scintilla_input.cpp").read_text()
def body(signature):
 start=e.index(signature);start=e.index("{",start)
 masked=re.sub(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"',lambda m:" "*len(m.group()),e,flags=re.S)
 depth=1
 for n in range(start+1,len(masked)):
  depth+=(masked[n]=="{")-(masked[n]=="}")
  if not depth:return e[start+1:n]
 raise AssertionError(signature)
checks=[]
def check(name,ok):
 assert ok,name
 checks.append(name)
def ordered(text,*needles):
 positions=[text.index(n) for n in needles];return positions==sorted(positions)
defense=body("void updateDefense(bool admitted)")
check("applied impact precedes immutable event consumption",ordered(defense,"ScintillaDefensePhysics::applyImpact(human)","defense.confirmImpact(applied)","defense.impactEvent()","mission.consumeImpact(event)","observeNewTether(now)"))
check("rejected/requested impacts never detach", "&&applied&&mission.required()&&mission.active()&&!missionTerminal()" in defense)
update=body("void updateCombat(int ticks,bool gameplayActive)")
check("presentation service remains reachable before held gameplay admission",ordered(update,"serviceMissionPresentation(nowUs())","if(!combat.configured())return","const bool admitted="))
check("hold raised during service closes same-step commits","gameplayActive&&gameplay.m_active&&active" in update)
service=body("void serviceMissionPresentation(int64_t now)")
check("deadline starts only on actual module presentation",ordered(service,"missionPresentationVisibility(2,true", "mission.confirmTetherPresented("))
check("monotonic technical watchdog is distinct from core window","now-tetherCreatedUs>=250000" in service and "setMissionPresentationHeld(true)" in service and "expireCapsule" not in service and "consumeImpact" not in service)
check("technical hold is cleared only by its owner and with no other mission fault","tetherPresentationHold&&missionStepFailure.empty()" in service)
query=body("Visibility missionPresentationVisibility(")
check("query binds immutable phase/instance/human to actual presentation",all(x in query for x in ["consumer.presentedMissionFrames(now)","frame.missionHumanInstance!=mission.humanInstance()","target.instance==snapshot.instance","target.mission.phaseRevision==snapshot.phaseRevision","target.mission.phase==snapshot.phase","pinFrame(*frame)","defensePointVisibility(*q.frame,center,.5,false)"]))
check("bounded service queries use independent leases","std::array<MissionPresentationQuery,3>" in e and "(uint64_t(1)<<63)+2+query" in query)
observe=body("void observeNewTether(int64_t now)")
check("reprepare or polling never restarts the watchdog","snapshot.instance==tetherPlacementInstance" in observe and "tetherCreatedUs=now" in observe)
capture=body("void beforeCapture(")
check("capture records geometry but cannot confirm transactions","missionHuman->getSmoothedTrans()" in capture and "current.missionTetherCandidates" in capture and "current.missionCableStart" in capture and "confirmTetherPresented" not in capture and "consumeImpact" not in capture)
check("only real current frame presentation can recover while inactive","Runtime::presentedMissionFrames" in i and "frame.missionRevision==m->missionRevision" in i and "missionHumanInstance" in h)
print(f"SC4 presentation: {len(checks)} source contracts passed; no build/GPU/runtime executed.")
