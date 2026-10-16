// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_DEFENSE_PHYSICS_HPP
#define HEADER_SCINTILLA_DEFENSE_PHYSICS_HPP
#ifdef ENABLE_IMMERSIVE_RELAY
#include "modes/immersive_relay/scintilla_defense.hpp"
class AbstractKart;
namespace ScintillaDefensePhysics {
struct Trace {bool available=false,valid=false,hit=false,human=false;ScintillaDefense::Vec point,humanPosition,humanForward,humanUp;};
// Called around the ACTUAL Bullet step on its owning game thread. No gameplay commits.
void before(AbstractKart* human,AbstractKart* source,const ScintillaDefense::Vec& pulse,const ScintillaDefense::Vec& velocity,int fps,bool relaxedHuman=false);
void after();
Trace take();
void clear();
bool eligibleHuman(AbstractKart*);
// eligibleHuman without the transient conditions (speed boost or airborne).
bool eligibleHumanCore(AbstractKart*);
bool humanTransient(AbstractKart*);
bool cameraLineClear(const ScintillaDefense::Vec&,AbstractKart* human,AbstractKart* source);
bool opportunity(AbstractKart* human,AbstractKart* source,ScintillaDefense::Sector retained,ScintillaDefense::Candidate*,ScintillaDefense::Ballistic*,const ScintillaDefense::Profile& profile,bool requireLaunchSolution=true,bool relaxedHuman=false);
bool applyImpact(AbstractKart*,double speedRetained=.85,bool relaxedHuman=false);
bool applyBoost(AbstractKart*,int fps);
}
#endif
#endif
