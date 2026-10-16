// Copyright (C) 2026 Immersive Relay contributors. SPDX-License-Identifier: GPL-3.0-or-later
#ifndef HEADER_SCINTILLA_ENGINE_HPP
#define HEADER_SCINTILLA_ENGINE_HPP
#ifdef ENABLE_IMMERSIVE_RELAY
#include "modes/immersive_relay/spatial/scintilla_input.hpp"
#include "modes/immersive_relay/scintilla_mission.hpp"
#include <array>
#include <functional>
namespace ImmersiveRelay {
class ImmersiveCameraRig;
class FullSphereCapture;
struct ImmersiveRenderConfig;
namespace ScintillaEngine {
ScintillaSpatial::Runtime& input();
int64_t nowUs();
bool enabled();
void setEnabled(bool value);
void invalidate(const std::string& reason);
void newWorld(bool preserveSpatialGrant=false);
void retireWorld();
void shutdown();
// SC4 authority is game-thread confined. The core clock counts admitted active
// steps; missionSimulationTickBase maps it onto the shared SC2/SC3 clock.
ScintillaMission::Runtime& missionRuntime();
bool missionRequired();
bool missionTerminal();
bool missionStartPermitted();
bool beginMissionStep(int ticks);
int64_t missionSimulationTickBase();
const std::string& missionStepBlockedReason();
void publishMissionSnapshot();
void setMissionPresentationHeld(bool held);
ScintillaMission::Result freezeMissionNaturalFinish(unsigned position,double time);
void terminateMission(const std::string& reason);
// Held-world preparation only; no filesystem work occurs during capture.
bool prepareMissionAnchors(std::string* reason);
const std::string& missionAnchorHash();
const std::string& missionAnchorStatus();
bool prepareMissionPresentationAssets(std::string* reason);
// Texture-cache/context teardown notification, before the driver drops it.
void releaseMissionPresentationAssets(const void* texture=nullptr);
// Full scene/content cache teardown only; individual dynamic/font textures do
// not change the qualified native geometry or require another preflight.
void invalidateMissionContent();
const std::string& producerUUID();
// SC5 policy and its held-world coordination, on the same authenticated queue.
bool crewRequired();
bool crewStartPermitted();
std::string crewStateJSON();
ScintillaSpatial::Reply crewState();
ScintillaSpatial::Reply crewCommand(const std::string& operation,const std::string& body);
std::string missionStateJSON();
ScintillaSpatial::Reply setMissionIntent(const std::string&);
ScintillaSpatial::Reply prepareMission(const std::string&);
ScintillaSpatial::Reply completeMissionIntro(const std::string&);
// Explicit diagnostic recording: held start, no consumer grant or SC4 policy.
// Results describe native rendered geometry, never optics or hardware.
bool missionMeasurementActive();
ScintillaSpatial::Reply missionInputMaintenanceCommand(const std::string& operation,const std::string& body);
ScintillaSpatial::Reply missionMeasurementCommand(const std::string& operation,const std::string& body);
std::string missionMeasurementStateJSON();
ScintillaSpatial::Reply missionMeasurementTrace(const std::string& offset);
void observeMissionMeasurementPhysics();
void observeMissionMeasurementNaturalFinish();
// Reuses the projection program/draw at native resolution with every write off.
void measureMissionProjection(unsigned int program,const std::function<void()>& draw);
// Combat commits only from RelayRaceMode; input/render callbacks collect evidence.
bool combatPolicyActive();
bool combatRequired();
bool combatStartPermitted();
void updateCombat(int ticks, bool gameplayActive);
void refreshCombatInput();
ScintillaSpatial::Reply prepareCombat(const std::string& body);
ScintillaSpatial::Reply setCombatIntent(const std::string& body);
std::string combatStateJSON();
ScintillaSpatial::Reply setDefenseIntent(const std::string&);
ScintillaSpatial::Reply prepareDefense(const std::string&);
std::string defenseStateJSON();
// Race tuning from $STK_RELAY_SCINTILLA_TUNING, reloaded with every new race.
std::string tuningStateJSON();
ScintillaSpatial::Reply reloadTuning(const std::string& body);
ScintillaSpatial::Reply rebindSpatialParticipants(const std::string&);
void beforeDefensePhysics();
void afterDefensePhysics();
// Published producer evidence exists before a consumer grant, on the same game/GL thread.
std::string historyJSON();
void seedPreparedHistory();
// Every call below is confined to the existing GL/game thread.
void beforeCapture(const ImmersiveCameraRig& rig, const ImmersiveRenderConfig& config);
void retainDepth(const FullSphereCapture& capture);
void finishFrame(bool published);
void bindProjectionUniforms(unsigned int program);
void finishProjectionUniforms();
}
}
#endif
#endif
